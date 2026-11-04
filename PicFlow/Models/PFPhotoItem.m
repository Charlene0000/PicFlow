//
//  PFPhotoItem.m
//  PicFlow
//

#import "PFPhotoItem.h"
#include "../../core/picflow_catalog.h"

@implementation PFPhotoItem

+ (instancetype)itemAtCatalogIndex:(NSInteger)index {
    if (index < 0 || index >= pf_catalog_count()) {
        return nil;
    }
    PFCatalogEntry entry;
    pf_catalog_entry_at((int)index, &entry);

    PFPhotoItem *item = [[PFPhotoItem alloc] init];
    item.identifier  = [NSString stringWithFormat:@"pf-%d", entry.id];
    item.title       = [NSString stringWithFormat:@"%@", entry.title];
    item.author      = [NSString stringWithFormat:@"%@", entry.author];
    item.seed        = entry.seed;
    item.catalogIndex = index;
    item.source      = PFPhotoSourceProgrammatic;

    // 像素尺寸按 2 倍屏宽估算，保证缩略图清晰；
    // 真实绘制尺寸由列宽决定，宽高比来自图库规格。
    CGFloat baseWidth = 640.0;
    item.pixelWidth  = baseWidth;
    item.pixelHeight = baseWidth * entry.aspect_ratio;
    return item;
}

+ (instancetype)itemWithDictionary:(NSDictionary *)dict {
    if (![dict isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    PFPhotoItem *item = [[PFPhotoItem alloc] init];
    NSString *identifier = dict[@"id"];
    item.identifier = identifier.length > 0 ? [identifier description] : [[NSUUID UUID] UUIDString];
    item.title  = dict[@"title"] ?: item.identifier;
    item.author = dict[@"author"] ?: @"Unknown";

    NSNumber *width = dict[@"width"];
    NSNumber *height = dict[@"height"];
    item.pixelWidth  = width.doubleValue > 0 ? width.doubleValue : 640;
    item.pixelHeight = height.doubleValue > 0 ? height.doubleValue : 640;

    NSNumber *seed = dict[@"seed"];
    item.seed = seed.unsignedIntValue;
    item.source = PFPhotoSourceRemote;
    return item;
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

- (NSString *)cacheKeyWithColumnWidth:(CGFloat)columnWidth {
    return [NSString stringWithFormat:@"%@-%.0f", self.identifier, columnWidth];
}

@end
