//
//  PFWaterfallLayout.m
//  PicFlow
//
//  本文件是 **C 布局引擎的 Objective-C 适配层**：
//  所有排布计算都交给 core/picflow_layout.c 完成，
//  这里只负责 UIKit 概念与 C 概念之间的翻译。
//

#import "PFWaterfallLayout.h"
#import "../Utils/PFLog.h"
#include "../../core/picflow_layout.h"

@interface PFWaterfallLayout (PFCallbackAccess)
- (CGFloat)currentColumnWidth;
@end

static double PFHeightProviderCallback(void *context, int index) {
    PFWaterfallLayout *layout = (__bridge PFWaterfallLayout *)context;
    UICollectionView *cv = layout.collectionView;
    if (!cv || index < 0 || index >= (NSInteger)[cv numberOfItemsInSection:0]) {
        return 0.0;
    }
    NSIndexPath *indexPath = [NSIndexPath indexPathForItem:index inSection:0];
    CGFloat columnWidth = [layout currentColumnWidth];
    return (double)[layout.delegate collectionView:cv
                                            layout:layout
                          heightForItemAtIndexPath:indexPath
                                          itemWidth:columnWidth];
}

@interface PFWaterfallLayout ()
/// 缓存所有 item 的 layoutAttributes，避免每次滑动都重新算（性能关键）。
@property (nonatomic, strong) NSMutableArray<UICollectionViewLayoutAttributes *> *attributesCache;
/// C 引擎算出的 frame 结果。
@property (nonatomic, strong) NSMutableData *frameBuffer;
/// C 引擎返回的内容总尺寸。
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

    NSInteger sectionCount = collectionView.numberOfSections;
    NSInteger columnCount = [self resolvedColumnCountForSection:0];
    CGFloat contentWidth = CGRectGetWidth(collectionView.bounds);
    UIEdgeInsets insets = [self resolvedInsetForSection:0];

    // 顶部预留：安全区由 collectionView.contentInset 处理，这里只处理自定义 header 高度。
    CGFloat top = insets.top + self.headerHeight;

    // ---- 高度提供者：C 函数指针作为 context 传给 C 引擎 ----
    // C 引擎通过这个回调逐个询问高度，与 UICollectionViewLayoutDelegate 的语义一致。
    // 注意：这里必须使用 C 函数指针，而不是 Objective-C block。
    PFItemHeightProvider provider = PFHeightProviderCallback;

    // ---- 组装 C 引擎配置 ----
    PFWaterfallConfig config;
    config.column_count      = (int)columnCount;
    config.line_spacing      = (double)self.lineSpacing;
    config.interitem_spacing = (double)self.interitemSpacing;
    config.inset_top         = (double)insets.top;
    config.inset_left        = (double)insets.left;
    config.inset_bottom      = (double)insets.bottom;
    config.inset_right       = (double)insets.right;
    config.header_height     = (double)self.headerHeight;
    pf_waterfall_config_validate(&config);

    NSInteger itemCount = [collectionView numberOfItemsInSection:0];
    double contentHeight = 0.0;

    if (itemCount > 0) {
        // 分配 frame 缓冲区（C 结构体数组）
        NSUInteger byteSize = (NSUInteger)itemCount * sizeof(PFWaterfallFrame);
        self.frameBuffer = [NSMutableData dataWithLength:byteSize];
        PFWaterfallFrame *frames = (PFWaterfallFrame *)self.frameBuffer.mutableBytes;

        int rc = pf_waterfall_layout_compute(&config,
                                             (int)itemCount,
                                             (double)contentWidth,
                                             provider,
                                             (__bridge void *)self,
                                             frames,
                                             &contentHeight);
        if (rc != 0) {
            PFLogError(@"C 布局引擎计算失败 rc=%d", rc);
            return;
        }

        // ---- C 结果 -> UIKit attributes ----
        for (NSInteger i = 0; i < itemCount; i++) {
            PFWaterfallFrame frame = frames[i];
            NSIndexPath *indexPath = [NSIndexPath indexPathForItem:i inSection:0];
            UICollectionViewLayoutAttributes *attrs =
                [UICollectionViewLayoutAttributes layoutAttributesForCellWithIndexPath:indexPath];
            attrs.frame = CGRectMake(frame.x, frame.y, frame.width, frame.height);
            [self.attributesCache addObject:attrs];
        }
    }

    self.calculatedContentSize = CGSizeMake(contentWidth, MAX(contentHeight, 1.0));

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
    }
    [super invalidateLayoutWithContext:context];
}

- (void)invalidateLayoutWithReset:(BOOL)reset {
    if (reset) {
        [self.attributesCache removeAllObjects];
    }
    [self invalidateLayout];
}

#pragma mark - 内部计算

- (CGFloat)currentColumnWidth {
    UIEdgeInsets insets = self.sectionInset;
    CGFloat usable = CGRectGetWidth(self.collectionView.bounds) - insets.left - insets.right;
    NSInteger columns = self.columnCount;
    CGFloat spacing = self.interitemSpacing * (columns - 1);
    return (usable - spacing) / columns;
}

- (NSInteger)resolvedColumnCountForSection:(NSInteger)section {
    if ([self.delegate respondsToSelector:@selector(collectionView:layout:numberOfColumnsInSection:)]) {
        return MAX(1, [self.delegate collectionView:self.collectionView layout:self numberOfColumnsInSection:section]);
    }
    return MAX(1, self.columnCount);
}

- (UIEdgeInsets)resolvedInsetForSection:(NSInteger)section {
    if ([self.delegate respondsToSelector:@selector(collectionView:layout:insetForSectionAtIndex:)]) {
        return [self.delegate collectionView:self.collectionView layout:self insetForSectionAtIndex:section];
    }
    return self.sectionInset;
}

@end
