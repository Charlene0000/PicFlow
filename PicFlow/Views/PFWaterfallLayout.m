//
//  PFWaterfallLayout.m
//  PicFlow
//

#import "PFWaterfallLayout.h"

@interface PFWaterfallLayout ()
/// 缓存所有 item 的 layoutAttributes，避免每次滑动都重新算（性能关键）。
@property (nonatomic, strong) NSMutableArray<UICollectionViewLayoutAttributes *> *attributesCache;
/// 每一列当前的最低 Y 值，用来决定下一个 item 放哪一列（最短列优先）。
@property (nonatomic, strong) NSMutableArray<NSNumber *> *columnHeights;
@property (nonatomic, assign) CGSize calculatedContentSize;
@end

@implementation PFWaterfallLayout

- (instancetype)init {
    self = [super init];
    if (self) {
        _columnCount = 2;
        _lineSpacing = 8.0;
        _interitemSpacing = 8.0;
        _sectionInset = UIEdgeInsetsMake(8, 8, 8, 8);
        _headerHeight = 0.0;
        _attributesCache = [NSMutableArray array];
        _columnHeights = [NSMutableArray array];
    }
    return self;
}

#pragma mark - 必须实现的四个方法

- (void)prepareLayout {
    [super prepareLayout];

    UICollectionView *collectionView = self.collectionView;
    if (!collectionView) {
        return;
    }

    [self.attributesCache removeAllObjects];
    [self.columnHeights removeAllObjects];

    NSInteger sectionCount = collectionView.numberOfSections;
    NSInteger columnCount = [self resolvedColumnCountForSection:0];
    CGFloat contentWidth = CGRectGetWidth(collectionView.bounds);
    UIEdgeInsets insets = [self resolvedInsetForSection:0];

    // 顶部预留：安全区由 collectionView.contentInset 处理，这里只处理自定义 header 高度。
    CGFloat top = insets.top + self.headerHeight;
    for (NSInteger column = 0; column < columnCount; column++) {
        [self.columnHeights addObject:@(top)];
    }

    CGFloat usableWidth = contentWidth - insets.left - insets.right;
    CGFloat totalSpacing = self.interitemSpacing * (columnCount - 1);
    CGFloat itemWidth = (usableWidth - totalSpacing) / columnCount;

    for (NSInteger section = 0; section < sectionCount; section++) {
        NSInteger itemCount = [collectionView numberOfItemsInSection:section];
        if (section > 0) {
            // 多 section 时重新按当前 section 的 inset / 列数起算。
            UIEdgeInsets sectionInsets = [self resolvedInsetForSection:section];
            CGFloat sectionTop = [self lastColumnHeight] + sectionInsets.top;
            [self.columnHeights removeAllObjects];
            for (NSInteger column = 0; column < columnCount; column++) {
                [self.columnHeights addObject:@(sectionTop)];
            }
        }

        for (NSInteger item = 0; item < itemCount; item++) {
            NSIndexPath *indexPath = [NSIndexPath indexPathForItem:item inSection:section];

            // 最短列优先 —— 瀑布流的核心排布策略。
            NSInteger targetColumn = [self shortestColumnIndex];

            CGFloat itemHeight = [self heightForIndexPath:indexPath itemWidth:itemWidth];

            UIEdgeInsets sectionInsets = [self resolvedInsetForSection:section];
            CGFloat x = sectionInsets.left + targetColumn * (itemWidth + self.interitemSpacing);
            CGFloat y = [self.columnHeights[targetColumn] doubleValue];

            UICollectionViewLayoutAttributes *attrs =
            [UICollectionViewLayoutAttributes layoutAttributesForCellWithIndexPath:indexPath];
            attrs.frame = CGRectMake(x, y, itemWidth, itemHeight);

            [self.attributesCache addObject:attrs];

            // 更新该列的最低 Y 值。
            self.columnHeights[targetColumn] = @(CGRectGetMaxY(attrs.frame) + self.lineSpacing);
        }
    }

    CGFloat bottom = [self lastColumnHeight] + insets.bottom;
    self.calculatedContentSize = CGSizeMake(contentWidth, MAX(bottom, contentWidth > 0 ? 1 : 0));

    PFLogDebug(@"瀑布流布局完成 items=%lu contentHeight=%.1f",
               (unsigned long)self.attributesCache.count, self.calculatedContentSize.height);
}

- (CGSize)collectionViewContentSize {
    return self.calculatedContentSize;
}

