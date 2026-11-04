/*
 * test_cache.c
 * PicFlow Core —— 内存预算 + LRU 缓存单元测试
 *
 * LRU 是最容易写出「能跑但不对」的模块：
 *   - 淘汰的应该是最久未用的，不是最早插入的；
 *   - 命中后要更新为最近使用；
 *   - 字节上限与条目上限要同时生效。
 * 这些性质无法靠肉眼保证，必须有断言。
 */

#include "picflow_test.h"
#include "picflow_cache.h"

#include <string.h>

static void test_budget_init(void) {
    PF_TEST_BEGIN("内存预算：默认值正确");
    PFMemoryBudget budget;
    pf_memory_budget_init(&budget, 60u * 1024u * 1024u);
    PF_ASSERT_EQ_INT(budget.memory_budget_bytes, 60u * 1024u * 1024u, "总预算 60MB");
    PF_ASSERT_EQ_INT(budget.max_concurrent, 6, "最大并发数");
    PF_ASSERT_EQ_INT(budget.reserved_bytes, 0, "初始占用为 0");
    PF_ASSERT_TRUE(pf_memory_budget_available(&budget) > 0, "应有可用空间");
    PF_TEST_END("内存预算：默认值正确");
}

static void test_budget_alloc_guard(void) {
    PF_TEST_BEGIN("内存预算：超出预算时拒绝分配");
    PFMemoryBudget budget;
    pf_memory_budget_init(&budget, 1024); /* 1KB 预算 -> 余量约 170 字节 */
    PF_ASSERT_TRUE(pf_memory_budget_available(&budget) > 0, "小预算下仍应有可用空间");
    PF_ASSERT_EQ_INT(pf_memory_budget_can_alloc(&budget, 100000), 0, "超额申请应被拒绝");

    PFMemoryBudget b2;
    pf_memory_budget_init(&b2, 1024u * 1024u); /* 1MB 预算 */
    size_t available = pf_memory_budget_available(&b2);
    PF_ASSERT_TRUE(available > 800u * 1024u, "可用空间应接近 1MB 减去六分之一余量");

    pf_memory_budget_reserve(&b2, 600u * 1024u);
    PF_ASSERT_EQ_INT(pf_memory_budget_can_alloc(&b2, 200u * 1024u), 1, "余量充足应允许");
    pf_memory_budget_reserve(&b2, 400u * 1024u);
    PF_ASSERT_EQ_INT(pf_memory_budget_can_alloc(&b2, 1), 0, "超额后应拒绝");
    pf_memory_budget_release(&b2, 1000u * 1024u);
    PF_ASSERT_EQ_INT(pf_memory_budget_can_alloc(&b2, 500u * 1024u), 1, "释放后应恢复");
    PF_TEST_END("内存预算：超出预算时拒绝分配");
}

static void test_budget_concurrency(void) {
    PF_TEST_BEGIN("内存预算：并发下载数受限");
    PFMemoryBudget budget;
    pf_memory_budget_init(&budget, 60u * 1024u * 1024u);
    for (int i = 0; i < 6; i++) {
        PF_ASSERT_EQ_INT(pf_memory_budget_can_start_download(&budget), 1, "前 6 个应允许");
        pf_memory_budget_download_started(&budget);
    }
    PF_ASSERT_EQ_INT(pf_memory_budget_can_start_download(&budget), 0, "第 7 个应被拒绝");
    pf_memory_budget_download_finished(&budget);
    PF_ASSERT_EQ_INT(pf_memory_budget_can_start_download(&budget), 1, "有空闲槽位应允许");
    PF_TEST_END("内存预算：并发下载数受限");
}

static void test_lru_basic(void) {
    PF_TEST_BEGIN("LRU：写入读取与统计");
    PFLruCache *cache = pf_lru_create(1024, 10);
    PF_ASSERT_TRUE(cache != NULL, "创建应成功");

    PF_ASSERT_EQ_INT(pf_lru_put_string(cache, "a", "hello"), 1, "写入 a");
    PF_ASSERT_EQ_INT(pf_lru_put(cache, "b", "world", 5), 1, "写入 b（二进制语义）");
    PF_ASSERT_EQ_INT(pf_lru_count(cache), 2, "条目数应为 2");
    PF_ASSERT_EQ_INT(pf_lru_bytes(cache), 6 + 5, "占用应为 6（hello 含\\0）+ 5 字节");

    const char *value = pf_lru_get_string(cache, "a");
    PF_ASSERT_TRUE(value != NULL, "应命中 a");
    PF_ASSERT_STR_EQ(value, "hello", "a 的内容");
    PF_ASSERT_TRUE(pf_lru_get(cache, "zzz") == NULL, "不存在的 key 应未命中");

    pf_lru_destroy(cache);
    PF_TEST_END("LRU：写入读取与统计");
}

static void test_lru_eviction_order(void) {
    PF_TEST_BEGIN("LRU：淘汰的是最久未使用的，而非最早插入的");
    PFLruCache *cache = pf_lru_create(1024, 2); /* 只放 2 条 */
    pf_lru_put(cache, "a", "1", 1);
    pf_lru_put(cache, "b", "2", 1);
    /* 访问 a，让它变成最近使用 */
    pf_lru_get(cache, "a");
    /* 写入 c，触发淘汰 —— 应该淘汰 b（最久未用），而不是 a */
    pf_lru_put(cache, "c", "3", 1);

    PF_ASSERT_EQ_INT(pf_lru_count(cache), 2, "仍应有 2 条");
    PF_ASSERT_TRUE(pf_lru_get(cache, "a") != NULL, "a 被访问过，不该被淘汰");
    PF_ASSERT_TRUE(pf_lru_get(cache, "b") == NULL, "b 最久未用，应被淘汰");
    PF_ASSERT_TRUE(pf_lru_get(cache, "c") != NULL, "c 是新写入的");
    PF_ASSERT_EQ_INT(pf_lru_evictions(cache), 1, "淘汰计数应为 1");

    pf_lru_destroy(cache);
    PF_TEST_END("LRU：淘汰的是最久未使用的，而非最早插入的");
}

