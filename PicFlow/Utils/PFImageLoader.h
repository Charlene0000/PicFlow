//
//  PFImageLoader.h
//  PicFlow
//
//  图片加载器。
//
//  两条通道，由 item 的 source 决定：
//  A. 程序化生成（默认，零网络）
//     内存缓存 -> 后台队列用 Core Graphics 绘制 -> 回填内存缓存
//  B. 远程下载（NSURLSession）
//     内存缓存 -> 磁盘缓存 -> NSURLSession 下载 -> 后台解码 -> 落盘 + 回填
//
//  两种通道都遵守同一套约定：
//  1. 每个任务都返回取消令牌，Cell 复用时必须取消；
//  2. 解码/绘制全部在后台队列，绝不阻塞主线程；
//  3. 内存占用由 C 层的预算决策器（picflow_cache.h）统一裁决，
//     超出预算时拒绝继续解码，避免内存暴涨。
//

#import <UIKit/UIKit.h>
#import "../Models/PFPhotoItem.h"

NS_ASSUME_NONNULL_BEGIN

typedef void (^PFImageCompletion)(UIImage * _Nullable image);

@interface PFImageLoader : NSObject

+ (instancetype)sharedLoader;

/// 加载图片。返回一个取消令牌（token），调用方必须在 cell 复用时取消它，
/// 否则滚动过快时，未完成的旧请求回调会覆盖新数据（图片错位）。
- (nullable id)loadImageForItem:(PFPhotoItem *)item
                        cacheKey:(NSString *)cacheKey
                       completion:(PFImageCompletion)completion;

/// 取消某次加载。
- (void)cancelLoadWithToken:(id _Nullable)token;

/// 当前内存占用估算（字节）。可用于调试与日志。
- (NSUInteger)estimatedMemoryUsage;

@end

NS_ASSUME_NONNULL_END
