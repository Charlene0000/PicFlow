//
//  PFGalleryViewModel.h
//  PicFlow
//
//  MVVM 中的 ViewModel 层。
//  职责边界（面试常问）：
//  - 持有业务状态与分页逻辑，不 import 任何 UIKit 视图类；
//  - 通过 block 回调把状态变化通知给 Controller；
//  - 网络请求的触发、错误提示文案、是否还有更多数据，都在这里决定。
//

#import <Foundation/Foundation.h>
#import "../Models/PFPhotoItem.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, PFGalleryState) {
    /// 初始/空闲。
    PFGalleryStateIdle = 0,
    /// 首屏加载中。
    PFGalleryStateLoading,
    /// 加载更多中。
    PFGalleryStateLoadingMore,
    /// 已加载完成（有数据）。
    PFGalleryStateLoaded,
    /// 没有更多数据。
    PFGalleryStateNoMoreData,
    /// 加载失败。
    PFGalleryStateError,
};

@interface PFGalleryViewModel : NSObject

/// 当前已加载的所有图片。
@property (nonatomic, copy, readonly) NSArray<PFPhotoItem *> *items;
/// 当前状态。
@property (nonatomic, assign, readonly) PFGalleryState state;
/// 失败时的错误信息。
@property (nonatomic, copy, readonly, nullable) NSString *errorMessage;

/// 状态变化回调。参数：状态、错误信息。
@property (nonatomic, copy, nullable) void (^onStateChanged)(PFGalleryState state, NSString * _Nullable errorMessage);
/// 数据变化回调（列表需要 reload）。
@property (nonatomic, copy, nullable) void (^onDataChanged)(NSArray<PFPhotoItem *> *items);

/// 加载首屏。
- (void)loadFirstPage;
/// 加载下一页。
- (void)loadNextPage;
/// 下拉刷新（重置分页）。
- (void)refresh;

/// 切换数据来源：本地程序化生成 / 远程图库（NSURLSession）。
- (void)switchSource:(PFPhotoSource)source;
/// 当前数据来源。
- (PFPhotoSource)currentSource;

/// 指定 indexPath 的 item。
- (nullable PFPhotoItem *)itemAtIndexPath:(NSIndexPath *)indexPath;

@end

NS_ASSUME_NONNULL_END