static void test_lru_byte_limit(void) {
    PF_TEST_BEGIN("LRU：字节上限生效");
    PFLruCache *cache = pf_lru_create(100, 0); /* 仅字节上限，无条目上限 */

    /* 写入 10 条各 10 字节 = 100 字节，刚好装满 */
    for (int i = 0; i < 10; i++) {
        char key[8];
        snprintf(key, sizeof(key), "k%d", i);
        pf_lru_put(cache, key, "0123456789", 10);
    }
    PF_ASSERT_EQ_INT(pf_lru_count(cache), 10, "10 条 * 10 字节 = 100，刚好装满");
    PF_ASSERT_EQ_INT(pf_lru_bytes(cache), 100, "占用应为 100 字节");
    PF_ASSERT_EQ_INT(pf_lru_evictions(cache), 0, "未超限，不应有淘汰");

    /* 再写一条触发淘汰 */
    pf_lru_put(cache, "overflow", "0123456789", 10);
    PF_ASSERT_EQ_INT(pf_lru_count(cache), 10, "超出字节上限应触发淘汰，条目数不变");
    PF_ASSERT_EQ_INT(pf_lru_bytes(cache), 100, "占用不应超过上限");
    PF_ASSERT_EQ_INT(pf_lru_evictions(cache), 1, "应淘汰 1 条");

    pf_lru_destroy(cache);
    PF_TEST_END("LRU：字节上限生效");
}

static void test_lru_overwrite(void) {
    PF_TEST_BEGIN("LRU：重复写入同 key 覆盖而非新增");
    PFLruCache *cache = pf_lru_create(1024, 10);
    pf_lru_put_string(cache, "a", "short");
    pf_lru_put_string(cache, "a", "muchlonger");
    PF_ASSERT_EQ_INT(pf_lru_count(cache), 1, "条目数仍为 1");
    PF_ASSERT_EQ_INT(pf_lru_bytes(cache), 11, "字节数应更新为新值长度（含\\0）");
    PF_ASSERT_STR_EQ(pf_lru_get_string(cache, "a"), "muchlonger", "内容应为新值");
    pf_lru_destroy(cache);
    PF_TEST_END("LRU：重复写入同 key 覆盖而非新增");
}

static void test_lru_remove_and_clear(void) {
    PF_TEST_BEGIN("LRU：移除与清空");
    PFLruCache *cache = pf_lru_create(1024, 10);
    pf_lru_put(cache, "a", "1", 1);
    pf_lru_put(cache, "b", "2", 1);
    PF_ASSERT_EQ_INT(pf_lru_remove(cache, "a"), 1, "移除存在的 key");
    PF_ASSERT_EQ_INT(pf_lru_remove(cache, "a"), 0, "重复移除应返回 0");
    PF_ASSERT_EQ_INT(pf_lru_count(cache), 1, "移除后条目数");
    pf_lru_clear(cache);
    PF_ASSERT_EQ_INT(pf_lru_count(cache), 0, "清空后条目数为 0");
    PF_ASSERT_EQ_INT(pf_lru_bytes(cache), 0, "清空后字节数为 0");
    pf_lru_destroy(cache);
    PF_TEST_END("LRU：移除与清空");
}

static void test_lru_hit_rate(void) {
    PF_TEST_BEGIN("LRU：命中率统计");
    PFLruCache *cache = pf_lru_create(1024, 10);
    pf_lru_put(cache, "a", "1", 1);
    pf_lru_get(cache, "a");   /* 命中 */
    pf_lru_get(cache, "b");   /* 未命中 */
    pf_lru_get(cache, "a");   /* 命中 */
    double rate = pf_lru_hit_rate(cache);
    PF_ASSERT_NEAR(rate, 2.0 / 3.0, 1e-9, "命中率应为 2/3");
    pf_lru_destroy(cache);
    PF_TEST_END("LRU：命中率统计");
}

static void test_lru_null_safety(void) {
    PF_TEST_BEGIN("LRU：空指针安全");
    pf_lru_destroy(NULL);
    pf_lru_clear(NULL);
    PF_ASSERT_EQ_INT(pf_lru_count(NULL), 0, "空指针计数为 0");
    PF_ASSERT_TRUE(pf_lru_get(NULL, "x") == NULL, "空指针读取返回 NULL");
    PF_ASSERT_EQ_INT(pf_lru_put(NULL, "x", "v", 1), 0, "空指针写入失败");
    PF_TEST_END("LRU：空指针安全");
}

int main(void) {
    printf("\n========================================\n");
    printf("  PicFlow Core —— 缓存与内存预算测试\n");
    printf("========================================\n\n");

    test_budget_init();
    test_budget_alloc_guard();
    test_budget_concurrency();
    test_lru_basic();
    test_lru_eviction_order();
    test_lru_byte_limit();
    test_lru_overwrite();
    test_lru_remove_and_clear();
    test_lru_hit_rate();
    test_lru_null_safety();

    PF_TEST_REPORT("缓存与内存预算");
}
