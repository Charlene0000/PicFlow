//
//  PFGalleryViewController.m
//  PicFlow
//

#import "PFGalleryViewController.h"
#import "PFPhotoDetailViewController.h"
#import "../Views/PFWaterfallLayout.h"
#import "../Views/PFPhotoCell.h"
#import "../ViewModels/PFGalleryViewModel.h"
#import "../Utils/PFLog.h"

static NSString * const kPFPhotoCellReuseID = @"PFPhotoCell";

@interface PFGalleryViewController () <UICollectionViewDelegate, UICollectionViewDataSource, PFWaterfallLayoutDelegate>

@property (nonatomic, strong) UICollectionView *collectionView;
@property (nonatomic, strong) PFWaterfallLayout *waterfallLayout;
@property (nonatomic, strong) PFGalleryViewModel *viewModel;
@property (nonatomic, strong) UIRefreshControl *refreshControl;
@property (nonatomic, strong) UIActivityIndicatorView *footerSpinner;

@end

@implementation PFGalleryViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    self.title = @"看图大师";
    self.view.backgroundColor = [UIColor colorWithWhite:0.97 alpha:1.0];

    [self setupNavigationBar];
    [self setupCollectionView];
    [self setupFooterSpinner];
    [self bindViewModel];

    [self.viewModel loadFirstPage];
}

#pragma mark - UI 构建

- (void)setupNavigationBar {
    UIBarButtonItem *sourceItem =
        [[UIBarButtonItem alloc] initWithTitle:@"本地"
                                         style:UIBarButtonItemStylePlain
                                        target:self
                                        action:@selector(toggleSource)];
    sourceItem.accessibilityIdentifier = @"sourceToggle";
    self.navigationItem.leftBarButtonItem = sourceItem;

    UIBarButtonItem *columnItem = [[UIBarButtonItem alloc] initWithTitle:@"换列"
                                                                     style:UIBarButtonItemStylePlain
                                                                    target:self
                                                                    action:@selector(toggleColumnCount)];
    self.navigationItem.rightBarButtonItem = columnItem;
}

- (void)setupCollectionView {
    self.waterfallLayout = [[PFWaterfallLayout alloc] init];
    self.waterfallLayout.delegate = self;
    // iPad 上用 3 列，iPhone 上 2 列 —— 通过 trait 判断实现多机型适配。
    self.waterfallLayout.columnCount = [self preferredColumnCount];

    self.collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                            collectionViewLayout:self.waterfallLayout];
    self.collectionView.backgroundColor = [UIColor colorWithWhite:0.97 alpha:1.0];
    self.collectionView.dataSource = self;
    self.collectionView.delegate = self;
    self.collectionView.alwaysBounceVertical = YES;
    self.collectionView.showsVerticalScrollIndicator = NO;

    // ★ 注册 Cell：reuseIdentifier 必须注册，否则 dequeue 时崩溃。
    [self.collectionView registerClass:[PFPhotoCell class]
            forCellWithReuseIdentifier:kPFPhotoCellReuseID];

    // 预估高度：告诉 UIKit 提前算好布局，减少首屏计算量。
    self.collectionView.estimatedItemSize = CGSizeMake(self.view.bounds.size.width / 2.0, 200.0);

    [self.view addSubview:self.collectionView];

    // ★ 安全区适配：collectionView 约束到 safeAreaLayoutGuide，
    // 这样刘海屏 / 灵动岛 / 底部 Home Indicator 都不会遮挡内容。
    UILayoutGuide *safeArea = self.view.safeAreaLayoutGuide;
    [self.collectionView.topAnchor constraintEqualToAnchor:safeArea.topAnchor].active = YES;
    [self.collectionView.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor].active = YES;
    [self.collectionView.trailingAnchor constraintEqualToAnchor:safeArea.trailingAnchor].active = YES;
    [self.collectionView.bottomAnchor constraintEqualToAnchor:safeArea.bottomAnchor].active = YES;

    // 下拉刷新。
    self.refreshControl = [[UIRefreshControl alloc] init];
    [self.refreshControl addTarget:self action:@selector(handleRefresh) forControlEvents:UIControlEventValueChanged];
    [self.collectionView addSubview:self.refreshControl];
}

- (void)setupFooterSpinner {
    self.footerSpinner = [[UIActivityIndicatorView alloc]
                          initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];
    self.footerSpinner.hidesWhenStopped = YES;
    self.footerSpinner.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.footerSpinner];
    [self.footerSpinner.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor].active = YES;
    [self.footerSpinner.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                                                   constant:-16.0].active = YES;
}

- (void)bindViewModel {
    self.viewModel = [[PFGalleryViewModel alloc] init];

    __weak typeof(self) weakSelf = self;
    self.viewModel.onDataChanged = ^(NSArray<PFPhotoItem *> *items) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        // 数据变化 -> 让布局重算并刷新列表。
        [strongSelf.waterfallLayout invalidateLayoutWithReset:YES];
        [strongSelf.collectionView reloadData];
    };

    self.viewModel.onStateChanged = ^(PFGalleryState state, NSString *errorMessage) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        switch (state) {
            case PFGalleryStateLoading:
            case PFGalleryStateLoadingMore:
                [strongSelf.footerSpinner startAnimating];
                break;
            case PFGalleryStateLoaded:
            case PFGalleryStateNoMoreData:
            case PFGalleryStateIdle:
                [strongSelf.footerSpinner stopAnimating];
                [strongSelf.refreshControl endRefreshing];
                break;
            case PFGalleryStateError:
                [strongSelf.footerSpinner stopAnimating];
                [strongSelf.refreshControl endRefreshing];
                [strongSelf showError:errorMessage];
                break;
        }
    };
}

