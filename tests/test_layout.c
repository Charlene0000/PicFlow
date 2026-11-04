/*
 * test_layout.c
 * PicFlow Core —— 瀑布流布局算法单元测试
 *
 * 这些断言正是「我的布局算法是对的」的书面证据：
 *   - 列宽计算正确
 *   - 最短列优先确实把 item 放到了当前最矮的列
 *   - 不重叠、不越界
 *   - 宽度变化会改变列宽（多机型适配）
 *   - 非法输入被优雅降级而不是崩溃
 */

#include "picflow_test.h"
#include "picflow_layout.h"
#include "picflow_catalog.h"

#include <string.h>

/* 高度回调：返回 context 中指定的固定高度 */
static double fixed_height_provider(void *ctx, int index) {
    const double *heights = (const double *)ctx;
    return heights[index];
}

/* 高度回调：按宽高比计算 */
static double ratio_height_provider(void *ctx, int index) {
    const double *ratios = (const double *)ctx;
    return ratios[index] * 100.0; /* 假设列宽 100 */
}

static void test_default_config(void) {
    PF_TEST_BEGIN("默认配置：2 列、间距 8");
    PFWaterfallConfig cfg = pf_waterfall_config_default();
    PF_ASSERT_EQ_INT(cfg.column_count, 2, "默认列数");
    PF_ASSERT_NEAR(cfg.line_spacing, 8.0, 1e-9, "默认行间距");
    PF_ASSERT_NEAR(cfg.interitem_spacing, 8.0, 1e-9, "默认列间距");
    PF_TEST_END("默认配置：2 列、间距 8");
}

static void test_column_width(void) {
    PF_TEST_BEGIN("列宽计算：扣除外边距与列间距后均分");
    PFWaterfallConfig cfg = pf_waterfall_config_default();
    /* 内容宽 375（即 iPhone 8 宽度），外边距 8+8，列间距 8 */
    double width = pf_waterfall_column_width(&cfg, 375.0);
    /* (375 - 8 - 8 - 8) / 2 = 175.5 */
    PF_ASSERT_NEAR(width, 175.5, 1e-9, "2 列时的列宽");
    PF_TEST_END("列宽计算：扣除外边距与列间距后均分");
}

static void test_column_width_device_adaptation(void) {
    PF_TEST_BEGIN("多机型适配：不同屏宽下列宽正确变化");
    PFWaterfallConfig cfg = pf_waterfall_config_default();

    double ipad_width = pf_waterfall_column_width(&cfg, 768.0);
    PF_ASSERT_NEAR(ipad_width, (768.0 - 16.0 - 8.0) / 2.0, 1e-9, "iPad 宽度下的列宽");

    cfg.column_count = 3;
    double ipad_3col = pf_waterfall_column_width(&cfg, 768.0);
    /* (768 - 16 - 16) / 3 */
    PF_ASSERT_NEAR(ipad_3col, (768.0 - 16.0 - 16.0) / 3.0, 1e-9, "iPad 三列时的列宽");

    /* 列数增加，列宽必须变小 —— 保证总宽度不溢出 */
    PF_ASSERT_TRUE(ipad_3col < ipad_width, "三列的列宽应小于两列");
    PF_TEST_END("多机型适配：不同屏宽下列宽正确变化");
}

