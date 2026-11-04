/*
 * picflow_cache.h
 * PicFlow Core —— 内存预算与 LRU 缓存（纯 C）
 *
 * 为什么 iOS 端已经有 NSCache，还要写一个纯 C 的 LRU？
 *
 * 真实场景里有两层缓存，职责不同：
 *   1. UIImage 的像素缓存 —— 必须用 NSCache（要能存 Objective-C 对象，
 *      且要跟随内存警告自动清理）；
 *   2. 「解码预算 + 预取窗口」的调度决策 —— 纯数值计算，不需要存对象。
 *      这一层决定：当前内存占用还能不能再解码一张大图？
 *      该不该为还没出现的 cell 提前发起请求？
 *
 * 把第 2 层做成纯 C 的好处：
 *   - 可以写穷举测试验证「任何时刻都不会超出预算」；
 *   - iOS 端在主线程上做这个决策时，零堆分配、零 ObjC 消息派发；
 *   - 与 UIKit 解耦，可单独压测。
 */

#ifndef PICFLOW_CACHE_H
#define PICFLOW_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* ------------------------------------------------------------------ */
/* 1. 内存预算决策器                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    size_t memory_budget_bytes; /* 总内存预算，例如 60MB */
    size_t reserved_bytes;      /* 已占用（已缓存图片的估算字节数） */
    size_t safety_margin_bytes; /* 安全余量，预留一部分给系统 */
    int    max_concurrent;      /* 允许的最大并发下载数 */
    int    active_downloads;    /* 当前进行中的下载数 */
} PFMemoryBudget;

/* 初始化预算：budget = 60MB，safety_margin = 10MB，max_concurrent = 6 */
void pf_memory_budget_init(PFMemoryBudget *budget, size_t budget_bytes);

/* 剩余可用字节数 */
size_t pf_memory_budget_available(const PFMemoryBudget *budget);

/* 是否还有余量解码一张估算大小为 cost_bytes 的图片 */
int pf_memory_budget_can_alloc(const PFMemoryBudget *budget, size_t cost_bytes);

/* 记一笔占用 */
void pf_memory_budget_reserve(PFMemoryBudget *budget, size_t cost_bytes);

/* 释放一笔占用（图片被淘汰） */
void pf_memory_budget_release(PFMemoryBudget *budget, size_t cost_bytes);

/* 是否可以再发起一个并发下载 */
int pf_memory_budget_can_start_download(const PFMemoryBudget *budget);

/* 标记一个下载开始 / 结束 */
void pf_memory_budget_download_started(PFMemoryBudget *budget);
void pf_memory_budget_download_finished(PFMemoryBudget *budget);

/* ------------------------------------------------------------------ */
/* 2. LRU 缓存（用于缩略图字节数据等非对象内容）                        */
/* ------------------------------------------------------------------ */

typedef struct PFLruCache PFLruCache;

/*
 * 创建 LRU 缓存。
 * max_bytes <= 0 表示不限制字节；max_items <= 0 表示不限制条目数。
 */
PFLruCache *pf_lru_create(size_t max_bytes, size_t max_items);

/* 销毁 */
void pf_lru_destroy(PFLruCache *cache);

/*
 * 写入。value 为二进制数据指针，cache 会**复制**一份。
 * 相同 key 会覆盖旧值并更新其成本。
 * 注意：value 按**二进制**语义存储，不保证以 '\0' 结尾。
 *       若要存字符串，请用 pf_lru_put_string。
 * @return 1 成功，0 失败（内存不足或参数非法）
 */
int pf_lru_put(PFLruCache *cache, const char *key, const void *value, size_t value_bytes);

/*
 * 写入字符串（自动追加 '\0'，长度含终止符）。
 * @return 1 成功，0 失败
 */
int pf_lru_put_string(PFLruCache *cache, const char *key, const char *value);

/*
 * 读取字符串。
 * @return 命中返回以 '\0' 结尾的字符串指针，未命中返回 NULL
 */
const char *pf_lru_get_string(PFLruCache *cache, const char *key);

/*
 * 读取。命中时会把节点移到链表头部（标记为最近使用）。
 * @return 命中返回数据指针（内部持有），未命中返回 NULL
 */
const void *pf_lru_get(PFLruCache *cache, const char *key);

/* 移除单个 key，@return 是否确实移除了 */
int pf_lru_remove(PFLruCache *cache, const char *key);

/* 清空 */
void pf_lru_clear(PFLruCache *cache);

/* 当前条目数 */
size_t pf_lru_count(const PFLruCache *cache);

/* 当前占用字节数 */
size_t pf_lru_bytes(const PFLruCache *cache);

/* 累计淘汰次数（用于观察缓存命中率趋势） */
size_t pf_lru_evictions(const PFLruCache *cache);

/* 命中率：命中数 / 总访问数 */
double pf_lru_hit_rate(const PFLruCache *cache);

#ifdef __cplusplus
}
#endif

#endif /* PICFLOW_CACHE_H */
