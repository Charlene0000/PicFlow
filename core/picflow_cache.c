/*
 * picflow_cache.c
 * PicFlow Core —— 内存预算决策器 + LRU 缓存实现
 */

#include "picflow_cache.h"

#include <stdlib.h>
#include <string.h>

/* ================================================================== */
/* 1. 内存预算决策器                                                  */
/* ================================================================== */

#define PF_DEFAULT_BUDGET_BYTES     (60u * 1024u * 1024u)
#define PF_MAX_SAFETY_MARGIN        (10u * 1024u * 1024u)
#define PF_DEFAULT_MAX_CONCURRENT   6

void pf_memory_budget_init(PFMemoryBudget *budget, size_t budget_bytes) {
    if (budget == NULL) {
        return;
    }
    memset(budget, 0, sizeof(*budget));
    budget->memory_budget_bytes = (budget_bytes > 0) ? budget_bytes : PF_DEFAULT_BUDGET_BYTES;

    /*
     * 安全余量按预算的 1/6 计算（上限 10MB），而不是写死一个绝对值。
     * 写死绝对值会在小预算场景下把整个额度吃光 —— 例如 100KB 预算配 10MB 余量，
     * 可用空间直接归零。比例计算才能让预算语义在任何量级下都成立。
     */
    size_t margin = budget->memory_budget_bytes / 6;
    if (margin > PF_MAX_SAFETY_MARGIN) {
        margin = PF_MAX_SAFETY_MARGIN;
    }
    budget->safety_margin_bytes = margin;

    budget->max_concurrent      = PF_DEFAULT_MAX_CONCURRENT;
    budget->reserved_bytes      = 0;
    budget->active_downloads    = 0;
}

size_t pf_memory_budget_available(const PFMemoryBudget *budget) {
    if (budget == NULL) {
        return 0;
    }
    size_t limit = budget->memory_budget_bytes;
    if (budget->safety_margin_bytes < limit) {
        limit -= budget->safety_margin_bytes;
    } else {
        limit = 0;
    }
    if (budget->reserved_bytes >= limit) {
        return 0;
    }
    return limit - budget->reserved_bytes;
}

int pf_memory_budget_can_alloc(const PFMemoryBudget *budget, size_t cost_bytes) {
    if (budget == NULL) {
        return 0;
    }
    return pf_memory_budget_available(budget) >= cost_bytes ? 1 : 0;
}

void pf_memory_budget_reserve(PFMemoryBudget *budget, size_t cost_bytes) {
    if (budget == NULL) {
        return;
    }
    budget->reserved_bytes += cost_bytes;
}

void pf_memory_budget_release(PFMemoryBudget *budget, size_t cost_bytes) {
    if (budget == NULL) {
        return;
    }
    if (budget->reserved_bytes >= cost_bytes) {
        budget->reserved_bytes -= cost_bytes;
    } else {
        budget->reserved_bytes = 0;
    }
}

int pf_memory_budget_can_start_download(const PFMemoryBudget *budget) {
    if (budget == NULL) {
        return 0;
    }
    return budget->active_downloads < budget->max_concurrent ? 1 : 0;
}

void pf_memory_budget_download_started(PFMemoryBudget *budget) {
    if (budget == NULL) {
        return;
    }
    budget->active_downloads += 1;
}

void pf_memory_budget_download_finished(PFMemoryBudget *budget) {
    if (budget == NULL) {
        return;
    }
    if (budget->active_downloads > 0) {
        budget->active_downloads -= 1;
    }
}

/* ================================================================== */
/* 2. LRU 缓存                                                        */
/* ================================================================== */

typedef struct LruNode {
    char  *key;
    void  *value;
    size_t value_bytes;
    struct LruNode *prev;   /* 指向更接近队尾的节点 */
    struct LruNode *next;   /* 指向更接近队头的节点（最近使用） */
} LruNode;

struct PFLruCache {
    LruNode *head;      /* 最近使用 */
    LruNode *tail;      /* 最久未使用 */
    size_t   count;
    size_t   bytes;
    size_t   max_bytes;
    size_t   max_items;
    size_t   evictions;
    size_t   hits;
    size_t   accesses;
};

PFLruCache *pf_lru_create(size_t max_bytes, size_t max_items) {
    PFLruCache *cache = (PFLruCache *)calloc(1, sizeof(PFLruCache));
    if (cache == NULL) {
        return NULL;
    }
    cache->max_bytes = max_bytes;
    cache->max_items = max_items;
    cache->head = NULL;
    cache->tail = NULL;
    return cache;
}

static void lru_unlink(PFLruCache *cache, LruNode *node) {
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        cache->head = node->next;
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        cache->tail = node->prev;
    }
    node->prev = NULL;
    node->next = NULL;
}

static void lru_push_front(PFLruCache *cache, LruNode *node) {
    node->prev = NULL;
    node->next = cache->head;
    if (cache->head != NULL) {
        cache->head->prev = node;
    }
    cache->head = node;
    if (cache->tail == NULL) {
        cache->tail = node;
    }
}

