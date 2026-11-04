//
//  PFGalleryViewModel.m
//  PicFlow
//

#import "PFGalleryViewModel.h"
#import "../Network/PFNetworkManager.h"
#import "../Utils/PFLog.h"

static NSString * const kPFGalleryListURL = @"https://picsum.photos/v2/list";
static const NSInteger kPFGalleryPageSize = 30;
static const NSInteger kPFGalleryMaxPages = 5;   // 示例数据源共约 1000 条，演示时只取前 5 页

@interface PFGalleryViewModel ()
@property (nonatomic, copy, readwrite) NSArray<PFPhotoItem *> *items;
@property (nonatomic, assign, readwrite) PFGalleryState state;
@property (nonatomic, copy, readwrite, nullable) NSString *errorMessage;
@property (nonatomic, assign) NSInteger currentPage;
@property (nonatomic, assign) BOOL hasMoreData;
/// 防止并发重复请求。
@property (nonatomic, assign) BOOL isRequesting;
@end

@implementation PFGalleryViewModel

- (instancetype)init {
    self = [super init];
    if (self) {
        _items = @[];
        _state = PFGalleryStateIdle;
        _currentPage = 0;
        _hasMoreData = YES;
    }
    return self;
}

#pragma mark - 加载

- (void)loadFirstPage {
    [self requestPage:1 reset:YES];
}

- (void)loadNextPage {
    if (!self.hasMoreData || self.state == PFGalleryStateLoadingMore) {
        return;
    }
    [self requestPage:self.currentPage + 1 reset:NO];
}

- (void)refresh {
    [self requestPage:1 reset:YES];
}

- (void)requestPage:(NSInteger)page reset:(BOOL)reset {
    if (self.isRequesting) {
        PFLogDebug(@"已有请求进行中，忽略 page=%ld", (long)page);
        return;
    }
    self.isRequesting = YES;
    self.errorMessage = nil;
    [self notifyState:reset ? PFGalleryStateLoading : PFGalleryStateLoadingMore error:nil];

    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    params[@"page"] = @(page);
    params[@"limit"] = @(kPFGalleryPageSize);

    __weak typeof(self) weakSelf = self;
    [[PFNetworkManager sharedManager] GET:kPFGalleryListURL
                               parameters:params
                                  success:^(id jsonObject) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        strongSelf.isRequesting = false;

        if (![jsonObject isKindOfClass:[NSArray class]]) {
            NSString *msg = @"数据格式异常";
            strongSelf.errorMessage = msg;
            [strongSelf notifyState:PFGalleryStateError error:msg];
            return;
        }

        NSMutableArray<PFPhotoItem *> *newItems = [NSMutableArray array];
        for (NSDictionary *dict in (NSArray *)jsonObject) {
            PFPhotoItem *item = [PFPhotoItem itemWithDictionary:dict];
            if (item) {
                [newItems addObject:item];
            }
        }

        strongSelf.currentPage = page;
        strongSelf.hasMoreData = (newItems.count > 0) && (page < kPFGalleryMaxPages);

        if (reset) {
            strongSelf.items = [newItems copy];
        } else {
            strongSelf.items = [strongSelf.items arrayByAddingObjectsFromArray:newItems];
        }

        PFLogInfo(@"第 %ld 页加载完成，累计 %lu 张", (long)page, (unsigned long)strongSelf.items.count);

        if (strongSelf.onDataChanged) {
            strongSelf.onDataChanged(strongSelf.items);
        }
        [strongSelf notifyState:strongSelf.hasMoreData ? PFGalleryStateLoaded : PFGalleryStateNoMoreData error:nil];
    }
                                failure:^(NSError *error) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        strongSelf.isRequesting = false;
        strongSelf.errorMessage = error.localizedDescription;
        [strongSelf notifyState:PFGalleryStateError error:error.localizedDescription];
    }];
}

#pragma mark - 数据访问

- (PFPhotoItem *)itemAtIndexPath:(NSIndexPath *)indexPath {
    if (indexPath.section != 0) {
        return nil;
    }
    // 越界保护：数据还没回来时 indexPath 可能超出当前 items 范围。
    if (indexPath.item < 0 || indexPath.item >= (NSInteger)self.items.count) {
        PFLogWarning(@"indexPath 越界 item=%ld count=%lu", (long)indexPath.item, (unsigned long)self.items.count);
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
