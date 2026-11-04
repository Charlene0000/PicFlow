/*
 * picflow_catalog.h
 * PicFlow Core —— 自研图库目录（纯 C）
 *
 * 设计要点：
 *   项目完全不使用外部图片 API。图库由「确定性程序生成」：
 *   每个条目 = (seed, 宽高比, 标题, 作者)，图片本身由 iOS 端
 *   用 Core Graphics 按 PFArtworkSpec 实时绘制。
 *
 *   好处：
 *   - 零网络依赖、零 API Key、离线可跑、CI 可复现；
 *   - 无第三方素材版权风险；
 *   - 宽高比由算法生成，覆盖 0.6~1.8 的各种比例，
 *     正好用来验证瀑布流的「最短列优先」是否真的把高度差拉开。
 *
 *   目录数据在编译期由 pf_catalog_build() 生成到调用方提供的数组里，
 *   避免动态分配，便于在 iOS 端直接作为只读数据使用。
 */

#ifndef PICFLOW_CATALOG_H
#define PICFLOW_CATALOG_H

#ifdef __cplusplus
extern "C" {
#endif

#define PF_CATALOG_CAPACITY 64

typedef struct {
    int          id;           /* 稳定 ID，等于索引      */
    unsigned int seed;         /* 图案随机种子           */
    double       aspect_ratio; /* 高 / 宽                */
    int          palette_index;/* 调色板索引             */
    const char  *title;        /* 标题                   */
    const char  *author;       /* 作者                   */
} PFCatalogEntry;

/* 生成完整目录，返回条目数。 */
int pf_catalog_build(PFCatalogEntry *out_entries, int capacity);

/* 单条目的确定性生成（供按需查询） */
void pf_catalog_entry_at(int index, PFCatalogEntry *out_entry);

/* 总条目数 */
int pf_catalog_count(void);

/*
 * 生成一张缩略图的规格（宽 -> 规格）。
 * iOS 端用这个规格调用 Core Graphics 绘制，
 * 也用于计算瀑布流中该 item 的目标高度。
 */
typedef struct {
    unsigned int seed;
    int          pixel_width;
    int          pixel_height;
    int          palette_index;
} PFThumbnailRequest;

void pf_catalog_thumbnail_request(int index, int target_width, PFThumbnailRequest *out);

/* 缩略图缓存 key 的构造（iOS 端用它做 NSCache / 磁盘缓存的 key） */
void pf_catalog_cache_key(int index, int target_width, char *buffer, int buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* PICFLOW_CATALOG_H */
