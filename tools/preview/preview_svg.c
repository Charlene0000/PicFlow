/*
 * preview_svg.c
 * PicFlow Core —— 瀑布流布局可视化工具
 *
 * 用途：
 *   在没有 macOS / Xcode 的机器上（例如 Windows），
 *   用同一个 C 布局引擎渲染出真实布局结果，输出 SVG。
 *   这样「算法是否正确」可以用眼睛直接验证，而不是只看测试数字。
 *
 *   这是关键的一点：**测试断言和可视化用的是同一份实现**，
 *   不存在「测试里对、实际跑起来不对」的问题。
 *
 * 用法：
 *   gcc -std=c99 -I core preview_svg.c core/picflow_layout.c ... -o preview_svg -lm
 *   ./preview_svg 输出.svg [列数] [内容宽] [项目数]
 */

#include "picflow_layout.h"
#include "picflow_catalog.h"
#include "picflow_palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITEMS 64

/* 生成 SVG 字符串转义（标题里有中文等字符） */
static void write_escaped(FILE *f, const char *text) {
    if (text == NULL) {
        return;
    }
    for (const char *p = text; *p; p++) {
        if (*p == '&')       fputs("&amp;", f);
        else if (*p == '<')  fputs("&lt;", f);
        else if (*p == '>')  fputs("&gt;", f);
        else if (*p == '"')  fputs("&quot;", f);
        else if ((unsigned char)*p < 0x20) fputc(' ', f);
        else fputc(*p, f);
    }
}

static void rgb_to_hex(const PFColor *c, char *out) {
    int r = (int)(c->r + 0.5), g = (int)(c->g + 0.5), b = (int)(c->b + 0.5);
    if (r < 0) { r = 0; }
    if (r > 255) { r = 255; }
    if (g < 0) { g = 0; }
    if (g > 255) { g = 255; }
    if (b < 0) { b = 0; }
    if (b > 255) { b = 255; }
    sprintf(out, "#%02X%02X%02X", r, g, b);
}

