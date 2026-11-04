//
//  PFImageLoader.m
//  PicFlow
//

#import "PFImageLoader.h"
#import "PFImageCache.h"
#import "../Views/PFArtworkRenderer.h"
#import "../Network/PFNetworkManager.h"
#import "../Utils/PFLog.h"
#include "../../core/picflow_cache.h"

static NSString * const kPFImageLoaderDiskFolder = @"com.picflow.images";

#pragma mark - 任务令牌

/// 用一个轻量对象持有 task，方便 cancel 时拿到准确的那一个 task。
@interface PFImageTaskToken : NSObject
@property (nonatomic, strong, nullable) NSURLSessionDataTask *task;
@property (nonatomic, copy) NSString *cacheKey;
@property (nonatomic, assign) BOOL cancelled;
@end

@implementation PFImageTaskToken
@end

#pragma mark - 加载器

@interface PFImageLoader ()
@property (nonatomic, strong) NSURLSession *session;
@property (nonatomic, strong) dispatch_queue_t renderQueue;
/// 磁盘缓存目录，懒加载。
@property (nonatomic, strong, nullable) NSURL *diskCacheURL;
/// ★ C 层内存预算决策器：决定「现在还能不能再解码/绘制一张图」。
@property (nonatomic, assign) PFMemoryBudget budget;
/// 保护 budget 的读写。
@property (nonatomic, strong) NSLock *budgetLock;
@end

@implementation PFImageLoader

+ (instancetype)sharedLoader {
    static PFImageLoader *loader = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        loader = [[PFImageLoader alloc] init];
    });
    return loader;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        NSURLSessionConfiguration *config = [NSURLSessionConfiguration defaultSessionConfiguration];
        config.timeoutIntervalForRequest = 20.0;
        config.URLCache = [[NSURLCache alloc] initWithMemoryCapacity:8 * 1024 * 1024
                                                         diskCapacity:50 * 1024 * 1024
                                                             diskPath:@"pf_images"];
        _session = [NSURLSession sessionWithConfiguration:config];
        _renderQueue = dispatch_queue_create("com.picflow.render", DISPATCH_QUEUE_CONCURRENT);
        _budgetLock = [[NSLock alloc] init];
        pf_memory_budget_init(&_budget, 60u * 1024u * 1024u);
    }
    return self;
}

#pragma mark - 加载入口

- (id)loadImageForItem:(PFPhotoItem *)item
              cacheKey:(NSString *)cacheKey
             completion:(PFImageCompletion)completion {
    if (!item || cacheKey.length == 0) {
        if (completion) {
            completion(nil);
        }
        return nil;
    }

    // 1) 内存缓存命中：立即回调，零延迟零闪烁。
    UIImage *cached = [[PFImageCache sharedCache] imageForKey:cacheKey];
    if (cached) {
        if (completion) {
            completion(cached);
        }
        return nil;   // 无需任务，无令牌可取消
    }

    // 2) 按来源分流。
    if (item.source == PFPhotoSourceRemote) {
        return [self loadRemoteImageForItem:item cacheKey:cacheKey completion:completion];
    }
    return [self renderProgrammaticImageForItem:item cacheKey:cacheKey completion:completion];
}

#pragma mark - 通道 A：程序化生成（零网络）

- (id)renderProgrammaticImageForItem:(PFPhotoItem *)item
                            cacheKey:(NSString *)cacheKey
                           completion:(PFImageCompletion)completion {
    PFImageTaskToken *token = [[PFImageTaskToken alloc] init];
    token.cacheKey = cacheKey;

    // 目标绘制尺寸 = 列宽对应的像素尺寸
    CGFloat scale = [UIScreen mainScreen].scale;
    CGFloat targetWidth = MAX(1.0, item.pixelWidth / scale);
    CGFloat ratio = (item.pixelHeight / MAX(item.pixelWidth, 1.0));
    CGSize targetSize = CGSizeMake(targetWidth, targetWidth * ratio);

    // 预算检查：估算这张图绘制后占用的内存，超预算就不画（直接给占位图）
    size_t estimatedCost = (size_t)(targetSize.width * scale * targetSize.height * scale * 4);

    __weak typeof(self) weakSelf = self;
    dispatch_async(self.renderQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        if (token.cancelled) {
            PFLogDebug(@"绘制前已取消 key=%@", cacheKey);
            return;
        }

        [strongSelf.budgetLock lock];
        BOOL allowed = pf_memory_budget_can_alloc(&strongSelf.budget, estimatedCost);
        if (allowed) {
            pf_memory_budget_reserve(&strongSelf.budget, estimatedCost);
        }
        [strongSelf.budgetLock unlock];

        if (!allowed) {
            PFLogWarning(@"超出内存预算，跳过绘制 key=%@ cost=%luKB",
                         cacheKey, (unsigned long)(estimatedCost / 1024));
            dispatch_async(dispatch_get_main_queue(), ^{
                if (completion) {
                    completion(nil);
                }
            });
            return;
        }

        // Core Graphics 绘制（CPU 密集，已在后台队列）
        UIImage *image = [PFArtworkRenderer renderArtworkWithSeed:item.seed
                                                             size:targetSize
                                                            scale:scale];
        if (!image) {
            [strongSelf.budgetLock lock];
            pf_memory_budget_release(&strongSelf.budget, estimatedCost);
            [strongSelf.budgetLock unlock];
            dispatch_async(dispatch_get_main_queue(), ^{
                if (completion) {
                    completion(nil);
                }
            });
            return;
        }

        [strongSelf.cacheImage:image key:cacheKey];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (completion) {
                completion(image);
            }
        });
    });

    return token;
}

