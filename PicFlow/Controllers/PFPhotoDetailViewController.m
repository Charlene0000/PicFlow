//
//  PFPhotoDetailViewController.m
//  PicFlow
//

#import "PFPhotoDetailViewController.h"
#import "../Views/PFArtworkRenderer.h"
#import "../Utils/PFImageLoader.h"
#import "../Utils/PFLog.h"

@interface PFPhotoDetailViewController ()
@property (nonatomic, strong) PFPhotoItem *item;
@property (nonatomic, strong) UIScrollView *scrollView;
@property (nonatomic, strong) UIImageView *imageView;
@property (nonatomic, strong) UILabel *infoLabel;
@property (nonatomic, strong) UIActivityIndicatorView *spinner;
@property (nonatomic, strong, nullable) id loadToken;
@end

@implementation PFPhotoDetailViewController

- (instancetype)initWithItem:(PFPhotoItem *)item {
    self = [super initWithNibName:nil bundle:nil];
    if (self) {
        _item = item;
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"图片详情";
    self.view.backgroundColor = [UIColor blackColor];

    [self setupScrollView];
    [self setupInfoLabel];
    [self loadImage];
}

#pragma mark - UI

- (void)setupScrollView {
    self.scrollView = [[UIScrollView alloc] init];
    self.scrollView.minimumZoomScale = 1.0;
    self.scrollView.maximumZoomScale = 3.0;
    self.scrollView.delegate = (id<UIScrollViewDelegate>)self;
    self.scrollView.showsHorizontalScrollIndicator = NO;
    self.scrollView.showsVerticalScrollIndicator = NO;
    [self.view addSubview:self.scrollView];

    self.imageView = [[UIImageView alloc] init];
    self.imageView.contentMode = UIViewContentModeScaleAspectFit;
    [self.scrollView addSubview:self.imageView];

    UILayoutGuide *safeArea = self.view.safeAreaLayoutGuide;
    [self.scrollView.topAnchor constraintEqualToAnchor:safeArea.topAnchor].active = YES;
    [self.scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor].active = YES;
    [self.scrollView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor].active = YES;
    [self.scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor].active = YES;

    [self.imageView.topAnchor constraintEqualToAnchor:self.scrollView.topAnchor].active = YES;
    [self.imageView.leadingAnchor constraintEqualToAnchor:self.scrollView.leadingAnchor].active = YES;
    [self.imageView.trailingAnchor constraintEqualToAnchor:self.scrollView.trailingAnchor].active = YES;
    [self.imageView.bottomAnchor constraintEqualToAnchor:self.scrollView.bottomAnchor].active = YES;

    self.spinner = [[UIActivityIndicatorView alloc]
                    initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleLarge];
    self.spinner.color = [UIColor whiteColor];
    [self.view addSubview:self.spinner];
    [self.spinner.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor].active = YES;
    [self.spinner.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor].active = YES;
}

- (void)setupInfoLabel {
    self.infoLabel = [[UILabel alloc] init];
    self.infoLabel.numberOfLines = 2;
    self.infoLabel.textColor = [UIColor colorWithWhite:0.8 alpha:1.0];
    self.infoLabel.font = [UIFont systemFontOfSize:12.0];
    self.infoLabel.textAlignment = NSTextAlignmentCenter;
    self.infoLabel.text = [NSString stringWithFormat:@"%@\n%@ × %@",
                           self.item.author,
                           @((int)self.item.pixelWidth), @((int)self.item.pixelHeight)];
    self.infoLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.infoLabel];

    [self.infoLabel.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                                                 constant:16.0].active = YES;
    [self.infoLabel.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                                                  constant:-16.0].active = YES;
    [self.infoLabel.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                                                constant:-12.0].active = YES;
}

#pragma mark - 加载

- (void)loadImage {
    [self.spinner startAnimating];

    // 详情页用更大的目标宽度渲染，保证清晰度
    PFPhotoItem *detailItem = [PFPhotoItem itemAtCatalogIndex:self.item.catalogIndex];
    if (!detailItem) {
        detailItem = self.item;
    }
    detailItem.pixelWidth = 1200.0;
    detailItem.pixelHeight = 1200.0 * [self.item aspectRatio];
    detailItem.source = self.item.source;
    detailItem.identifier = self.item.identifier;

    NSString *cacheKey = [NSString stringWithFormat:@"detail-%@", self.item.identifier];

    __weak typeof(self) weakSelf = self;
    self.loadToken = [[PFImageLoader sharedLoader] loadImageForItem:detailItem
                                                            cacheKey:cacheKey
                                                           completion:^(UIImage *image) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        [strongSelf.spinner stopAnimating];
        strongSelf.imageView.image = image;
        if (image) {
            strongSelf.imageView.frame = CGRectMake(0, 0, image.size.width, image.size.height);
            strongSelf.scrollView.contentSize = image.size;
        } else {
            PFLogWarning(@"详情图加载失败 id=%@", strongSelf.item.identifier);
        }
    }];
}

- (void)dealloc {
    if (self.loadToken) {
        [[PFImageLoader sharedLoader] cancelLoadWithToken:self.loadToken];
    }
    PFLogDebug(@"PFPhotoDetailViewController 释放");
}

#pragma mark - UIScrollViewDelegate（缩放）

- (UIView *)viewForZoomingInScrollView:(UIScrollView *)scrollView {
    return self.imageView;
}

@end