- (NSArray<UICollectionViewLayoutAttributes *> *)layoutAttributesForElementsInRect:(CGRect)rect {
    // 只返回与可视区域相交的属性，避免把整屏 item 都交给 UIKit 处理。
    NSMutableArray<UICollectionViewLayoutAttributes *> *result = [NSMutableArray array];
    for (UICollectionViewLayoutAttributes *attrs in self.attributesCache) {
        if (CGRectIntersectsRect(attrs.frame, rect)) {
            [result addObject:attrs];
        }
    }
    return result;
}

- (UICollectionViewLayoutAttributes *)layoutAttributesForItemAtIndexPath:(NSIndexPath *)indexPath {
    NSInteger index = indexPath.item;
    if (index < 0 || index >= (NSInteger)self.attributesCache.count) {
        return nil;
    }
    return self.attributesCache[index];
}

#pragma mark - 失效与刷新

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds {
    // 宽度变化（旋转 / 分屏 / 折叠屏展开）必须重算列宽，否则 item 宽度会错位。
    // 高度变化（纯滚动）不重算，靠 layoutAttributesForElementsInRect 增量取即可。
    return CGRectGetWidth(newBounds) != CGRectGetWidth(self.collectionView.bounds);
}

- (void)invalidateLayoutWithContext:(UICollectionViewLayoutInvalidationContext *)context {
    // invalidateLayoutWithContext: 之后会重新走 prepareLayout，
    // 但 attributesCache 需要在这里清空，否则旧数据会累积。
    if (context.invalidateEverything || context.invalidateDataSourceCounts) {
        [self.attributesCache removeAllObjects];
        [self.columnHeights removeAllObjects];
    }
    [super invalidateLayoutWithContext:context];
}

- (void)invalidateLayoutWithReset:(BOOL)reset {
    if (reset) {
        [self.attributesCache removeAllObjects];
        [self.columnHeights removeAllObjects];
    }
    [self invalidateLayout];
}

#pragma mark - 内部计算

- (NSInteger)resolvedColumnCountForSection:(NSInteger)section {
    if ([self.delegate respondsToSelector:@selector(collectionView:layout:numberOfColumnsInSection:)]) {
        return MAX(1, [self.delegate collectionView:self.collectionView layout:self numberOfColumnsInSection:section]);
    }
    return MAX(1, self.columnCount);
}

- (CGFloat)resolvedLineSpacingForSection:(NSInteger)section {
    if ([self.delegate respondsToSelector:@selector(collectionView:layout:minimumLineSpacingForSection:)]) {
        return [self.delegate collectionView:self.collectionView layout:self minimumLineSpacingForSection:section];
    }
    return self.lineSpacing;
}

- (CGFloat)resolvedInteritemSpacingForSection:(NSInteger)section {
    if ([self.delegate respondsToSelector:@selector(collectionView:layout:minimumInteritemSpacingForSection:)]) {
        return [self.delegate collectionView:self.collectionView layout:self minimumInteritemSpacingForSection:section];
    }
    return self.interitemSpacing;
}

- (UIEdgeInsets)resolvedInsetForSection:(NSInteger)section {
    if ([self.delegate respondsToSelector:@selector(collectionView:layout:insetForSectionAtIndex:)]) {
        return [self.delegate collectionView:self.collectionView layout:self insetForSectionAtIndex:section];
    }
    return self.sectionInset;
}

- (CGFloat)heightForIndexPath:(NSIndexPath *)indexPath itemWidth:(CGFloat)itemWidth {
    if ([self.delegate respondsToSelector:@selector(collectionView:layout:heightForItemAtIndexPath:itemWidth:)]) {
        return [self.delegate collectionView:self.collectionView layout:self
                heightForItemAtIndexPath:indexPath itemWidth:itemWidth];
    }
    return itemWidth;
}

- (NSInteger)shortestColumnIndex {
    NSInteger shortest = 0;
    CGFloat minHeight = CGFLOAT_MAX;
    for (NSInteger i = 0; i < (NSInteger)self.columnHeights.count; i++) {
        CGFloat height = [self.columnHeights[i] doubleValue];
        if (height < minHeight) {
            minHeight = height;
            shortest = i;
        }
    }
    return shortest;
}

- (CGFloat)lastColumnHeight {
    CGFloat maxHeight = 0;
    for (NSNumber *height in self.columnHeights) {
        maxHeight = MAX(maxHeight, height.doubleValue);
    }
    return maxHeight;
}

@end
