//
//  PFImageLoader.h
//  PicFlow
//
//  图片加载器：内存缓存 -> 磁盘缓存 -> 网络下载 三级链路。
//  1. 命中 NSCache 直接回调（同步返回，零延迟）；
//  2. 命中磁盘缓存直接解码并回填内存缓存；
//  3. 都未命中才走网络，下载完成后落盘 + 回填内存。
//
//  关键点：每个任务都可以 cancel，Cell 复用时必须取消，
//  否则滚动时会出现「图片错位」——这是 UICollectionView 最经典的 Bug 之一。
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef void (^PFImageCompletion)(UIImage * _Nullable image);

@interface PFImageLoader : NSObject

+ (instancetype)sharedLoader;

/// 加载图片。返回一个取消令牌（token），调用方必须在 cell 复用时取消它，
/// 否则滚动过快时，未完成的旧请求回调会覆盖新数据（图片错位）。
- (id)loadImageWithURL:(NSURL *)url
              cacheKey:(NSString *)cacheKey
             completion:(PFImageCompletion)completion;

/// 取消某次加载。
- (void)cancelLoadWithToken:(id _Nullable)token;

@end

NS_ASSUME_NONNULL_END
