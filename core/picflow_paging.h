/*
 * picflow_paging.h
 * PicFlow Core —— 分页加载状态机（纯 C）
 *
 * 为什么把分页逻辑做成 C：
 *   分页是典型的「业务规则密集、易出边界 bug」的代码
 *   （重复请求、页码错乱、加载更多与下拉刷新冲突、最后一页判定）。
 *   抽成纯 C 后可以用状态机穷举测试，
 *   ��在 Windows 上跑，而不必依赖 Xcode。
 *
 * 对应 iOS 端：PFGalleryViewModel 的状态与分页部分。
 */

#ifndef PICFLOW_PAGING_H
#define PICFLOW_PAGING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PF_PAGING_IDLE = 0,        /* 初始/空闲            */
    PF_PAGING_LOADING,         /* 首屏加载中            */
    PF_PAGING_LOADING_MORE,   /* 加载更多中            */
    PF_PAGING_LOADED,          /* 已加载完成（有更多）    */
    PF_PAGING_NO_MORE_DATA,    /* 没有更多数据          */
    PF_PAGING_ERROR            /* 加载失败              */
} PFPagingState;

typedef struct {
    PFPagingState state;
    int page;              /* 当前已成功加载的页码，从 1 开始 */
    int page_size;         /* 每页条数                    */
    int max_pages;         /* 最大页数（<=0 表示不限）       */
    int has_more;          /* 是否还有下一页               */
    int request_in_flight; /* 是否有请求正在进行（防并发）   */
    int item_count;        /* 当前累计 item 数             */
    int last_error_code;   /* 最近一次错误码               */
} PFPagingContext;

/* 初始化。page_size <= 0 会被修正为 1。 */
void pf_paging_init(PFPagingContext *ctx, int page_size, int max_pages);

/*
 * 请求是否应该被发起。
 *
 * 返回 1 = 应该发起；0 = 应该拒绝。
 * 拒绝条件：
 *   - 已有请求在进行中（防止连点 / 滚动抖动导致的重复请求）
 *   - 处于错误状态且不是 reset（需要先重置）
 *   - 已无更多数据且不是 reset
 */
int pf_paging_should_request(const PFPagingContext *ctx, int reset);

/*
 * 标记请求开始，返回本次应请求的页码（reset 时为 1，否则为当前页 + 1）。
 * 调用前应先用 pf_paging_should_request 判断。
 */
int pf_paging_begin_request(PFPagingContext *ctx, int reset);

/*
 * 请求成功回调。
 * @param added 本次新增的条数
 * @param total_available 服务端返回的总数（<=0 表示未知）
 *
 * 返回 1 = 还有更多；0 = 已到末尾
 */
int pf_paging_succeed(PFPagingContext *ctx, int added, int total_available);

/* 请求失败 */
void pf_paging_fail(PFPagingContext *ctx, int error_code);

/* 是否处于「正在加载」状态（加载中/加载更多中） */
int pf_paging_is_loading(const PFPagingContext *ctx);

/* 状态转字符串，便于日志输出与测试断言 */
const char *pf_paging_state_name(PFPagingState state);

#ifdef __cplusplus
}
#endif

#endif /* PICFLOW_PAGING_H */