#pragma mark - 通道 B：远程下载（NSURLSession）

- (id)loadRemoteImageForItem:(PFPhotoItem *)item
                     cacheKey:(NSString *)cacheKey
                    completion:(PFImageCompletion)completion {
    // 远程图库约定：identifier 字段承载图片 URL。
    NSURL *url = [NSURL URLWithString:item.identifier];
    if (!url || !url.scheme) {
        if (completion) {
            completion(nil);
        }
        return nil;
    }

    // 2a. 磁盘缓存命中
    NSString *diskPath = [self diskPathForKey:cacheKey];
    if ([[NSFileManager defaultManager] fileExistsAtPath:diskPath]) {
        PFImageTaskToken *token = [[PFImageTaskToken alloc] init];
        token.cacheKey = cacheKey;
        [self decodeImageAtPath:diskPath completion:^(UIImage *image) {
            if (!image) {
                // 磁盘文件损坏，回退到网络重新下载。
                [self loadRemoteFromNetworkURL:url cacheKey:cacheKey completion:completion];
                return;
            }
            [self.cacheImage:image key:cacheKey];
            if (completion) {
                completion(image);
            }
        }];
        return token;
    }

    // 2b. 网络下载
    return [self loadRemoteFromNetworkURL:url cacheKey:cacheKey completion:completion];
}

- (id)loadRemoteFromNetworkURL:(NSURL *)url
                       cacheKey:(NSString *)cacheKey
                      completion:(PFImageCompletion)completion {
    PFImageTaskToken *token = [[PFImageTaskToken alloc] init];
    token.cacheKey = cacheKey;

    __weak typeof(self) weakSelf = self;
    token.task = [[PFNetworkManager sharedManager] GETData:url.absoluteString
                                                   success:^(NSData *data) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf || token.cancelled) {
            return;
        }
        [strongSelf decodeImageWithData:data completion:^(UIImage *image) {
            if (image) {
                [strongSelf.cacheImage:image key:cacheKey];
                [strongSelf saveImageData:data forKey:cacheKey];
            }
            if (completion) {
                completion(image);
            }
        }];
    }
                                                   failure:^(NSError *error) {
        PFLogWarning(@"远程图片加载失败 %@ | %@",
                     url.lastPathComponent, error.localizedDescription);
        if (completion) {
            completion(nil);
        }
    }];
    return token;
}

#pragma mark - 解码 / 缓存 / 落盘

- (void)decodeImageWithData:(NSData *)data completion:(PFImageCompletion)completion {
    dispatch_async(self.renderQueue, ^{
        // CGFloat 缓存图片在解码时不解压到精确像素，避免内存暴涨。
        UIImage *image = [UIImage imageWithData:data scale:UIScreen.mainScreen.scale];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (completion) {
                completion(image);
            }
        });
    });
}

- (void)decodeImageAtPath:(NSString *)path completion:(PFImageCompletion)completion {
    dispatch_async(self.renderQueue, ^{
        NSData *data = [NSData dataWithContentsOfFile:path];
        UIImage *image = data ? [UIImage imageWithData:data scale:UIScreen.mainScreen.scale] : nil;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (completion) {
                completion(image);
            }
        });
    });
}

- (void)cacheImage:(UIImage *)image key:(NSString *)key {
    NSUInteger cost = (NSUInteger)(image.size.width * image.scale * image.size.height * image.scale * 4);
    [[PFImageCache sharedCache] storeImage:image forKey:key cost:cost];

    [self.budgetLock lock];
    pf_memory_budget_reserve(&self.budget, cost);
    [self.budgetLock unlock];
}

- (void)saveImageData:(NSData *)data forKey:(NSString *)key {
    NSString *path = [self diskPathForKey:key];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        [[NSFileManager defaultManager] createDirectoryAtPath:path.stringByDeletingLastPathComponent
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:nil];
        [data writeToFile:path atomically:YES];
    });
}

- (NSUInteger)estimatedMemoryUsage {
    [self.budgetLock lock];
    size_t used = self.budget.reserved_bytes;
    [self.budgetLock unlock];
    return (NSUInteger)used;
}

#pragma mark - 取消

- (void)cancelLoadWithToken:(id)token {
    if (![token isKindOfClass:[PFImageTaskToken class]]) {
        return;
    }
    PFImageTaskToken *imageToken = (PFImageTaskToken *)token;
    imageToken.cancelled = YES;
    [imageToken.task cancel];
    imageToken.task = nil;
    PFLogDebug(@"取消图片加载 key=%@", imageToken.cacheKey);
}

#pragma mark - 磁盘路径

- (NSString *)diskPathForKey:(NSString *)key {
    if (!self.diskCacheURL) {
        NSURL *caches = [[NSFileManager defaultManager] URLsForDirectory:NSCachesDirectory
                                                                 inDomains:NSUserDomainMask].firstObject;
        self.diskCacheURL = [caches URLByAppendingPathComponent:kPFImageLoaderDiskFolder isDirectory:YES];
    }
    // key 里的 "/" 会被替换，避免目录穿越。
    NSString *safeKey = [key stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
    return [self.diskCacheURL.path stringByAppendingPathComponent:safeKey];
}

@end
