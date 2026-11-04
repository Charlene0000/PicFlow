/*
 * picflow_paging.c
 * PicFlow Core —— 分页加载状态机实现
 */

#include "picflow_paging.h"

#include <string.h>

void pf_paging_init(PFPagingContext *ctx, int page_size, int max_pages) {
    if (ctx == NULL) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->state       = PF_PAGING_IDLE;
    ctx->page        = 0;
    ctx->page_size   = (page_size > 0) ? page_size : 1;
    ctx->max_pages   = (max_pages > 0) ? max_pages : 0;
    ctx->has_more    = 1;
    ctx->item_count  = 0;
    ctx->request_in_flight = 0;
    ctx->last_error_code = 0;
}

int pf_paging_is_loading(const PFPagingContext *ctx) {
    if (ctx == NULL) {
        return 0;
    }
    return (ctx->state == PF_PAGING_LOADING || ctx->state == PF_PAGING_LOADING_MORE);
}

int pf_paging_should_request(const PFPagingContext *ctx, int reset) {
    if (ctx == NULL) {
        return 0;
    }
    /* 并发保护：任何时刻只允许一个请求在进行 */
    if (ctx->request_in_flight) {
        return 0;
    }
    if (reset) {
        return 1;
    }
    /* 无更多数据时拒绝（reset 可重新拉取） */
    if (!ctx->has_more || ctx->state == PF_PAGING_NO_MORE_DATA) {
        return 0;
    }
    /* 错误状态需要先 reset */
    if (ctx->state == PF_PAGING_ERROR) {
        return 0;
    }
    return 1;
}

int pf_paging_begin_request(PFPagingContext *ctx, int reset) {
    if (ctx == NULL || !pf_paging_should_request(ctx, reset)) {
        return 0;
    }
    ctx->request_in_flight = 1;
    ctx->last_error_code   = 0;
    if (reset) {
        ctx->state     = PF_PAGING_LOADING;
        ctx->page      = 0;
        ctx->item_count = 0;
        ctx->has_more  = 1;
    } else {
        ctx->state = PF_PAGING_LOADING_MORE;
    }
    return ctx->page + 1;
}

int pf_paging_succeed(PFPagingContext *ctx, int added, int total_available) {
    if (ctx == NULL) {
        return 0;
    }
    ctx->request_in_flight = 0;
    ctx->page += 1;
    if (added > 0) {
        ctx->item_count += added;
    }

    /* 判定是否还有更多：
     * 1. 本页一条都没拿到 -> 一定没有了
     * 2. 达到 max_pages -> 没有了
     * 3. 服务端告知总数且已达上限 -> 没有了
     * 4. 本页数量不足 page_size -> 通常是最后一页
     */
    int has_more = 1;
    if (added <= 0) {
        has_more = 0;
    } else if (ctx->max_pages > 0 && ctx->page >= ctx->max_pages) {
        has_more = 0;
    } else if (total_available > 0 && ctx->item_count >= total_available) {
        has_more = 0;
    } else if (added < ctx->page_size) {
        has_more = 0;
    }

    ctx->has_more = has_more;
    ctx->state = has_more ? PF_PAGING_LOADED : PF_PAGING_NO_MORE_DATA;
    return has_more;
}

void pf_paging_fail(PFPagingContext *ctx, int error_code) {
    if (ctx == NULL) {
        return;
    }
    ctx->request_in_flight = 0;
    ctx->last_error_code   = error_code;
    ctx->state             = PF_PAGING_ERROR;
    /* 注意：失败时保留 page 与 item_count，
     * 这样用户重试可以继续，而不是从头再来。 */
}

const char *pf_paging_state_name(PFPagingState state) {
    switch (state) {
        case PF_PAGING_IDLE:         return "IDLE";
        case PF_PAGING_LOADING:      return "LOADING";
        case PF_PAGING_LOADING_MORE: return "LOADING_MORE";
        case PF_PAGING_LOADED:       return "LOADED";
        case PF_PAGING_NO_MORE_DATA: return "NO_MORE_DATA";
        case PF_PAGING_ERROR:        return "ERROR";
        default:                     return "UNKNOWN";
    }
}
