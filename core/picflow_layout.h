/*
 * picflow_layout.h
 * PicFlow Core —— 瀑布流布局算法（纯 C，零平台依赖）
 *
 * 设计动机：
 *   本算法是整个 App 性能与视觉的核心，把它从 UIKit 中抽离成纯 C，
 *   带来三个工程收益：
 *   1. 可以在 Windows/Linux 上用 gcc 直接编译并跑单元测试，
 *      无需 Mac，算法正确性可被 CI 持续验证；
 *   2. 纯 C 便于在 Instruments 中定位耗时（无 ObjC 消息派发开销）；
 *   3. iOS 端只做「取数据 -> 调算法 -> 渲染」的薄壳，逻辑集中不分散。
 *
 * 与 iOS 端 UIKit 的对应关系：
 *   PFWaterfallFrame   ≈ UICollectionViewLayoutAttributes.frame
 *   PFItemHeightProvider ≈ UICollectionViewLayoutDelegate 的高度回调
 */

#ifndef PICFLOW_LAYOUT_H
#define PICFLOW_LAYOUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* 配置                                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    int    column_count;      /* 列数，>= 1                                */
    double line_spacing;      /* 行间距（垂直）                            */
    double interitem_spacing; /* 列间距（水平）                            */
    double inset_top;         /* 外边距 - 上                                */
    double inset_left;        /* 外边距 - 左                                */
    double inset_bottom;      /* 外边距 - 下                                */
    double inset_right;       /* 外边距 - 右                                */
    double header_height;     /* 顶部自定义区域高度（如吸顶筛选栏）          */
} PFWaterfallConfig;

/* ------------------------------------------------------------------ */
/* 计算结果                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    double x;
    double y;
    double width;
    double height;
} PFWaterfallFrame;

/* ------------------------------------------------------------------ */
/* 高度提供者：与 UICollectionViewLayoutDelegate 高度回调等价的 C 回调 */
/* ------------------------------------------------------------------ */

/*
 * 返回第 index 个 item 的高度。
 * context 为调用方原样回传的上下文指针。
 * 返回 <= 0 表示该 item 高度非法，布局会退化为 item_width（正方形）。
 */
typedef double (*PFItemHeightProvider)(void *context, int index);

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

/* 默认配置：2 列、间距 8、外边距 8，与 iOS 端保持一致 */
PFWaterfallConfig pf_waterfall_config_default(void);

/* 校验配置，非法值会被修正。返回 0 表示配置合法。 */
int pf_waterfall_config_validate(PFWaterfallConfig *config);

/*
 * 核心布局计算。
 *
 * @param config          布局配置
 * @param item_count      item 数量
 * @param content_width   内容区宽度（通常是 collectionView 的宽度）
 * @param height_provider 高度回调，可为 NULL（此时所有 item 为正方形）
 * @param context         传给 height_provider 的上下文
 * @param out_frames      输出缓冲区，长度需 >= item_count；可为 NULL（仅算总高度）
 * @param out_content_height 输出内容总高度；可为 NULL
 *
 * @return 0 成功；-1 参数非法；-2 缓冲区不足
 */
int pf_waterfall_layout_compute(const PFWaterfallConfig *config,
                                int item_count,
                                double content_width,
                                PFItemHeightProvider height_provider,
                                void *context,
                                PFWaterfallFrame *out_frames,
                                double *out_content_height);

/*
 * 便捷版本：直接给定每个 item 的宽高比（高/宽），无需回调。
 * 用于测试与「已知原始像素尺寸」的快速路径。
 */
int pf_waterfall_layout_compute_by_ratio(const PFWaterfallConfig *config,
                                         int item_count,
                                         const double *aspect_ratios,
                                         double content_width,
                                         PFWaterfallFrame *out_frames,
                                         double *out_content_height);

/*
 * 计算单个 item 在给定配置下的列宽。
 * 列宽 = (可用宽度 - 列间距 * (列数-1)) / 列数
 */
double pf_waterfall_column_width(const PFWaterfallConfig *config, double content_width);

/*
 * 布局健康检查：验证 frames 是否存在重叠、是否越界。
 * 返回 0 表示布局健康；返回负数表示发现问题的下标（-1 表示未知问题）。
 * 这是单元测试里非常有用的一致性断言。
 */
int pf_waterfall_validate_frames(const PFWaterfallFrame *frames,
                                 int item_count,
                                 double content_width,
                                 double content_height);

#ifdef __cplusplus
}
#endif

#endif /* PICFLOW_LAYOUT_H */
