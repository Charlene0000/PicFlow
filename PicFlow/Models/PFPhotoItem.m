//
//  PFPhotoItem.m
//  PicFlow
//

#import "PFPhotoItem.h"

@implementation PFPhotoItem

+ (instancetype)itemWithDictionary:(NSDictionary *)dict {
    if (![dict isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    PFPhotoItem *item = [[PFPhotoItem alloc] init];
    item.identifier = [dict[@"id"] description] ?: @"";

    NSString *author = dict[@"author"];
    item.author = [author isKindOfClass:[NSString class]] ? author : @"Unknown";

    // 标题用作者 + ID 组合，保证列表里每一项都有可读文本。
    item.title = [NSString stringWithFormat:@"%@ · #%@", item.author, item.identifier];

    NSNumber *width = dict[@"width"];
    NSNumber *height = dict[@"height"];
    item.pixelWidth = width.doubleValue > 0 ? width.doubleValue : 1000;
    item.pixelHeight = height.doubleValue > 0 ? height.doubleValue : 1000;

    NSString *urlString = dict[@"url"];
    if ([urlString isKindOfClass:[NSString class]]) {
        item.originURL = [NSURL URLWithString:urlString];
    }
    item.source = PFPhotoSourcePicsum;
    return item;
}

- (NSURL *)thumbnailURLWithWidth:(CGFloat)width {
    // 目标高度按原始宽高比推算，向下取整避免拉伸。
    CGFloat height = [self estimatedHeightWithColumnWidth:width];
    NSString *urlString = [NSString stringWithFormat:@"https://picsum.photos/id/%@/%.0f/%.0f",
                           self.identifier, width, height];
    return [NSURL URLWithString:urlString];
}

- (CGFloat)aspectRatio {
    if (self.pixelWidth <= 0) {
        return 1.0;
    }
    return self.pixelHeight / self.pixelWidth;
}

- (CGFloat)estimatedHeightWithColumnWidth:(CGFloat)columnWidth {
    return columnWidth * [self aspectRatio];
}

@end
