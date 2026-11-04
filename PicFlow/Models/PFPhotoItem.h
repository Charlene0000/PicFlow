//
//  PFPhotoItem.h
//  PicFlow
//
//  Model 层：只负责数据承载与业务语义，不持有任何 UI 对象，
//  保证可被后台线程安全访问（这是 MVVM 中 ViewModel 的线程约束前提）。
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/// 图片来源。示例数据源用 Picsum（基于 Unsplash 的公开示例图库），无需 API Key 即可跑通。
typedef NS_ENUM(NSUInteger, PFPhotoSource) {
    PFPhotoSourcePicsum = 0,
};

@interface PFPhotoItem : NSObject

/// 业务唯一 ID，用作图片缓存 key 的组成部分。
@property (nonatomic, copy)   NSString  *identifier;
/// 展示用标题。
@property (nonatomic, copy)   NSString  *title;
/// 缩略图 URL。
@property (nonatomic, strong) NSURL     *thumbURL;
/// 原图 URL。
@property (nonatomic, strong) NSURL     *originURL;
/// 像素宽高，用于在下载前就计算好瀑布流的目标高度（避免图片回来后二次 reflow）。
@property (nonatomic, assign) CGFloat   pixelWidth;
@property (nonatomic, assign) CGFloat   pixelHeight;
/// 作者名。
@property (nonatomic, copy)   NSString  *author;
/// 数据来源。
@property (nonatomic, assign) PFPhotoSource source;

/// 原始 JSON 字典 -> 模型对象。
/// 放在 Model 而不是 Controller 里，是为了让 ViewModel 只关心业务数据。
+ (instancetype)itemWithDictionary:(NSDictionary *)dict;

/// 依据原始像素比计算缩略图 URL。
/// Picsum 的图片服务支持 /id/{id}/{width}/{height} 形式裁剪，
/// 这里统一按列宽请求，减少一次无谓的整图下载。
- (NSURL *)thumbnailURLWithWidth:(CGFloat)width;

/// 图片宽高比（高 / 宽），瀑布流布局计算高度时使用。
- (CGFloat)aspectRatio;

/// 预估的目标高度（给定列宽）。
- (CGFloat)estimatedHeightWithColumnWidth:(CGFloat)columnWidth;

@end

NS_ASSUME_NONNULL_END
