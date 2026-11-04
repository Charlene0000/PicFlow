/*
 * picflow_catalog.c
 * PicFlow Core —— 自研图库目录实现
 */

#include "picflow_catalog.h"
#include "picflow_palette.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* 标题词库：形容词 + 名词，确定性组合出可读标题 */
static const char *const kAdjectives[] = {
    "晨雾", "深蓝", "暖阳", "静谧", "斑驳", "薄暮", "清冽", "流光",
    "孤岛", "旷野", "琉璃", "潮汐", "山岚", "幽篁", "疏星", "暮色",
    "微光", "远山", "潮生", "拾光"
};

static const char *const kNouns[] = {
    "海岸", "巷陌", "天际", "原野", "松林", "湖面", "屋顶", "长街",
    "荒原", "峡谷", "渡口", "山脊", "河畔", "灯塔", "沙丘", "云海",
    "石桥", "旷野", "旧城", "竹径"
};

static const char *const kAuthorPrefix[] = {
    "青", "白", "墨", "青", "素", "丹", "朱", "玄"
};

static const char *const kAuthorSuffix[] = {
    "川", "山", "舟", "砚", "墨", "弦", "崖", "屿"
};

/* 标题与作者用静态缓冲区保存，避免动态内存 */
#define PF_TITLE_MAX 32
#define PF_AUTHOR_MAX 16

typedef struct {
    char    title[PF_TITLE_MAX];
    char    author[PF_AUTHOR_MAX];
} TextStore;

static TextStore g_text_store[PF_CATALOG_CAPACITY];

/*
 * 宽高比生成：
 *   覆盖 0.62 ~ 1.78，模拟真实图库的长图/方图/宽图混合。
 *   用低差异序列（Van der Corput，base 2）取模，使相邻条目的比例差异明显但不杂乱。
 *
 *   Van der Corput 的构造：把 n 的二进制位**逆序**后作为小数展开。
 *   注意不能用 "累加 1/2 + 1/4 + 1/8..." 的写法 —— 那样得到的值全部 >= 0.5，
 *   分布严重偏向高图，瀑布流就看不出高度差了。
 */
static double aspect_ratio_for(int index) {
    double value = 0.0;
    double inverse = 0.5;      /* 当前位的权重：1/2, 1/4, 1/8 ... */
    int n = index + 1;
    while (n > 0) {
        if (n & 1) {
            value += inverse;
        }
        n >>= 1;
        inverse *= 0.5;
    }
    /* 映射到 [0.62, 1.78] */
    return 0.62 + value * (1.78 - 0.62);
}

void pf_catalog_entry_at(int index, PFCatalogEntry *out_entry) {
    if (out_entry == NULL || index < 0 || index >= PF_CATALOG_CAPACITY) {
        return;
    }

    out_entry->id = index;
    out_entry->seed = (unsigned int)(index + 1) * 2654435761u;
    out_entry->aspect_ratio = aspect_ratio_for(index);
    out_entry->palette_index = index % 6;

    /* 生成标题与作者（只生成一次并缓存） */
    TextStore *store = &g_text_store[index];
    if (store->title[0] == '\0') {
        int adj = (int)(pf_random_unit(out_entry->seed, 11) * 20.0) % 20;
        int noun = (int)(pf_random_unit(out_entry->seed, 12) * 20.0) % 20;
        int pa = (int)(pf_random_unit(out_entry->seed, 13) * 8.0) % 8;
        int ps = (int)(pf_random_unit(out_entry->seed, 14) * 8.0) % 8;

        snprintf(store->title, PF_TITLE_MAX, "%s%s", kAdjectives[adj], kNouns[noun]);
        snprintf(store->author, PF_AUTHOR_MAX, "%s%s", kAuthorPrefix[pa], kAuthorSuffix[ps]);
    }

    out_entry->title  = store->title;
    out_entry->author = store->author;
}

int pf_catalog_count(void) {
    return PF_CATALOG_CAPACITY;
}

int pf_catalog_build(PFCatalogEntry *out_entries, int capacity) {
    if (out_entries == NULL || capacity <= 0) {
        return 0;
    }
    int count = (capacity < PF_CATALOG_CAPACITY) ? capacity : PF_CATALOG_CAPACITY;
    for (int i = 0; i < count; i++) {
        pf_catalog_entry_at(i, &out_entries[i]);
    }
    return count;
}

void pf_catalog_thumbnail_request(int index, int target_width, PFThumbnailRequest *out) {
    if (out == NULL) {
        return;
    }
    PFCatalogEntry entry;
    pf_catalog_entry_at(index, &entry);

    int width = (target_width > 0) ? target_width : 320;

    out->seed = entry.seed;
    out->pixel_width = width;
    out->pixel_height = (int)((double)width * entry.aspect_ratio);
    if (out->pixel_height < 1) {
        out->pixel_height = 1;
    }
    out->palette_index = entry.palette_index;
}

void pf_catalog_cache_key(int index, int target_width, char *buffer, int buffer_size) {
    if (buffer == NULL || buffer_size <= 0) {
        return;
    }
    buffer[0] = '\0';
    snprintf(buffer, (size_t)buffer_size, "pf-art-%d-%d", index, target_width);
}
