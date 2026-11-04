//
//  PFWaterfallLayout.h
//  PicFlow
//
//  自定义瀑布流布局。
//  为什么不直接用 UICollectionViewFlowLayout？
//  FlowLayout 的 itemSize 是全局唯一的，没法做「每个 item 高度不同」的瀑布流；
//  而瀑布流要求每个 item 保留原始宽高比，高度必须在拿到图片尺寸后才知道。
//  所以这里实现 UICollectionViewLayout 的子类，自己管理 attributes 与列高。
//
//  核心 API（UICollectionViewLayout 要求的四个必须实现的方法）：
//  - prepareLayout            : 计算所有 item 的 frame
//  - collectionViewContentSize: 内容总高度
//  - layoutAttributesForElementsInRect: 取可视区内的 attributes
//  - layoutAttributesForItemAtIndexPath: 取单个 item 的 attributes
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@class PFWaterfallLayout;

@protocol PFWaterfallLayoutDelegate <NSObject>

@required
/// 返回 indexPath 对应 item 的高度。列宽由布局计算后传入。
- (CGFloat)collectionView:(UICollectionView *)collectionView
                   layout:(PFWaterfallLayout *)layout
heightForItemAtIndexPath:(NSIndexPath *)indexPath
                itemWidth:(CGFloat)itemWidth;

@optional
/// 列数，默认 2。
- (NSInteger)collectionView:(UICollectionView *)collectionView
                      layout:(PFWaterfallLayout *)layout
        numberOfColumnsInSection:(NSInteger)section;

/// 行间距，默认 8。
- (CGFloat)collectionView:(UICollectionView *)collectionView
                   layout:(PFWaterfallLayout *)layout
   minimumLineSpacingForSection:(NSInteger)section;

/// 列间距，默认 8。
- (CGFloat)collectionView:(UICollectionView *)collectionView
                   layout:(PFWaterfallLayout *)layout
minimumInteritemSpacingForSection:(NSInteger)section;

/// 外边距，默认 (8, 8, 8, 8)。
- (UIEdgeInsets)collectionView:(UICollectionView *)collectionView
                         layout:(PFWaterfallLayout *)layout
             insetForSectionAtIndex:(NSInteger)section;

@end

@interface PFWaterfallLayout : UICollectionViewLayout

@property (nonatomic, weak, nullable) id<PFWaterfallLayoutDelegate> delegate;
/// 列数，未实现 delegate 方法时使用。
@property (nonatomic, assign) NSInteger columnCount;
/// 行间距。
@property (nonatomic, assign) CGFloat lineSpacing;
/// 列间距。
@property (nonatomic, assign) CGFloat interitemSpacing;
/// 外边距。
@property (nonatomic, assign) UIEdgeInsets sectionInset;
/// 单个 section 的顶部预留高度（用于放置吸顶筛选栏等）。
@property (nonatomic, assign) CGFloat headerHeight;

/// 强制重新计算布局（例如数据源变化后调用）。
- (void)invalidateLayoutWithReset:(BOOL)reset;

@end

NS_ASSUME_NONNULL_END
