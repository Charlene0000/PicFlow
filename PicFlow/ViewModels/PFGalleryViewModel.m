//
//  PFGalleryViewModel.m
//  PicFlow
//

#import "PFGalleryViewModel.h"
#import "../Network/PFNetworkManager.h"
#import "../Utils/PFLog.h"
#include "../../core/picflow_paging.h"
#include "../../core/picflow_catalog.h"

/// 远程图库地址（自建 mock server，见 tools/mock_server.py）。
/// 项目默认走本地程序化生成，这个地址仅在手动切换到远程模式时使用。
static NSString * const kPFGalleryRemoteURL = @"http://127.0.0.1:8765/catalog.json";
static const NSInteger kPFGalleryPageSize = 24;
static const NSInteger kPFGalleryMaxPages = 3;   // 自研图库共 64 条，演示时取前 3 页

@interface PFGalleryViewModel ()
@property (nonatomic, copy, readwrite) NSArray<PFPhotoItem *> *items;
@property (nonatomic, assign, readwrite) PFGalleryState state;
@property (nonatomic, copy, readwrite, nullable) NSString *errorMessage;
/// ★ C 层分页状态机：并发保护、页码递增、末页判定全部由它负责。
@property (nonatomic, assign) PFPagingContext paging;
/// 当前数据来源模式。
@property (nonatomic, assign) PFPhotoSource source;
@end

@implementation PFGalleryViewModel

- (instancetype)init {
    self = [super init];
    if (self) {
        _items = @[];
        _state = PFGalleryStateIdle;
        _source = PFPhotoSourceProgrammatic;
        pf_paging_init(&_paging, kPFGalleryPageSize, kPFGalleryMaxPages);
    }
    return self;
}

#pragma mark - 加载

- (void)loadFirstPage {
    [self requestReset:YES];
}

- (void)loadNextPage {
    [self requestReset:NO];
}

- (void)refresh {
    [self requestReset:YES];
}

/// 切换数据来源（本地程序化 / 远程图库）。
- (void)switchSource:(PFPhotoSource)source {
    if (source == self.source) {
        return;
    }
    self.source = source;
    [self refresh];
}

- (PFPhotoSource)currentSource {
    return self.source;
}

- (void)requestReset:(BOOL)reset {
    // C 状态机负责判断「该不该发请求」（并发保护、无更多数据、错误态）
    if (!pf_paging_should_request(&self.paging, reset)) {
        PFLogDebug(@"分页状态机拒绝请求 reset=%d state=%s",
                   reset, pf_paging_state_name(self.paging.state));
        return;
    }
    int page = pf_paging_begin_request(&self.paging, reset);
    if (page == 0) {
        return;
    }

    [self notifyState:reset ? PFGalleryStateLoading : PFGalleryStateLoadingMore error:nil];

    if (self.source == PFPhotoSourceProgrammatic) {
        [self loadProgrammaticPage:page reset:reset];
    } else {
        [self loadRemotePage:page reset:reset];
    }
}

#pragma mark - 本地程序化图库

- (void)loadProgrammaticPage:(int)page reset:(BOOL)reset {
    // 模拟一次异步加载（真实场景这里是磁盘 / 数据库读取）
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSInteger start = (page - 1) * kPFGalleryPageSize;
        NSMutableArray<PFPhotoItem *> *newItems = [NSMutableArray array];
        for (NSInteger i = 0; i < kPFGalleryPageSize; i++) {
            NSInteger index = start + i;
            if (index >= pf_catalog_count()) {
                break;
            }
            PFPhotoItem *item = [PFPhotoItem itemAtCatalogIndex:index];
            if (item) {
                [newItems addObject:item];
            }
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            [self applyLoadedItems:newItems reset:reset];
        });
    });
}

#pragma mark - 远程图库（NSURLSession）

- (void)loadRemotePage:(int)page reset:(BOOL)reset {
    __weak typeof(self) weakSelf = self;
    [[PFNetworkManager sharedManager] GET:kPFGalleryRemoteURL
                               parameters:@{ @"page": @(page), @"limit": @(kPFGalleryPageSize) }
                                  success:^(id jsonObject) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        if (![jsonObject isKindOfClass:[NSArray class]]) {
            NSString *msg = @"远程图库数据格式异常";
            strongSelf.errorMessage = msg;
            [strongSelf notifyState:PFGalleryStateError error:msg];
            pf_paging_fail(&strongSelf.paging, PFNetworkErrorCodeParseFailed);
            return;
        }
        NSMutableArray<PFPhotoItem *> *newItems = [NSMutableArray array];
        for (NSDictionary *dict in (NSArray *)jsonObject) {
            PFPhotoItem *item = [PFPhotoItem itemWithDictionary:dict];
            if (item) {
                [newItems addObject:item];
            }
        }
        [strongSelf applyLoadedItems:newItems reset:reset];
    }
                                  failure:^(NSError *error) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        strongSelf.errorMessage = error.localizedDescription;
        pf_paging_fail(&strongSelf.paging, error.code);
        [strongSelf notifyState:PFGalleryStateError error:error.localizedDescription];
    }];
}

#pragma mark - 应用结果

- (void)applyLoadedItems:(NSArray<PFPhotoItem *> *)newItems reset:(BOOL)reset {
    // C 状态机统一处理页码递增、条数累计、末页判定
    int hasMore = pf_paging_succeed(&self.paging, (int)newItems.count, 0);

    if (reset) {
        self.items = [newItems copy];
    } else {
        self.items = [self.items arrayByAddingObjectsFromArray:newItems];
    }

    PFLogInfo(@"第 %d 页加载完成，累计 %lu 张，hasMore=%d",
              self.paging.page, (unsigned long)self.items.count, hasMore);

    if (self.onDataChanged) {
        self.onDataChanged(self.items);
    }
    [self notifyState:hasMore ? PFGalleryStateLoaded : PFGalleryStateNoMoreData error:nil];
}

#pragma mark - 数据访问

- (PFPhotoItem *)itemAtIndexPath:(NSIndexPath *)indexPath {
    if (indexPath.section != 0) {
        return nil;
    }
    // 越界保护：数据还没回来时 indexPath 可能超出当前 items 范围。
    if (indexPath.item < 0 || indexPath.item >= (NSInteger)self.items.count) {
        PFLogWarning(@"indexPath 越界 item=%ld count=%lu",
                     (long)indexPath.item, (unsigned long)self.items.count);
        return nil;
    }
    return self.items[indexPath.item];
}

#pragma mark - 通知

- (void)notifyState:(PFGalleryState)state error:(nullable NSString *)error {
    self.state = state;
    if (self.onStateChanged) {
        self.onStateChanged(state, error);
    }
}

@end
