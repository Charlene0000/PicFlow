/*
 * test_palette.c
 * PicFlow Core —— 配色引擎与图库单元测试
 *
 * 关键性质：
 *   1. 确定性：同 seed 必得同结果（这是程序化生成能替代外部图源的前提）；
 *   2. 色彩模型正确：HSL -> RGB -> HSL 往返误差在容差内；
 *   3. 和谐关系正确：互补色相差 180°，三角色相差 120°；
 *   4. 图库宽高比覆盖足够范围（否则瀑布流看不出高度差）。
 */

#include "picflow_test.h"
#include "picflow_palette.h"
#include "picflow_catalog.h"

#include <math.h>
#include <string.h>

static double hue_of(const PFColor *c) {
    double h, s, l;
    pf_rgb_to_hsl(c, &h, &s, &l);
    return h;
}

static void test_hsl_roundtrip(void) {
    PF_TEST_BEGIN("HSL -> RGB -> HSL 往返一致");
    double hues[] = { 0.0, 45.0, 120.0, 180.0, 240.0, 300.0, 359.0 };
    for (int i = 0; i < 7; i++) {
        PFColor rgb;
        pf_hsl_to_rgb(hues[i], 0.6, 0.5, &rgb);
        double h, s, l;
        pf_rgb_to_hsl(&rgb, &h, &s, &l);
        PF_ASSERT_NEAR(h, hues[i], 0.5, "色相往返");
        PF_ASSERT_NEAR(s, 0.6, 0.01, "饱和度往返");
        PF_ASSERT_NEAR(l, 0.5, 0.01, "明度往返");
    }
    PF_TEST_END("HSL -> RGB -> HSL 往返一致");
}

static void test_hsl_bounds(void) {
    PF_TEST_BEGIN("HSL 边界：黑白灰与纯色");
    PFColor black, white, gray;
    pf_hsl_to_rgb(0.0, 0.0, 0.0, &black);
    pf_hsl_to_rgb(0.0, 0.0, 1.0, &white);
    pf_hsl_to_rgb(0.0, 0.0, 0.5, &gray);
    PF_ASSERT_NEAR(black.r, 0.0, 0.5, "黑色 R");
    PF_ASSERT_NEAR(black.g, 0.0, 0.5, "黑色 G");
    PF_ASSERT_NEAR(black.b, 0.0, 0.5, "黑色 B");
    PF_ASSERT_NEAR(white.r, 255.0, 0.5, "白色 R");
    PF_ASSERT_NEAR(gray.r, 127.5, 1.0, "灰色 R");

    PFColor red;
    pf_hsl_to_rgb(0.0, 1.0, 0.5, &red);
    PF_ASSERT_NEAR(red.r, 255.0, 0.5, "纯红 R");
    PF_ASSERT_NEAR(red.g, 0.0, 0.5, "纯红 G");
    PF_ASSERT_NEAR(red.b, 0.0, 0.5, "纯红 B");

    /* 超出范围的输入应被钳制而不是产生垃圾值 */
    PFColor clamped;
    pf_hsl_to_rgb(400.0, 2.0, -1.0, &clamped);
    PF_ASSERT_TRUE(clamped.r >= 0.0 && clamped.r <= 255.0, "R 应在合法范围");
    PF_ASSERT_TRUE(clamped.g >= 0.0 && clamped.g <= 255.0, "G 应在合法范围");
    PF_ASSERT_TRUE(clamped.b >= 0.0 && clamped.b <= 255.0, "B 应在合法范围");
    PF_TEST_END("HSL 边界：黑白灰与纯色");
}

static void test_determinism(void) {
    PF_TEST_BEGIN("确定性：同 seed 必得同结果");
    PFArtworkSpec a, b, c;
    pf_palette_generate(12345u, &a);
    pf_palette_generate(12345u, &b);
    pf_palette_generate(12346u, &c);

    PF_ASSERT_NEAR(a.primary.r, b.primary.r, 1e-9, "同 seed 主色 R 应相同");
    PF_ASSERT_NEAR(a.primary.g, b.primary.g, 1e-9, "同 seed 主色 G 应相同");
    PF_ASSERT_NEAR(a.accent.b, b.accent.b, 1e-9, "同 seed 强调色 B 应相同");
    PF_ASSERT_EQ_INT(a.pattern, b.pattern, "同 seed 图案类型应相同");
    PF_ASSERT_EQ_INT(a.motif_count, b.motif_count, "同 seed 元素数应相同");

    /* 不同 seed 应产生不同结果 */
    int differs = (fabs(a.primary.r - c.primary.r) > 1e-6) ||
                  (fabs(a.primary.g - c.primary.g) > 1e-6) ||
                  (fabs(a.primary.b - c.primary.b) > 1e-6);
    PF_ASSERT_TRUE(differs, "不同 seed 应产生不同配色");
    PF_TEST_END("确定性：同 seed 必得同结果");
}

