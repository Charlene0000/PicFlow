/*
 * test_paging.c
 * PicFlow Core —— 分页状态机单元测试
 *
 * 覆盖典型的分页 bug：
 *   - 重复请求（连点、滚动抖动）
 *   - 加载更多与下拉刷新冲突
 *   - 最后一页判定错误（多请求一页 / 提前停止）
 *   - 失败后丢失已加载数据
 */

#include "picflow_test.h"
#include "picflow_paging.h"

static void test_initial_state(void) {
    PF_TEST_BEGIN("初始状态：空闲、有更多、页码为 0");
    PFPagingContext ctx;
    pf_paging_init(&ctx, 30, 5);
    PF_ASSERT_EQ_INT(ctx.state, PF_PAGING_IDLE, "初始状态");
    PF_ASSERT_EQ_INT(ctx.page, 0, "初始页码");
    PF_ASSERT_EQ_INT(ctx.item_count, 0, "初始条数");
    PF_ASSERT_EQ_INT(ctx.has_more, 1, "初始应有更多");
    PF_ASSERT_EQ_INT(ctx.request_in_flight, 0, "初始无请求进行中");
    PF_TEST_END("初始状态：空闲、有更多、页码为 0");
}

static void test_first_page_flow(void) {
    PF_TEST_BEGIN("首屏流程：请求 -> 成功 -> 已加载");
    PFPagingContext ctx;
    pf_paging_init(&ctx, 30, 5);

    PF_ASSERT_EQ_INT(pf_paging_should_request(&ctx, 0), 1, "应允许请求");
    int page = pf_paging_begin_request(&ctx, 1);
    PF_ASSERT_EQ_INT(page, 1, "首屏请求第 1 页");
    PF_ASSERT_EQ_INT(ctx.state, PF_PAGING_LOADING, "状态应为 LOADING");
    PF_ASSERT_EQ_INT(ctx.request_in_flight, 1, "应有请求进行中");

    int has_more = pf_paging_succeed(&ctx, 30, 150);
    PF_ASSERT_EQ_INT(has_more, 1, "还有更多");
    PF_ASSERT_EQ_INT(ctx.state, PF_PAGING_LOADED, "状态应为 LOADED");
    PF_ASSERT_EQ_INT(ctx.page, 1, "页码应为 1");
    PF_ASSERT_EQ_INT(ctx.item_count, 30, "累计 30 条");
    PF_TEST_END("首屏流程：请求 -> 成功 -> 已加载");
}

static void test_duplicate_request_guard(void) {
    PF_TEST_BEGIN("并发保护：请求进行中时拒绝重复请求");
    PFPagingContext ctx;
    pf_paging_init(&ctx, 30, 5);

    pf_paging_begin_request(&ctx, 1);
    /* 请求还在飞，此时再触发一次（比如用户连点下拉刷新） */
    PF_ASSERT_EQ_INT(pf_paging_should_request(&ctx, 0), 0, "应拒绝重复请求");
    PF_ASSERT_EQ_INT(pf_paging_should_request(&ctx, 1), 0, "reset 也应被拒绝（请求中）");
    pf_paging_succeed(&ctx, 30, 150);
    PF_ASSERT_EQ_INT(pf_paging_should_request(&ctx, 0), 1, "请求结束后应允许下一次");
    PF_TEST_END("并发保护：请求进行中时拒绝重复请求");
}

static void test_load_more_paging(void) {
    PF_TEST_BEGIN("加载更多：页码正确递增、累计条数正确");
    PFPagingContext ctx;
    pf_paging_init(&ctx, 30, 5);

    pf_paging_begin_request(&ctx, 1);
    pf_paging_succeed(&ctx, 30, 150);

    for (int i = 0; i < 3; i++) {
        int page = pf_paging_begin_request(&ctx, 0);
        PF_ASSERT_EQ_INT(page, ctx.page + 1, "请求页码应为当前页 + 1");
        pf_paging_succeed(&ctx, 30, 150);
    }
    PF_ASSERT_EQ_INT(ctx.page, 4, "共加载 4 页");
    PF_ASSERT_EQ_INT(ctx.item_count, 120, "累计 120 条");
    PF_ASSERT_EQ_INT(ctx.state, PF_PAGING_LOADED, "状态应为 LOADED");
    PF_TEST_END("加载更多：页码正确递增、累计条数正确");
}

static void test_max_pages_boundary(void) {
    PF_TEST_BEGIN("边界：达到 max_pages 后不再请求");
    PFPagingContext ctx;
    pf_paging_init(&ctx, 30, 3);

    for (int i = 0; i < 3; i++) {
        pf_paging_begin_request(&ctx, (i == 0) ? 1 : 0);
        pf_paging_succeed(&ctx, 30, 0);
    }
    PF_ASSERT_EQ_INT(ctx.page, 3, "已加载 3 页");
    PF_ASSERT_EQ_INT(ctx.has_more, 0, "不应再有更多");
    PF_ASSERT_EQ_INT(ctx.state, PF_PAGING_NO_MORE_DATA, "状态应为 NO_MORE_DATA");
    PF_ASSERT_EQ_INT(pf_paging_should_request(&ctx, 0), 0, "应拒绝加载更多");
    /* 但 reset 应该仍然允许（下拉刷新要能重新拉） */
    PF_ASSERT_EQ_INT(pf_paging_should_request(&ctx, 1), 1, "reset 应被允许");
    PF_TEST_END("边界：达到 max_pages 后不再请求");
}