static void test_shortest_column_first(void) {
    PF_TEST_BEGIN("最短列优先：item 落在当前最矮的列");
    PFWaterfallConfig cfg = pf_waterfall_config_default();
    cfg.column_count = 2;
    cfg.line_spacing = 0.0;
    cfg.interitem_spacing = 0.0;
    cfg.inset_top = cfg.inset_left = cfg.inset_bottom = cfg.inset_right = 0.0;

    double heights[4] = { 100.0, 50.0, 80.0, 40.0 };
    PFWaterfallFrame frames[4];
    int rc = pf_waterfall_layout_compute(&cfg, 4, 200.0, fixed_height_provider, heights, frames, NULL);
    PF_ASSERT_EQ_INT(rc, 0, "布局计算应成功");

    /* 列宽 = 200/2 = 100
     * item0 -> 列0 (列0 高 0, 列1 高 0，取列0) -> y=0
     * item1 -> 列1 (列0 高 100, 列1 高 0)          -> y=0
     * item2 -> 列1 (列0 高 100, 列1 高 50)          -> y=50
     * item3 -> 列0 (列0 高 100, 列1 高 130)         -> y=100
     */
    PF_ASSERT_NEAR(frames[0].x, 0.0, 1e-9, "item0 应在第 0 列");
    PF_ASSERT_NEAR(frames[0].y, 0.0, 1e-9, "item0 的 y");
    PF_ASSERT_NEAR(frames[1].x, 100.0, 1e-9, "item1 应在第 1 列");
    PF_ASSERT_NEAR(frames[2].x, 100.0, 1e-9, "item2 应在第 1 列（更矮）");
    PF_ASSERT_NEAR(frames[2].y, 50.0, 1e-9, "item2 的 y 紧接 item1 之下");
    PF_ASSERT_NEAR(frames[3].x, 0.0, 1e-9, "item3 应回到第 0 列");
    PF_ASSERT_NEAR(frames[3].y, 100.0, 1e-9, "item3 的 y");
    PF_TEST_END("最短列优先：item 落在当前最矮的列");
}

static void test_no_overlap_and_bounds(void) {
    PF_TEST_BEGIN("布局健康：无重叠、不越界、尺寸合法");
    PFWaterfallConfig cfg = pf_waterfall_config_default();
    cfg.column_count = 3;

    /* 用真实图库的宽高比做压力测试 */
    double ratios[PF_CATALOG_CAPACITY];
    PFCatalogEntry entries[PF_CATALOG_CAPACITY];
    pf_catalog_build(entries, PF_CATALOG_CAPACITY);
    for (int i = 0; i < PF_CATALOG_CAPACITY; i++) {
        ratios[i] = entries[i].aspect_ratio;
    }

    PFWaterfallFrame frames[PF_CATALOG_CAPACITY];
    double content_height = 0.0;
    int rc = pf_waterfall_layout_compute_by_ratio(&cfg, PF_CATALOG_CAPACITY, ratios,
                                                  375.0, frames, &content_height);
    PF_ASSERT_EQ_INT(rc, 0, "布局计算应成功");
    PF_ASSERT_TRUE(content_height > 0.0, "内容高度应大于 0");

    int bad = pf_waterfall_validate_frames(frames, PF_CATALOG_CAPACITY, 375.0, content_height);
    PF_ASSERT_EQ_INT(bad, 0, "不应存在重叠或越界的 item");

    /* 所有 item 宽度应一致（瀑布流只变高不变宽） */
    for (int i = 1; i < PF_CATALOG_CAPACITY; i++) {
        PF_ASSERT_NEAR(frames[i].width, frames[0].width, 1e-9, "所有 item 宽度应相同");
    }
    PF_TEST_END("布局健康：无重叠、不越界、尺寸合法");
}

static void test_content_height_accuracy(void) {
    PF_TEST_BEGIN("内容总高度：等于最高列底边 + 下外边距");
    PFWaterfallConfig cfg = pf_waterfall_config_default();
    cfg.column_count = 2;
    cfg.inset_bottom = 10.0;

    double heights[3] = { 100.0, 200.0, 50.0 };
    PFWaterfallFrame frames[3];
    double content_height = 0.0;
    pf_waterfall_layout_compute(&cfg, 3, 200.0, fixed_height_provider, heights, frames, &content_height);

    /* 列宽 100（含间距 8 -> (200-16-8)/2 = 88）
     * 列0: item0 y=8 高100 -> 底 108 (+8 spacing) = 116
     * 列1: item1 y=8 高200 -> 底 208 (+8) = 216
     * item2 -> 列0 (116 < 216) y=116 高 50 -> 底 166
     * 最高列底 = 216，减去末尾 line_spacing(8) = 208，+ inset_bottom(10) = 218
     */
    PF_ASSERT_NEAR(content_height, 218.0, 1e-6, "内容总高度");
    PF_TEST_END("内容总高度：等于最高列底边 + 下外边距");
}