static void test_random_range(void) {
    PF_TEST_BEGIN("伪随机：值域在 [0,1) 且分布不极端");
    double min_v = 1.0, max_v = 0.0, sum = 0.0;
    const int N = 1000;
    for (int i = 0; i < N; i++) {
        double v = pf_random_unit(777u, i);
        PF_ASSERT_TRUE(v >= 0.0 && v < 1.0, "值应在 [0,1)");
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
        sum += v;
    }
    PF_ASSERT_TRUE(min_v < 0.05, "应能取到接近 0 的值");
    PF_ASSERT_TRUE(max_v > 0.95, "应能取到接近 1 的值");
    PF_ASSERT_NEAR(sum / N, 0.5, 0.05, "均值应接近 0.5");
    PF_TEST_END("伪随机：值域在 [0,1) 且分布不极端");
}

static void test_harmony_relations(void) {
    PF_TEST_BEGIN("和谐关系：互补 180°、三角 120°、类似 38.2°");
    PFColor base;
    pf_hsl_to_rgb(30.0, 0.7, 0.5, &base);

    PFColor comp = pf_palette_complementary(&base);
    double diff = fabs(hue_of(&comp) - hue_of(&base));
    if (diff > 180.0) diff = 360.0 - diff;
    PF_ASSERT_NEAR(diff, 180.0, 0.5, "互补色色相差应为 180°");

    PFColor tri = pf_palette_triadic(&base, 0);
    double tdiff = fabs(hue_of(&tri) - hue_of(&base));
    if (tdiff > 180.0) tdiff = 360.0 - tdiff;
    PF_ASSERT_NEAR(tdiff, 120.0, 0.5, "三角色色相差应为 120°");

    PFColor ana = pf_palette_analogous(&base, 1);
    double adiff = fabs(hue_of(&ana) - hue_of(&base));
    if (adiff > 180.0) adiff = 360.0 - adiff;
    PF_ASSERT_NEAR(adiff, 38.2, 0.5, "类似色色相差应为 38.2°（黄金分割角）");
    PF_TEST_END("和谐关系：互补 180°、三角 120°、类似 38.2°");
}

static void test_catalog_aspect_ratio_coverage(void) {
    PF_TEST_BEGIN("图库：宽高比覆盖足够范围（瀑布流才能看出高度差）");
    PFCatalogEntry entries[PF_CATALOG_CAPACITY];
    int count = pf_catalog_build(entries, PF_CATALOG_CAPACITY);
    PF_ASSERT_EQ_INT(count, PF_CATALOG_CAPACITY, "条目数");

    double min_r = 10.0, max_r = 0.0;
    int unique_seeds = 0;
    unsigned int seen[PF_CATALOG_CAPACITY];
    for (int i = 0; i < count; i++) {
        if (entries[i].aspect_ratio < min_r) min_r = entries[i].aspect_ratio;
        if (entries[i].aspect_ratio > max_r) max_r = entries[i].aspect_ratio;
        int dup = 0;
        for (int j = 0; j < i; j++) {
            if (seen[j] == entries[i].seed) { dup = 1; break; }
        }
        if (!dup) unique_seeds++;
        seen[i] = entries[i].seed;
    }
    PF_ASSERT_TRUE(min_r < 0.8, "应有偏矮的图（横图）");
    PF_ASSERT_TRUE(max_r > 1.5, "应有偏高的图（竖图）");
    PF_ASSERT_TRUE(max_r - min_r > 1.0, "宽高比跨度应大于 1.0");
    PF_ASSERT_EQ_INT(unique_seeds, count, "每个条目的 seed 应唯一");
    PF_TEST_END("图库：宽高比覆盖足够范围（瀑布流才能看出高度差）");
}

static void test_catalog_thumbnail_request(void) {
    PF_TEST_BEGIN("图库：缩略图请求规格正确");
    PFCatalogEntry entry;
    pf_catalog_entry_at(0, &entry);

    PFThumbnailRequest request;
    pf_catalog_thumbnail_request(0, 320, &request);
    PF_ASSERT_EQ_INT(request.pixel_width, 320, "宽度");
    PF_ASSERT_NEAR((double)request.pixel_height,
                   320.0 * entry.aspect_ratio, 1.0, "高度按宽高比推导");
    PF_ASSERT_TRUE(request.pixel_height > 0, "高度应为正");

    /* 非法宽度应被修正 */
    pf_catalog_thumbnail_request(0, -5, &request);
    PF_ASSERT_EQ_INT(request.pixel_width, 320, "非法宽度应退化为默认值");

    char key[64];
    pf_catalog_cache_key(3, 320, key, sizeof(key));
    PF_ASSERT_TRUE(strlen(key) > 0, "缓存 key 不应为空");
    PF_ASSERT_STR_EQ(key, "pf-art-3-320", "缓存 key 格式");
    PF_TEST_END("图库：缩略图请求规格正确");
}

int main(void) {
    printf("\n========================================\n");
    printf("  PicFlow Core —— 配色引擎与图库测试\n");
    printf("========================================\n\n");

    test_hsl_roundtrip();
    test_hsl_bounds();
    test_determinism();
    test_random_range();
    test_harmony_relations();
    test_catalog_aspect_ratio_coverage();
    test_catalog_thumbnail_request();

    PF_TEST_REPORT("配色引擎与图库");
}