static void test_short_page_means_end(void) {
    PF_TEST_BEGIN("边界：返回数量不足一页视为最后一页");
    PFPagingContext ctx;
    pf_paging_init(&ctx, 30, 0); /* 不限页数 */

    pf_paging_begin_request(&ctx, 1);
    int has_more = pf_paging_succeed(&ctx, 12, 0); /* 只回了 12 条 < 30 */
    PF_ASSERT_EQ_INT(has_more, 0, "不足一页应判定为无更多");
    PF_ASSERT_EQ_INT(ctx.state, PF_PAGING_NO_MORE_DATA, "状态应为 NO_MORE_DATA");
    PF_TEST_END("边界：返回数量不足一页视为最后一页");
}

static void test_empty_page_means_end(void) {
    PF_TEST_BEGIN("边界：返回 0 条立即判定为无更多");
    PFPagingContext ctx;
    pf_paging_init(&ctx, 30, 0);

    pf_paging_begin_request(&ctx, 1);
    int has_more = pf_paging_succeed(&ctx, 0, 0);
    PF_ASSERT_EQ_INT(has_more, 0, "0 条应判定为无更多");
    PF_TEST_END("边界：返回 0 条立即判定为无更多");
}

static void test_failure_preserves_data(void) {
    PF_TEST_BEGIN("失败处理：保留已加载数据，可重试");
    PFPagingContext ctx;
    pf_paging_init(&ctx, 30, 5);

    pf_paging_begin_request(&ctx, 1);
    pf_paging_succeed(&ctx, 30, 150);
    PF_ASSERT_EQ_INT(ctx.item_count, 30, "首屏已加载 30 条");

    /* 第二页失败 */
    pf_paging_begin_request(&ctx, 0);
    pf_paging_fail(&ctx, -1004);
    PF_ASSERT_EQ_INT(ctx.state, PF_PAGING_ERROR, "状态应为 ERROR");
    PF_ASSERT_EQ_INT(ctx.last_error_code, -1004, "应记录错误码");
    PF_ASSERT_EQ_INT(ctx.item_count, 30, "失败后已加载数据不能丢");
    PF_ASSERT_EQ_INT(ctx.page, 1, "失败后页码不能前进");

    /* 失败后普通请求被拒绝，必须 reset */
    PF_ASSERT_EQ_INT(pf_paging_should_request(&ctx, 0), 0, "失败后应拒绝普通请求");
    PF_ASSERT_EQ_INT(pf_paging_should_request(&ctx, 1), 1, "失败后应允许 reset 重试");

    int page = pf_paging_begin_request(&ctx, 1);
    PF_ASSERT_EQ_INT(page, 1, "reset 后从第 1 页重新开始");
    PF_ASSERT_EQ_INT(ctx.item_count, 0, "reset 后累计条数清零");
    PF_TEST_END("失败处理：保留已加载数据，可重试");
}

static void test_reset_clears_state(void) {
    PF_TEST_BEGIN("下拉刷新：reset 清空分页状态");
    PFPagingContext ctx;
    pf_paging_init(&ctx, 30, 5);

    pf_paging_begin_request(&ctx, 1);
    pf_paging_succeed(&ctx, 30, 150);
    pf_paging_begin_request(&ctx, 0);
    pf_paging_succeed(&ctx, 30, 150);
    PF_ASSERT_EQ_INT(ctx.item_count, 60, "已加载 60 条");

    pf_paging_begin_request(&ctx, 1); /* 下拉刷新 */
    PF_ASSERT_EQ_INT(ctx.item_count, 0, "reset 后条数清零");
    PF_ASSERT_EQ_INT(ctx.page, 0, "reset 后页码归零");
    PF_ASSERT_EQ_INT(ctx.has_more, 1, "reset 后恢复为有更多");
    PF_TEST_END("下拉刷新：reset 清空分页状态");
}

static void test_state_names(void) {
    PF_TEST_BEGIN("状态名称：便于日志与断言");
    PF_ASSERT_STR_EQ(pf_paging_state_name(PF_PAGING_IDLE), "IDLE", "IDLE");
    PF_ASSERT_STR_EQ(pf_paging_state_name(PF_PAGING_LOADING), "LOADING", "LOADING");
    PF_ASSERT_STR_EQ(pf_paging_state_name(PF_PAGING_LOADING_MORE), "LOADING_MORE", "LOADING_MORE");
    PF_ASSERT_STR_EQ(pf_paging_state_name(PF_PAGING_LOADED), "LOADED", "LOADED");
    PF_ASSERT_STR_EQ(pf_paging_state_name(PF_PAGING_NO_MORE_DATA), "NO_MORE_DATA", "NO_MORE_DATA");
    PF_ASSERT_STR_EQ(pf_paging_state_name(PF_PAGING_ERROR), "ERROR", "ERROR");
    PF_TEST_END("状态名称：便于日志与断言");
}

int main(void) {
    printf("\n========================================\n");
    printf("  PicFlow Core —— 分页状态机测试\n");
    printf("========================================\n\n");

    test_initial_state();
    test_first_page_flow();
    test_duplicate_request_guard();
    test_load_more_paging();
    test_max_pages_boundary();
    test_short_page_means_end();
    test_empty_page_means_end();
    test_failure_preserves_data();
    test_reset_clears_state();
    test_state_names();

    PF_TEST_REPORT("分页状态机");
}