#pragma mark - 交互

- (void)handleRefresh {
    PFLogInfo(@"用户下拉刷新");
    [self.viewModel refresh];
}

- (void)toggleColumnCount {
    NSInteger next = (self.waterfallLayout.columnCount + 1) % 4;
    if (next == 0) {
        next = 2;
    }
    self.waterfallLayout.columnCount = next;
    [self.waterfallLayout invalidateLayoutWithReset:YES];
    [self.collectionView reloadData];
}

/// 切换数据源：本地程序化生成 <-> 远程图库（NSURLSession）。
- (void)toggleSource {
    PFPhotoSource next = (self.viewModel.currentSource == PFPhotoSourceProgrammatic)
                       ? PFPhotoSourceRemote
                       : PFPhotoSourceProgrammatic;
    [self.viewModel switchSource:next];
    self.navigationItem.leftBarButtonItem.title = (next == PFPhotoSourceProgrammatic) ? @"本地" : @"远程";
    PFLogInfo(@"切换数据源 -> %@", (next == PFPhotoSourceProgrammatic) ? @"本地程序化" : @"远程图库");
}

- (void)showError:(NSString *)message {
    if (!message) {
        return;
    }
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"加载失败"
                                                                   message:message
                                                            preferredStyle:UIAlertControllerStyleAlert];
    __weak typeof(self) weakSelf = self;
    [alert addAction:[UIAlertAction actionWithTitle:@"重试"
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction *action) {
        [weakSelf.viewModel loadFirstPage];
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"取消" style:UIAlertActionStyleCancel handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
}

#pragma mark - 多机型适配

- (NSInteger)preferredColumnCount {
    // 用 traitCollection 判断设备宽度等级，而不是硬编码设备型号。
    UIUserInterfaceSizeClass horizontal = self.traitCollection.horizontalSizeClass;
    if (horizontal == UIUserInterfaceSizeClassRegular) {
        return 3;   // iPad
    }
    return 2;       // iPhone
}

/// 旋转 / 分屏时重新计算列数。
- (void)traitCollectionDidChange:(UITraitCollection *)previousTraitCollection {
    [super traitCollectionDidChange:previousTraitCollection];
    if (previousTraitCollection.horizontalSizeClass != self.traitCollection.horizontalSizeClass) {
        self.waterfallLayout.columnCount = [self preferredColumnCount];
        [self.waterfallLayout invalidateLayoutWithReset:YES];
        [self.collectionView reloadData];
    }
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    // 尺寸变化后必须让布局失效，否则列宽仍是旧的。
    [self.waterfallLayout invalidateLayoutWithReset:YES];
    [coordinator animateAlongsideTransition:nil completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.collectionView reloadData];
    }];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView *)collectionView {
    return 1;
}

- (NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section {
    return (NSInteger)self.viewModel.items.count;
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView
                  cellForItemAtIndexPath:(NSIndexPath *)indexPath {
    // ★ 复用机制核心：从复用队列取 cell，取不到时自动创建并注册过的类实例。
    PFPhotoCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:kPFPhotoCellReuseID
                                                                 forIndexPath:indexPath];
    PFPhotoItem *item = [self.viewModel itemAtIndexPath:indexPath];
    if (item) {
        [cell configureWithItem:item columnWidth:[self currentColumnWidth]];
    }
    return cell;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView *)collectionView
didSelectItemAtIndexPath:(NSIndexPath *)indexPath {
    PFPhotoItem *item = [self.viewModel itemAtIndexPath:indexPath];
    if (!item) {
        return;
    }
    PFPhotoDetailViewController *detail = [[PFPhotoDetailViewController alloc] initWithItem:item];
    [self.navigationController pushViewController:detail animated:YES];
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView {
    // 接近底部时预加载下一页，提升用户体验。
    CGFloat offsetY = scrollView.contentOffset.y;
    CGFloat threshold = scrollView.contentSize.height - scrollView.bounds.size.height - 300.0;
    if (offsetY > threshold && self.viewModel.state == PFGalleryStateLoaded) {
        [self.viewModel loadNextPage];
    }
}

#pragma mark - PFWaterfallLayoutDelegate

- (CGFloat)collectionView:(UICollectionView *)collectionView
                   layout:(PFWaterfallLayout *)layout
heightForItemAtIndexPath:(NSIndexPath *)indexPath
                itemWidth:(CGFloat)itemWidth {
    PFPhotoItem *item = [self.viewModel itemAtIndexPath:indexPath];
    if (!item) {
        return itemWidth;
    }
    // 图片高度 + 标题区高度(20)
    CGFloat imageHeight = [item estimatedHeightWithColumnWidth:itemWidth];
    return imageHeight + 20.0;
}

- (NSInteger)collectionView:(UICollectionView *)collectionView
                      layout:(PFWaterfallLayout *)layout
        numberOfColumnsInSection:(NSInteger)section {
    return [self preferredColumnCount];
}

#pragma mark - 列宽计算

- (CGFloat)currentColumnWidth {
    UIEdgeInsets insets = self.waterfallLayout.sectionInset;
    CGFloat usable = CGRectGetWidth(self.collectionView.bounds) - insets.left - insets.right;
    NSInteger columns = self.waterfallLayout.columnCount;
    CGFloat spacing = self.waterfallLayout.interitemSpacing * (columns - 1);
    return (usable - spacing) / columns;
}

@end
