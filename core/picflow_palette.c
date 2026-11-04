/*
 * picflow_palette.c
 * PicFlow Core —— HSL 配色引擎实现
 */

#include "picflow_palette.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double pf_random_unit(unsigned int seed, int index) {
    /*
     * 线性同余伪随机（LCG），参数取自 Numerical Recipes 的常用组合，
     * 周期足够大且分布均匀。同 seed + 同 index 必得同结果。
     */
    unsigned int x = (unsigned int)(index + 1) * 1103515245u + seed * 12345u + 1013904223u;
    x = (x << 13) ^ x;
    x = x * (unsigned int)(1274126177u);
    return (double)(x & 0x7FFFFFFFu) / (double)0x7FFFFFFFu;
}

static double clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

void pf_hsl_to_rgb(double h, double s, double l, PFColor *out) {
    if (out == NULL) {
        return;
    }
    h = fmod(h, 360.0);
    if (h < 0.0) {
        h += 360.0;
    }
    s = clamp01(s);
    l = clamp01(l);

    double c = (1.0 - fabs(2.0 * l - 1.0)) * s;
    double hp = h / 60.0;
    double x = c * (1.0 - fabs(fmod(hp, 2.0) - 1.0));
    double m = l - c / 2.0;

    double r = 0.0, g = 0.0, b = 0.0;
    if (hp < 1.0)      { r = c; g = x; b = 0.0; }
    else if (hp < 2.0) { r = x; g = c; b = 0.0; }
    else if (hp < 3.0) { r = 0.0; g = c; b = x; }
    else if (hp < 4.0) { r = 0.0; g = x; b = c; }
    else if (hp < 5.0) { r = x; g = 0.0; b = c; }
    else               { r = c; g = 0.0; b = x; }

    out->r = (r + m) * 255.0;
    out->g = (g + m) * 255.0;
    out->b = (b + m) * 255.0;
}

void pf_rgb_to_hsl(const PFColor *color, double *h, double *s, double *l) {
    if (color == NULL) {
        return;
    }
    double r = clamp01(color->r / 255.0);
    double g = clamp01(color->g / 255.0);
    double b = clamp01(color->b / 255.0);

    double max_v = r > g ? (r > b ? r : b) : (g > b ? g : b);
    double min_v = r < g ? (r < b ? r : b) : (g < b ? g : b);
    double delta = max_v - min_v;

    double hh = 0.0, ss = 0.0, ll = (max_v + min_v) / 2.0;

    if (delta > 1e-9) {
        ss = ll > 0.5 ? delta / (2.0 - max_v - min_v) : delta / (max_v + min_v);
        if (max_v == r) {
            hh = fmod((g - b) / delta, 6.0);
        } else if (max_v == g) {
            hh = (b - r) / delta + 2.0;
        } else {
            hh = (r - g) / delta + 4.0;
        }
        hh *= 60.0;
        if (hh < 0.0) {
            hh += 360.0;
        }
    }

    if (h) *h = hh;
    if (s) *s = ss;
    if (l) *l = ll;
}

void pf_palette_generate(unsigned int seed, PFArtworkSpec *out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->seed = seed;

    /* 基色：随机色相 + 中高饱和 + 中明度（保证视觉上不刺眼、不浑浊） */
    double base_hue      = pf_random_unit(seed, 1) * 360.0;
    double base_sat      = 0.45 + pf_random_unit(seed, 2) * 0.35;  /* 0.45~0.80 */
    double base_light    = 0.42 + pf_random_unit(seed, 3) * 0.22;  /* 0.42~0.64 */

    /* 主色：基色本身 */
    pf_hsl_to_rgb(base_hue, base_sat, base_light, &out->primary);

    /* 次色：分裂互补（+150° / -150°），视觉上协调但不沉闷 */
    pf_hsl_to_rgb(base_hue + 150.0, base_sat * 0.85, base_light * 1.05, &out->secondary);

    /* 强调色：黄金分割角（+38.2°），饱和度更高用于点睛 */
    pf_hsl_to_rgb(base_hue + 38.2, base_sat * 1.10, base_light * 1.12, &out->accent);

    /* 背景：同色相但极低饱和 + 高明度，保证前景图案清晰 */
    pf_hsl_to_rgb(base_hue, base_sat * 0.25, 0.93, &out->background);

    /* 线条：主色的深色版本 */
    pf_hsl_to_rgb(base_hue, base_sat * 0.9, base_light * 0.55, &out->line);

    /* 图案与密度 */
    out->pattern = (PFPatternKind)(seed % (unsigned int)PF_PATTERN_COUNT);
    out->motif_count = 5 + (int)(pf_random_unit(seed, 4) * 14.0);   /* 5~18 */
    out->density = 0.25 + pf_random_unit(seed, 5) * 0.55;           /* 0.25~0.80 */
}

/* --- 配色关系推导：供单测验证色彩和谐模型 --- */

static PFColor hsl_shift(const PFColor *base, double dh, double ds, double dl) {
    double h, s, l;
    pf_rgb_to_hsl(base, &h, &s, &l);
    PFColor out;
    pf_hsl_to_rgb(h + dh, clamp01(s + ds), clamp01(l + dl), &out);
    return out;
}

PFColor pf_palette_complementary(const PFColor *base) {
    return hsl_shift(base, 180.0, 0.0, 0.0);
}

PFColor pf_palette_triadic(const PFColor *base, int which) {
    double offset = (which == 0) ? 120.0 : 240.0;
    return hsl_shift(base, offset, 0.0, 0.0);
}

PFColor pf_palette_analogous(const PFColor *base, int which) {
    /* 黄金分割角 ±38.2° */
    double offset = (which >= 0) ? 38.2 : -38.2;
    return hsl_shift(base, offset, 0.0, 0.0);
}
