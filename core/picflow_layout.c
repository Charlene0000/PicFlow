/*
 * picflow_layout.c
 * PicFlow Core —— 瀑布流布局算法实现
 *
 * 算法：最短列优先（Shortest Column First / SCF）
 *   1. 维护每一列当前的底部 Y 值（column_heights）
 *   2. 依次处理每个 item，选择当前底部 Y 值最小的列放入
 *   3. 放入后更新该列的底部 Y 值
 *
 * 复杂度：O(n * column_count)，n 为 item 数、column_count 通常为 2~3。
 * 实际瓶颈不在这里，而在于「每次滚动都重算」—— 因此 iOS 端
 * 把结果缓存进 attributesCache，滚动时只做矩形相交裁剪。
 */

#include "picflow_layout.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PF_EPSILON 1e-6

PFWaterfallConfig pf_waterfall_config_default(void) {
    PFWaterfallConfig config;
    config.column_count      = 2;
    config.line_spacing      = 8.0;
    config.interitem_spacing = 8.0;
    config.inset_top         = 8.0;
    config.inset_left        = 8.0;
    config.inset_bottom      = 8.0;
    config.inset_right       = 8.0;
    config.header_height     = 0.0;
    return config;
}

int pf_waterfall_config_validate(PFWaterfallConfig *config) {
    if (config == NULL) {
        return -1;
    }
    if (config->column_count < 1) {
        config->column_count = 1;
    }
    if (config->line_spacing < 0.0) {
        config->line_spacing = 0.0;
    }
    if (config->interitem_spacing < 0.0) {
        config->interitem_spacing = 0.0;
    }
    if (config->inset_top < 0.0)    { config->inset_top = 0.0; }
    if (config->inset_left < 0.0)   { config->inset_left = 0.0; }
    if (config->inset_bottom < 0.0) { config->inset_bottom = 0.0; }
    if (config->inset_right < 0.0)  { config->inset_right = 0.0; }
    if (config->header_height < 0.0){ config->header_height = 0.0; }
    return 0;
}

double pf_waterfall_column_width(const PFWaterfallConfig *config, double content_width) {
    if (config == NULL || config->column_count < 1) {
        return 0.0;
    }
    double usable = content_width - config->inset_left - config->inset_right;
    double spacing = config->interitem_spacing * (double)(config->column_count - 1);
    double width = (usable - spacing) / (double)config->column_count;
    return width > 0.0 ? width : 0.0;
}

/* 找出底部 Y 值最小的列 */
static int shortest_column(const double *column_heights, int column_count) {
    int shortest = 0;
    double min_height = column_heights[0];
    for (int i = 1; i < column_count; i++) {
        if (column_heights[i] < min_height - PF_EPSILON) {
            min_height = column_heights[i];
            shortest = i;
        }
    }
    return shortest;
}

int pf_waterfall_layout_compute(const PFWaterfallConfig *config_in,
                                int item_count,
                                double content_width,
                                PFItemHeightProvider height_provider,
                                void *context,
                                PFWaterfallFrame *out_frames,
                                double *out_content_height) {
    if (config_in == NULL || item_count < 0 || content_width <= 0.0) {
        return -1;
    }
    if (item_count > 0 && out_frames == NULL) {
        /* 只算总高度是允许的（out_frames 传 NULL） */
        if (out_content_height == NULL) {
            return -1;
        }
    }

    PFWaterfallConfig config = *config_in;
    pf_waterfall_config_validate(&config);

    const int column_count = config.column_count;
    const double item_width = pf_waterfall_column_width(&config, content_width);
    if (item_width <= 0.0) {
        return -1;
    }

    double *column_heights = (double *)calloc((size_t)column_count, sizeof(double));
    if (column_heights == NULL) {
        return -2;
    }

    const double top = config.inset_top + config.header_height;
    for (int c = 0; c < column_count; c++) {
        column_heights[c] = top;
    }

    for (int i = 0; i < item_count; i++) {
        int target_column = shortest_column(column_heights, column_count);

        double height = 0.0;
        if (height_provider != NULL) {
            height = height_provider(context, i);
        }
        if (!(height > 0.0) || !isfinite(height)) {
            /* 高度非法（<=0 或 NaN/Inf）时退化为正方形，避免出现 0 高度崩溃 */
            height = item_width;
        }

        const double x = config.inset_left
                       + (double)target_column * (item_width + config.interitem_spacing);
        const double y = column_heights[target_column];

        if (out_frames != NULL) {
            out_frames[i].x = x;
            out_frames[i].y = y;
            out_frames[i].width = item_width;
            out_frames[i].height = height;
        }

        column_heights[target_column] = y + height + config.line_spacing;
    }

    double max_bottom = 0.0;
    for (int c = 0; c < column_count; c++) {
        if (column_heights[c] > max_bottom) {
            max_bottom = column_heights[c];
        }
    }
    /* 最后一行不额外加 line_spacing，所以减去一次 */
    double total = max_bottom + config.inset_bottom;
    if (item_count > 0) {
        total -= config.line_spacing;
    }
    if (total < 0.0) {
        total = 0.0;
    }
    if (out_content_height != NULL) {
        *out_content_height = total;
    }

    free(column_heights);
    return 0;
}