int main(int argc, char **argv) {
    const char *output_path = (argc > 1) ? argv[1] : "picflow_layout.svg";

    /* 可选：命令行指定列数与宽度，便于对比 iPhone / iPad */
    int columns = (argc > 2) ? atoi(argv[2]) : 2;
    double width = (argc > 3) ? atof(argv[3]) : 375.0;
    int item_count = (argc > 4) ? atoi(argv[4]) : 24;

    if (item_count > MAX_ITEMS) {
        item_count = MAX_ITEMS;
    }

    PFWaterfallConfig config = pf_waterfall_config_default();
    config.column_count = columns;

    PFCatalogEntry entries[MAX_ITEMS];
    double ratios[MAX_ITEMS];
    pf_catalog_build(entries, MAX_ITEMS);
    for (int i = 0; i < item_count; i++) {
        ratios[i] = entries[i].aspect_ratio;
    }

    PFWaterfallFrame frames[MAX_ITEMS];
    double content_height = 0.0;
    int rc = pf_waterfall_layout_compute_by_ratio(&config, item_count, ratios,
                                                  width, frames, &content_height);
    if (rc != 0) {
        fprintf(stderr, "布局计算失败: %d\n", rc);
        return 1;
    }

    /* 布局自检：不通过就不出图，避免给出错误的结果 */
    int bad = pf_waterfall_validate_frames(frames, item_count, width, content_height);
    if (bad != 0) {
        fprintf(stderr, "布局自检失败：第 %d 个 item 存在重叠或越界\n", bad);
        return 2;
    }

    FILE *f = fopen(output_path, "w");
    if (f == NULL) {
        fprintf(stderr, "无法写入 %s\n", output_path);
        return 1;
    }

    const double margin = 16.0;
    double svg_height = content_height + margin * 2.0;

    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.0f\" height=\"%.0f\" "
               "viewBox=\"0 0 %.0f %.0f\" font-family=\"PingFang SC, Microsoft YaHei, sans-serif\">\n",
            width, svg_height, width, svg_height);
    fprintf(f, "  <rect width=\"%.0f\" height=\"%.0f\" fill=\"#F5F5F7\"/>\n", width, svg_height);

    fprintf(f, "  <text x=\"%.0f\" y=\"20\" font-size=\"11\" fill=\"#8A8A8E\">"
               "PicFlow 瀑布流布局 · %d 列 · 内容宽 %.0f · %d 项 · 内容高 %.0f</text>\n",
            margin, columns, width, item_count, content_height);

    for (int i = 0; i < item_count; i++) {
        PFArtworkSpec spec;
        pf_palette_generate(entries[i].seed, &spec);

        char bg[8], primary[8], accent[8], line[8], secondary[8];
        rgb_to_hex(&spec.background, bg);
        rgb_to_hex(&spec.primary, primary);
        rgb_to_hex(&spec.accent, accent);
        rgb_to_hex(&spec.line, line);
        rgb_to_hex(&spec.secondary, secondary);

        const double x = frames[i].x + margin;
        const double y = frames[i].y + margin;
        const double w = frames[i].width;
        const double h = frames[i].height;

        /* 卡片背景 + 圆角 */
        fprintf(f, "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" rx=\"8\" "
                   "fill=\"%s\" stroke=\"%s\" stroke-width=\"0.6\"/>\n",
                x, y, w, h, bg, line);

        /* 图案裁剪区：保证图案元素不会溢出卡片边界 */
        fprintf(f, "  <clipPath id=\"clip%d\"><rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" "
                   "height=\"%.1f\" rx=\"8\"/></clipPath>\n", i, x, y, w, h);
        fprintf(f, "  <g clip-path=\"url(#clip%d)\">\n", i);

        /* 图案：按 pattern 类型画不同的几何元素（模拟 iOS 端 Core Graphics 绘制） */
        if (spec.pattern == PF_PATTERN_CIRCLES) {
            int circles = spec.motif_count;
            double radius = (w < h ? w : h) / (double)(circles > 2 ? circles / 2 + 1 : 2);
            if (radius < 4) radius = 4;
            for (int k = 0; k < circles; k++) {
                double cx = x + w * 0.5 + (k % 3 - 1) * radius * 0.55;
                double cy = y + h * (0.3 + 0.2 * (k % 4));
                fprintf(f, "  <circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.1f\" fill=\"none\" "
                           "stroke=\"%s\" stroke-width=\"1.1\" opacity=\"0.75\"/>\n",
                        cx, cy, radius, primary);
            }
        } else if (spec.pattern == PF_PATTERN_DIAGONAL) {
            int lines = spec.motif_count;
            for (int k = 0; k < lines; k++) {
                double offset = k * (w / (double)lines);
                fprintf(f, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" "
                           "stroke=\"%s\" stroke-width=\"1.2\" opacity=\"0.7\"/>\n",
                        x + offset, y + h, x + offset + h * 0.45, y, primary);
            }
        } else if (spec.pattern == PF_PATTERN_WAVES) {
            int waves = spec.motif_count / 2 + 2;
            for (int k = 0; k < waves; k++) {
                double base_y = y + h * (0.15 + 0.7 * k / (double)waves);
                fprintf(f, "  <path d=\"M %.1f %.1f Q %.1f %.1f %.1f %.1f\" fill=\"none\" "
                           "stroke=\"%s\" stroke-width=\"1.3\" opacity=\"0.7\"/>\n",
                        x, base_y, x + w * 0.5, base_y - h * 0.12, x + w, base_y, primary);
            }
        } else if (spec.pattern == PF_PATTERN_TRIANGLES) {
            int tris = spec.motif_count;
            for (int k = 0; k < tris; k++) {
                double ty = y + h * (0.2 + 0.65 * k / (double)tris);
                double s = w * 0.16;
                fprintf(f, "  <polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f\" "
                           "fill=\"%s\" opacity=\"0.42\"/>\n",
                        x + w * 0.5, ty, x + w * 0.5 + s, ty + s, x + w * 0.5 - s, ty + s, accent);
            }
        } else { /* PF_PATTERN_GRID */
            int cells = spec.motif_count;
            double step = w / 3.0;
            for (int k = 0; k < cells; k++) {
                double gx = x + (k % 3) * step + step * 0.15;
                double gy = y + (k / 3) * step * 0.9 + step * 0.15;
                if (gy > y + h - 6) break;
                fprintf(f, "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" "
                           "rx=\"2\" fill=\"%s\" opacity=\"0.45\"/>\n",
                        gx, gy, step * 0.7, step * 0.6, k % 2 ? primary : secondary);
            }
        }

        /* 关闭裁剪组 */
        fprintf(f, "  </g>\n");

        /* 标题条 */
        fprintf(f, "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"9\" fill=\"#3A3A3C\">",
                x + 5, y + h - 5);
        write_escaped(f, entries[i].title);
        fprintf(f, "</text>\n");
    }

    fprintf(f, "</svg>\n");
    fclose(f);

    printf("已生成 %s\n", output_path);
    printf("  列数=%d 内容宽=%.0f 项目数=%d 内容高=%.0f\n",
           columns, width, item_count, content_height);
    printf("  布局自检：通过（无重叠、无越界）\n");
    return 0;
}