static void lru_destroy_node(LruNode *node) {
    if (node == NULL) {
        return;
    }
    free(node->key);
    free(node->value);
    free(node);
}

static LruNode *lru_find(PFLruCache *cache, const char *key) {
    for (LruNode *node = cache->head; node != NULL; node = node->next) {
        if (strcmp(node->key, key) == 0) {
            return node;
        }
    }
    return NULL;
}

static void lru_evict_until_fit(PFLruCache *cache, size_t incoming_bytes) {
    while ((cache->max_bytes > 0 && cache->bytes + incoming_bytes > cache->max_bytes) ||
           (cache->max_items > 0 && cache->count + 1 > cache->max_items)) {
        LruNode *victim = cache->tail;
        if (victim == NULL) {
            break;
        }
        lru_unlink(cache, victim);
        cache->bytes -= victim->value_bytes;
        cache->count -= 1;
        cache->evictions += 1;
        lru_destroy_node(victim);
    }
}

void pf_lru_destroy(PFLruCache *cache) {
    if (cache == NULL) {
        return;
    }
    pf_lru_clear(cache);
    free(cache);
}

int pf_lru_put(PFLruCache *cache, const char *key, const void *value, size_t value_bytes) {
    if (cache == NULL || key == NULL || key[0] == '\0') {
        return 0;
    }
    if (value == NULL && value_bytes > 0) {
        return 0;
    }

    /* 已存在则覆盖 */
    LruNode *existing = lru_find(cache, key);
    if (existing != NULL) {
        void *copy = NULL;
        if (value_bytes > 0) {
            copy = malloc(value_bytes);
            if (copy == NULL) {
                return 0;
            }
            memcpy(copy, value, value_bytes);
        }
        cache->bytes -= existing->value_bytes;
        free(existing->value);
        existing->value = copy;
        existing->value_bytes = value_bytes;
        cache->bytes += value_bytes;
        /* 移到头部 */
        lru_unlink(cache, existing);
        lru_push_front(cache, existing);
        return 1;
    }

    /* 淘汰直到能放下 */
    lru_evict_until_fit(cache, value_bytes);

    LruNode *node = (LruNode *)calloc(1, sizeof(LruNode));
    if (node == NULL) {
        return 0;
    }
    size_t key_len = strlen(key);
    node->key = (char *)malloc(key_len + 1);
    if (node->key == NULL) {
        free(node);
        return 0;
    }
    memcpy(node->key, key, key_len + 1);

    if (value_bytes > 0) {
        node->value = malloc(value_bytes);
        if (node->value == NULL) {
            free(node->key);
            free(node);
            return 0;
        }
        memcpy(node->value, value, value_bytes);
    }
    node->value_bytes = value_bytes;

    lru_push_front(cache, node);
    cache->count += 1;
    cache->bytes += value_bytes;
    return 1;
}

int pf_lru_put_string(PFLruCache *cache, const char *key, const char *value) {
    if (value == NULL) {
        return 0;
    }
    /* 长度含终止符，保证取出后可直接当 C 字符串用 */
    return pf_lru_put(cache, key, value, strlen(value) + 1);
}

const char *pf_lru_get_string(PFLruCache *cache, const char *key) {
    return (const char *)pf_lru_get(cache, key);
}

const void *pf_lru_get(PFLruCache *cache, const char *key) {
    if (cache == NULL || key == NULL) {
        return NULL;
    }
    cache->accesses += 1;
    LruNode *node = lru_find(cache, key);
    if (node == NULL) {
        return NULL;
    }
    cache->hits += 1;
    lru_unlink(cache, node);
    lru_push_front(cache, node);
    return node->value;
}

int pf_lru_remove(PFLruCache *cache, const char *key) {
    if (cache == NULL || key == NULL) {
        return 0;
    }
    LruNode *node = lru_find(cache, key);
    if (node == NULL) {
        return 0;
    }
    lru_unlink(cache, node);
    cache->bytes -= node->value_bytes;
    cache->count -= 1;
    lru_destroy_node(node);
    return 1;
}

void pf_lru_clear(PFLruCache *cache) {
    if (cache == NULL) {
        return;
    }
    LruNode *node = cache->head;
    while (node != NULL) {
        LruNode *next = node->next;
        lru_destroy_node(node);
        node = next;
    }
    cache->head = NULL;
    cache->tail = NULL;
    cache->count = 0;
    cache->bytes = 0;
}

size_t pf_lru_count(const PFLruCache *cache) {
    return (cache == NULL) ? 0 : cache->count;
}

size_t pf_lru_bytes(const PFLruCache *cache) {
    return (cache == NULL) ? 0 : cache->bytes;
}

size_t pf_lru_evictions(const PFLruCache *cache) {
    return (cache == NULL) ? 0 : cache->evictions;
}

double pf_lru_hit_rate(const PFLruCache *cache) {
    if (cache == NULL || cache->accesses == 0) {
        return 0.0;
    }
    return (double)cache->hits / (double)cache->accesses;
}
