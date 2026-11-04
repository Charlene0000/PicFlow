//
//  PFPhotoItem.h
//  PicFlow
//
//  Model 层：只负责数据承载与业务语义，不持有任何 UI 对象。
//
//  数据来源是**自研图库**（core/picflow_catalog.c），
//  不依赖任何外部图片 API：每个条目由 (seed, 宽高比) 确定，
//  图片本身由 PFArtworkRenderer 用 Core Graphics 实时绘制。
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/// 图片来源模式。
typedef NS_ENUM(NSInteger, PFPhotoSource) {
    /// 本地程序化生成（默认，零网络、零外部依赖）。
    PFPhotoSourceProgrammatic = 0,
    /// 远程图库（通过 NSURLSession 访问自建 mock server）。
    PFPhotoSourceRemote,
};

@interface PFPhotoItem : NSObject

/// 业务唯一 ID，用作图片缓存 key 的组成部分。
@property (nonatomic, copy)   NSString  *identifier;
/// 展示用标题。
@property (nonatomic, copy)   NSString  *title;
/// 作者名。
@property (nonatomic, copy)   NSString  *author;
/// 像素宽高，用于在渲染前就计算好瀑布流的目标高度（避免二次 reflow）。
@property (nonatomic, assign) CGFloat   pixelWidth;
@property (nonatomic, assign) CGFloat   pixelHeight;
/// 绘制种子（决定配色与图案）。
@property (nonatomic, assign) uint32_t  seed;
/// 图库索引（程序化模式下用于定位规格）。
@property (nonatomic, assign) NSInteger catalogIndex;
/// 数据来源。
@property (nonatomic, assign) PFPhotoSource source;

/// 从自研图库构造指定索引的条目。
+ (nullable instancetype)itemAtCatalogIndex:(NSInteger)index;

/// 远程图库模式：从 JSON 字典构造。
+ (nullable instancetype)itemWithDictionary:(NSDictionary *)dict;

/// 图片宽高比（高 / 宽），瀑布流布局计算高度时使用。
- (CGFloat)aspectRatio;

/// 预估的目标高度（给定列宽）。
- (CGFloat)estimatedHeightWithColumnWidth:(CGFloat)columnWidth;

/// 缩略图缓存 key。
- (NSString *)cacheKeyWithColumnWidth:(CGFloat)columnWidth;

@end

NS_ASSUME_NONNULL_END