static void test_invalid_input_degradation(void) {
    PF_TEST_BEGIN("健壮性：非法高度退化为正方形而非崩溃");
    PFWaterfallConfig cfg = pf_waterfall_config_default();
    double heights[3] = { -50.0, 0.0, 100.0 };
    PFWaterfallFrame frames[3];
    double content_height = 0.0;

    int rc = pf_waterfall_layout_compute(&cfg, 3, 375.0, fixed_height_provider, heights, frames, &content_height);
    PF_ASSERT_EQ_INT(rc, 0, "非法高度不应导致失败");
    PF_ASSERT_TRUE(frames[0].height > 0.0, "非法高度应退化为列宽");
    PF_ASSERT_TRUE(frames[1].height > 0.0, "0 高度应退化为列宽");
    PF_ASSERT_NEAR(frames[2].height, 100.0, 1e-9, "合法高度应保持不变");
    PF_TEST_END("健壮性：非法高度退化为正方形而非崩溃");
}

static void test_edge_cases(void) {
    PF_TEST_BEGIN("边界：空列表 / 零列 / 非法配置");
    PFWaterfallConfig cfg = pf_waterfall_config_default();
    PFWaterfallFrame frames[4];
    double content_height = -1.0;

    /* 空列表 */
    int rc = pf_waterfall_layout_compute(&cfg, 0, 375.0, NULL, NULL, frames, &content_height);
    PF_ASSERT_EQ_INT(rc, 0, "空列表应成功");
    PF_ASSERT_NEAR(content_height, 16.0, 1e-9, "空列表的内容高度 = 上下外边距之和");

    /* 非法列数被修正 */
    PFWaterfallConfig bad = pf_waterfall_config_default();
    bad.column_count = 0;
    bad.line_spacing = -10.0;
    pf_waterfall_config_validate(&bad);
    PF_ASSERT_EQ_INT(bad.column_count, 1, "列数应被修正为 1");
    PF_ASSERT_NEAR(bad.line_spacing, 0.0, 1e-9, "负间距应被修正为 0");

    /* 非法内容宽度 */
    rc = pf_waterfall_layout_compute(&cfg, 4, 0.0, NULL, NULL, frames, &content_height);
    PF_ASSERT_EQ_INT(rc, -1, "非法宽度应返回错误码");

    /* NULL 高度回调时退化为正方形 */
    rc = pf_waterfall_layout_compute(&cfg, 4, 375.0, NULL, NULL, frames, &content_height);
    PF_ASSERT_EQ_INT(rc, 0, "NULL 回调应成功");
    PF_ASSERT_NEAR(frames[0].height, frames[0].width, 1e-9, "应退化为正方形");
    PF_TEST_END("边界：空列表 / 零列 / 非法配置");
}

static void test_header_offset(void) {
    PF_TEST_BEGIN("顶部预留：header 高度正确偏移所有 item");
    PFWaterfallConfig cfg = pf_waterfall_config_default();
    cfg.column_count = 2;
    cfg.header_height = 44.0;

    PFWaterfallFrame frames[2];
    double content_height = 0.0;
    pf_waterfall_layout_compute(&cfg, 2, 375.0, ratio_height_provider, (double[]){1.0, 1.0}, frames, &content_height);
    PF_ASSERT_NEAR(frames[0].y, 52.0, 1e-9, "首行 y = inset_top(8) + header(44)");
    PF_TEST_END("顶部预留：header 高度正确偏移所有 item");
}