/* 宽高比版本的上下文 */
struct ratio_context {
    const double *ratios;
    double column_width; /* 必须带上列宽：宽高比要乘以实际列宽才是真实高度 */
};

static double ratio_height_provider(void *ctx, int index) {
    struct ratio_context *c = (struct ratio_context *)ctx;
    /* 关键：宽高比只是比例，必须乘以列宽才是像素高度。
     * 早期版本漏乘列宽，导致所有 item 高度只有 0.6~1.8 点，
     * 列表几乎贴在一起 —— 单元测试只断言了「无重叠」没断言绝对高度，
     * 是靠布局可视化工具才发现的。 */
    return c->ratios[index] * c->column_width;
}

int pf_waterfall_layout_compute_by_ratio(const PFWaterfallConfig *config,
                                         int item_count,
                                         const double *aspect_ratios,
                                         double content_width,
                                         PFWaterfallFrame *out_frames,
                                         double *out_content_height) {
    if (item_count > 0 && aspect_ratios == NULL) {
        return -1;
    }
    struct ratio_context ctx;
    ctx.ratios = aspect_ratios;
    ctx.column_width = 0.0;
    if (config != NULL) {
        ctx.column_width = pf_waterfall_column_width(config, content_width);
    }
    return pf_waterfall_layout_compute(config, item_count, content_width,
                                       ratio_height_provider, &ctx,
                                       out_frames, out_content_height);
}

int pf_waterfall_validate_frames(const PFWaterfallFrame *frames,
                                 int item_count,
                                 double content_width,
                                 double content_height) {
    if (frames == NULL || item_count <= 0) {
        return 0;
    }

    for (int i = 0; i < item_count; i++) {
        /* 1. 越界检查：不能超出内容宽度 */
        if (frames[i].x < -PF_EPSILON ||
            frames[i].x + frames[i].width > content_width + PF_EPSILON) {
            return i;
        }
        /* 2. 负尺寸检查 */
        if (frames[i].width <= 0.0 || frames[i].height <= 0.0) {
            return i;
        }
        /* 3. 超出内容高度检查 */
        if (frames[i].y + frames[i].height > content_height + PF_EPSILON) {
            return i;
        }
    }

    /* 4. 重叠检查：同列相邻 item 不应重叠（不同列本来就可能 y 重叠） */
    for (int i = 0; i < item_count; i++) {
        for (int j = i + 1; j < item_count; j++) {
            const PFWaterfallFrame *a = &frames[i];
            const PFWaterfallFrame *b = &frames[j];

            const double x_overlap = (a->x < b->x + b->width - PF_EPSILON) &&
                                     (b->x < a->x + a->width - PF_EPSILON);
            const double y_overlap = (a->y < b->y + b->height - PF_EPSILON) &&
                                     (b->y < a->y + a->height - PF_EPSILON);
            if (x_overlap && y_overlap) {
                return i < j ? i : j;
            }
        }
    }

    return 0;
}
