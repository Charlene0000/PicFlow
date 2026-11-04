//
//  PFPhotoCell.h
//  PicFlow
//
//  瀑布流 Cell。
//  复用相关的三个关键实现点（面试高频追问）：
//  1. prepareForReuse 中重置所有 UI 状态 + 取消上一次的图片请求；
//  2. 用 representedID 记录当前 cell 绑定的数据 ID，回调时校验，
//     防止「快速滚动时图片错位」（A 的图显示到 B 的 cell 上）；
//  3. 图片容器固定宽高比，用 aspect ratio 约束，避免布局二次抖动。
//

#import <UIKit/UIKit.h>
#import "../Models/PFPhotoItem.h"

NS_ASSUME_NONNULL_BEGIN

@interface PFPhotoCell : UICollectionViewCell

/// 绑定的数据 ID，用于异步回调校验。
@property (nonatomic, copy, nullable) NSString *representedID;

/// 配置 cell。
- (void)configureWithItem:(PFPhotoItem *)item
                columnWidth:(CGFloat)columnWidth;

@end

NS_ASSUME_NONNULL_END
