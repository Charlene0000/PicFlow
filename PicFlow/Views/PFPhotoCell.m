//
//  PFPhotoCell.m
//  PicFlow
//

#import "PFPhotoCell.h"
#import "../Utils/PFImageLoader.h"
#import "../Utils/PFLog.h"

@interface PFPhotoCell ()
@property (nonatomic, strong) UIImageView *imageView;
@property (nonatomic, strong) UILabel *titleLabel;
/// 当前图片加载的取消令牌。
@property (nonatomic, strong, nullable) id loadToken;
/// 占位背景层：图片下载中显示，避免出现空白跳动。
@property (nonatomic, strong) UIView *placeholderView;
/// 图片区高度约束。用属性持有而不是每次新建，
/// 否则复用时不断追加约束会触发 "Unable to simultaneously satisfy constraints"。
@property (nonatomic, strong, nullable) NSLayoutConstraint *imageHeightConstraint;
@end

@implementation PFPhotoCell

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self setupSubviews];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    self = [super initWithCoder:coder];
    if (self) {
        [self setupSubviews];
    }
    return self;
}

#pragma mark - UI 构建（纯代码 + Auto Layout / NSLayoutAnchor）

- (void)setupSubviews {
    self.contentView.backgroundColor = [UIColor whiteColor];
    self.contentView.clipsToBounds = YES;
    self.contentView.layer.cornerRadius = 8.0;
    self.contentView.layer.masksToBounds = YES;

    self.placeholderView = [[UIView alloc] init];
    self.placeholderView.backgroundColor = [UIColor colorWithWhite:0.92 alpha:1.0];
    [self.contentView addSubview:self.placeholderView];

    self.imageView = [[UIImageView alloc] init];
    self.imageView.contentMode = UIViewContentModeScaleAspectFill;
    self.imageView.clipsToBounds = YES;
    self.imageView.backgroundColor = [UIColor colorWithWhite:0.95 alpha:1.0];
    [self.contentView addSubview:self.imageView];

    self.titleLabel = [[UILabel alloc] init];
    self.titleLabel.font = [UIFont systemFontOfSize:11.0];
    self.titleLabel.textColor = [UIColor colorWithWhite:0.35 alpha:1.0];
    self.titleLabel.numberOfLines = 1;
    self.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    [self.contentView addSubview:self.titleLabel];

    // 上下左右四边贴 contentView（cell 内部不需要安全区，安全区由 VC 处理）。
    [self.placeholderView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor].active = YES;
    [self.placeholderView.leadingAnchor constraintEqualToAnchor:self.contentView.leadingAnchor].active = YES;
    [self.placeholderView.trailingAnchor constraintEqualToAnchor:self.contentView.trailingAnchor].active = YES;
    [self.placeholderView.bottomAnchor constraintEqualToAnchor:self.contentView.bottomAnchor].active = YES;

    [self.imageView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor].active = YES;
    [self.imageView.leadingAnchor constraintEqualToAnchor:self.contentView.leadingAnchor].active = YES;
    [self.imageView.trailingAnchor constraintEqualToAnchor:self.contentView.trailingAnchor].active = YES;

    // 标题固定在底部 16pt 高的区域。
    [self.titleLabel.leadingAnchor constraintEqualToAnchor:self.contentView.leadingAnchor constant:6.0].active = YES;
    [self.titleLabel.trailingAnchor constraintEqualToAnchor:self.contentView.trailingAnchor constant:-6.0].active = YES;
    [self.titleLabel.bottomAnchor constraintEqualToAnchor:self.contentView.bottomAnchor constant:-4.0].active = YES;
    [self.titleLabel.heightAnchor constraintEqualToConstant:16.0].active = YES;

    // 图片底部锚定到标题顶部。
    [self.imageView.bottomAnchor constraintEqualToAnchor:self.titleLabel.topAnchor constant:-4.0].active = YES;

    // 高度约束先创建一次（激活状态为 NO），复用时只改 constant。
    self.imageHeightConstraint = [self.imageView.heightAnchor constraintEqualToConstant:0.0];
    self.imageHeightConstraint.priority = UILayoutPriorityDefaultHigh;
    self.imageHeightConstraint.active = YES;
}

#pragma mark - 复用

- (void)prepareForReuse {
    [super prepareForReuse];

    // ① 取消上一次尚未完成的图片请求，避免旧请求回来覆盖新数据。
    if (self.loadToken) {
        [[PFImageLoader sharedLoader] cancelLoadWithToken:self.loadToken];
        self.loadToken = nil;
    }
    // ② 重置 UI 状态，否则复用后会残留上一个 item 的标题。
    self.imageView.image = nil;
    self.titleLabel.text = nil;
    self.representedID = nil;
    self.placeholderView.hidden = NO;
}

#pragma mark - 配置

- (void)configureWithItem:(PFPhotoItem *)item columnWidth:(CGFloat)columnWidth {
    self.representedID = item.identifier;
    self.titleLabel.text = item.title;

    // 用原始宽高比计算图片区高度，保证瀑布流视觉上不裁切。
    // 注意：改 constant 而不是重新添加约束 —— 这是避免约束冲突的关键细节。
    CGFloat imageHeight = [item estimatedHeightWithColumnWidth:columnWidth];
    self.imageHeightConstraint.constant = imageHeight;

    NSURL *thumbURL = [item thumbnailURLWithWidth:MAX(1.0, columnWidth * [UIScreen mainScreen].scale)];
    NSString *cacheKey = [NSString stringWithFormat:@"%@-%.0f", item.identifier, columnWidth];

    __weak typeof(self) weakSelf = self;

    // 保存取消令牌：cell 被复用时 prepareForReuse 会取消这次请求。
    self.loadToken = [[PFImageLoader sharedLoader] loadImageWithURL:thumbURL
                                                            cacheKey:cacheKey
                                                           completion:^(UIImage *image) {
        // ③ 回调时校验 representedID：cell 可能已经被复用给别的 item 了。
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        if (![strongSelf.representedID isEqualToString:item.identifier]) {
            PFLogDebug(@"丢弃过期图片回调 id=%@ 当前=%@", item.identifier, strongSelf.representedID);
            return;
        }
        strongSelf.imageView.image = image;
        strongSelf.placeholderView.hidden = (image != nil);
    }];
}

@end