static void test_absolute_heights(void) {
    PF_TEST_BEGIN("绝对高度：宽高比必须乘以列宽得到真实像素高度");
    PFWaterfallConfig cfg = pf_waterfall_config_default();
    cfg.column_count = 2;

    double ratios[4] = { 1.0, 1.5, 2.0, 0.5 };
    PFWaterfallFrame frames[4];
    double content_height = 0.0;
    int rc = pf_waterfall_layout_compute_by_ratio(&cfg, 4, ratios, 375.0, frames, &content_height);
    PF_ASSERT_EQ_INT(rc, 0, "布局计算应成功");

    /* 列宽 = (375 - 8 - 8 - 8) / 2 = 175.5
     * 因此高度应为 比例 * 175.5：175.5 / 263.25 / 351 / 87.75
     * 如果高度回调漏乘列宽，这里会得到 1.0 / 1.5 / 2.0 / 0.5 —— 断言立刻失败。 */
    PF_ASSERT_NEAR(frames[0].height, 175.5, 0.01, "比例 1.0 的高度应等于列宽");
    PF_ASSERT_NEAR(frames[1].height, 263.25, 0.01, "比例 1.5 的高度");
    PF_ASSERT_NEAR(frames[2].height, 351.0, 0.01, "比例 2.0 的高度");
    PF_ASSERT_NEAR(frames[3].height, 87.75, 0.01, "比例 0.5 的高度");

    /* 内容总高度必须是几百上千像素量级，而不是几十 */
    PF_ASSERT_TRUE(content_height > 500.0,
                   "内容高度应达到数百像素量级（若只有几十说明高度计算漏乘列宽）");

    /* 校验实际绘制时的尺寸一致性：所有宽度都应等于列宽 */
    for (int i = 0; i < 4; i++) {
        PF_ASSERT_NEAR(frames[i].width, 175.5, 0.01, "item 宽度应等于列宽");
    }
    PF_TEST_END("绝对高度：宽高比必须乘以列宽得到真实像素高度");
}

static void test_realistic_content_height(void) {
    PF_TEST_BEGIN("真实图库：24 项的布局高度符合瀑布流量级");
    PFWaterfallConfig cfg = pf_waterfall_config_default();
    cfg.column_count = 2;

    PFCatalogEntry entries[PF_CATALOG_CAPACITY];
    pf_catalog_build(entries, PF_CATALOG_CAPACITY);
    double ratios[24];
    for (int i = 0; i < 24; i++) {
        ratios[i] = entries[i].aspect_ratio;
    }

    PFWaterfallFrame frames[24];
    double content_height = 0.0;
    pf_waterfall_layout_compute_by_ratio(&cfg, 24, ratios, 375.0, frames, &content_height);

    /* 2 列 24 项 = 每列 12 项，单项高度约 175 * (0.62~1.78) ≈ 108~311，
     * 12 项累加应在 1500~4000 像素量级。 */
    PF_ASSERT_TRUE(content_height > 1200.0 && content_height < 5000.0,
                   "内容高度应在合理区间内");
    PF_ASSERT_EQ_INT(pf_waterfall_validate_frames(frames, 24, 375.0, content_height), 0,
                     "布局应健康");
    PF_TEST_END("真实图库：24 项的布局高度符合瀑布流量级");
}

int main(void) {
    printf("\n========================================\n");
    printf("  PicFlow Core —— 瀑布流布局算法测试\n");
    printf("========================================\n\n");

    test_default_config();
    test_column_width();
    test_column_width_device_adaptation();
    test_shortest_column_first();
    test_absolute_heights();
    test_realistic_content_height();
    test_no_overlap_and_bounds();
    test_content_height_accuracy();
    test_invalid_input_degradation();
    test_edge_cases();
    test_header_offset();

    PF_TEST_REPORT("布局算法");
}
