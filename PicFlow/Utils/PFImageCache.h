//
//  PFImageCache.h
//  PicFlow
//
//  图片内存缓存，基于 NSCache。
//  为什么用 NSCache 而不是 NSDictionary？
//  1. NSCache 在内存吃紧时会被系统自动清理（线程安全，不会因此崩溃）；
//  2. 支持 totalCostLimit，按字节数控制内存占用，比按个数更精确；
//  3. 不会因为强引用导致图片内存无法释放。
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface PFImageCache : NSObject

+ (instancetype)sharedCache;

/// 读缓存。返回 nil 表示未命中。
- (nullable UIImage *)imageForKey:(NSString *)key;

/// 写缓存。cost 传图片字节数，超出 totalCostLimit 时 NSCache 自动淘汰。
- (void)storeImage:(UIImage *)image forKey:(NSString *)key cost:(NSUInteger)cost;

/// 内存告警时调用：清空全部内存缓存。
- (void)clearMemory;

/// 当前缓存的图片数量（调试用）。
- (NSUInteger)currentCount;

/// 当前缓存占用的字节数（调试用）。
- (NSUInteger)currentCost;

@end

NS_ASSUME_NONNULL_END
