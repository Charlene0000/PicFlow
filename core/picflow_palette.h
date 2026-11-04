/*
 * picflow_palette.h
 * PicFlow Core —— 自研 HSL 配色引擎 + 程序化图案规格（纯 C）
 *
 * 为什么不用外部图片 API？
 *
 * 1. 零外部依赖：不需要 API Key、不受网络与配额影响、离线可跑、CI 可复现；
 * 2. 版权干净：程序化生成的图案没有第三方素材的授权风险，
 *    简历项目可以放心给面试官看、可以商用；
 * 3. 技术含量更高：色彩和谐（Harmony）是有明确数学模型的，
 *    用 HSL 色轮 + 黄金分割做配色推导，比调用接口更能体现工程能力；
 * 4. 完全可测试：配色输出是确定的（同一 seed 必得同一结果），
 *    纯 C 实现让这一点可以被单元测试断言。
 *
 * 配色模型（HSL）：
 *   H 色相 0~360°   S 饱和度 0~1   L 明度 0~1
 *   互补色      hue + 180°
 *   三角配色    hue + 120°、hue + 240°
 *   分裂互补    hue + 150°、hue + 210°
 *   黄金分割    hue + 38.2°、hue - 38.2°（模拟自然界的和谐关系）
 */

#ifndef PICFLOW_PALETTE_H
#define PICFLOW_PALETTE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double r; /* 0~255 */
    double g; /* 0~255 */
    double b; /* 0~255 */
} PFColor;

/* 图案类型 */
typedef enum {
    PF_PATTERN_GRID = 0,      /* 网格          */
    PF_PATTERN_CIRCLES,        /* 同心圆        */
    PF_PATTERN_DIAGONAL,       /* 斜条纹        */
    PF_PATTERN_WAVES,          /* 波形          */
    PF_PATTERN_TRIANGLES,      /* 三角          */
    PF_PATTERN_COUNT
} PFPatternKind;

/* 一张程序化图片的完整绘制规格 */
typedef struct {
    unsigned int seed;        /* 随机种子，同 seed 必得同结果       */
    PFColor      background;  /* 背景色                            */
    PFColor      primary;     /* 主色                              */
    PFColor      secondary;   /* 次色                              */
    PFColor      accent;      /* 强调色                            */
    PFColor      line;        /* 线条/描边色                        */
    PFPatternKind pattern;    /* 图案类型                          */
    int          motif_count; /* 图案元素数量                      */
    double       density;     /* 图案密度 0~1                      */
} PFArtworkSpec;

/* HSL -> RGB，h 单位为度，s/l 为 0~1 */
void pf_hsl_to_rgb(double h, double s, double l, PFColor *out);

/* RGB -> HSL（便于测试往返一致性） */
void pf_rgb_to_hsl(const PFColor *color, double *h, double *s, double *l);

/* 线性同余伪随机：把 seed 映射到 [0,1) */
double pf_random_unit(unsigned int seed, int index);

/*
 * 按 seed 生成一份完整的图案规格。
 * 同一 seed 的输出完全确定 —— 这条性质会被单元测试断言。
 */
void pf_palette_generate(unsigned int seed, PFArtworkSpec *out);

/* 按配色关系推导和谐色（供单测验证配色模型） */
PFColor pf_palette_complementary(const PFColor *base);
PFColor pf_palette_triadic(const PFColor *base, int which); /* which: 0 或 1 */
PFColor pf_palette_analogous(const PFColor *base, int which); /* which: -1 或 +1 */

#ifdef __cplusplus
}
#endif

#endif /* PICFLOW_PALETTE_H */
