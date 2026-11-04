//
//  PFImageLoader.m
//  PicFlow
//

#import "PFImageLoader.h"
#import "PFImageCache.h"
#import "PFLog.h"

static NSString * const kPFImageLoaderDiskFolder = @"com.picflow.images";

#pragma mark - 任务令牌

/// 用一个轻量对象持有 task，方便 cancel 时拿到准确的那一个 task。
@interface PFImageTaskToken : NSObject
@property (nonatomic, strong, nullable) NSURLSessionDataTask *task;
@property (nonatomic, copy) NSString *cacheKey;
@end

@implementation PFImageTaskToken
@end

#pragma mark - 加载器

@interface PFImageLoader ()
@property (nonatomic, strong) NSURLSession *session;
@property (nonatomic, strong) dispatch_queue_t decodeQueue;
/// 磁盘缓存目录，懒加载。
@property (nonatomic, strong, nullable) NSURL *diskCacheURL;
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
        _decodeQueue = dispatch_queue_create("com.picflow.decode", DISPATCH_QUEUE_CONCURRENT);
    }
    return self;
}

#pragma mark - 加载

- (id)loadImageWithURL:(NSURL *)url
              cacheKey:(NSString *)cacheKey
             completion:(PFImageCompletion)completion {
    if (!url) {
        if (completion) {
            completion(nil);
        }
        return nil;
    }

    // 1) 内存缓存命中：立即回调，Cell 直接显示，无闪烁。
    UIImage *cached = [[PFImageCache sharedCache] imageForKey:cacheKey];
    if (cached) {
        if (completion) {
            completion(cached);
        }
        return nil;   // 无需网络任务，无令牌可取消
    }

    // 2) 磁盘缓存命中：后台解码 -> 回填内存 -> 主线程回调。
    NSString *diskPath = [self diskPathForKey:cacheKey];
    if ([[NSFileManager defaultManager] fileExistsAtPath:diskPath]) {
        PFImageTaskToken *token = [[PFImageTaskToken alloc] init];
        token.cacheKey = cacheKey;
        [self decodeImageAtPath:diskPath
                           token:token
                      completion:^(UIImage *image) {
            if (!image) {
                // 磁盘文件损坏，回退到网络重新下载。
                [self loadFromNetworkWithURL:url cacheKey:cacheKey completion:completion];
                return;
            }
            [self.cacheImage:image key:cacheKey];
            if (completion) {
                completion(image);
            }
        }];
        return token;
    }

    // 3) 网络下载。
    return [self loadFromNetworkWithURL:url cacheKey:cacheKey completion:completion];
}

- (id)loadFromNetworkWithURL:(NSURL *)url
                     cacheKey:(NSString *)cacheKey
                    completion:(PFImageCompletion)completion {
    PFImageTaskToken *token = [[PFImageTaskToken alloc] init];
    token.cacheKey = cacheKey;

    __weak typeof(self) weakSelf = self;
    NSURLSessionDataTask *task = [self.session dataTaskWithURL:url
                                              completionHandler:^(NSData *data,
                                                                  NSURLResponse *response,
                                                                  NSError *error) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        if (error) {
            PFLogWarning(@"图片下载失败 %@ | %@", url.lastPathComponent, error.localizedDescription);
            if (completion) {
                completion(nil);
            }
            return;
        }
        if (data.length == 0) {
            if (completion) {
                completion(nil);
            }
            return;
        }

        // 图片解码是 CPU 密集操作，放到后台并发队列，避免阻塞主线程掉帧。
        [strongSelf decodeImageWithData:data
                                  token:token
                             completion:^(UIImage *image) {
            if (!image) {
                if (completion) {
                    completion(nil);
                }
                return;
            }
            [strongSelf.cacheImage:image key:cacheKey];
            [strongSelf saveImageData:data forKey:cacheKey];
            if (completion) {
                completion(image);
            }
        }];
    }];

    token.task = task;
    [task resume];
    return token;
}

- (void)cancelLoadWithToken:(id)token {
    if (![token isKindOfClass:[PFImageTaskToken class]]) {
        return;
    }
    PFImageTaskToken *imageToken = (PFImageTaskToken *)token;
    [imageToken.task cancel];
    imageToken.task = nil;
    PFLogDebug(@"取消图片加载 key=%@", imageToken.cacheKey);
}

#pragma mark - 解码 / 缓存 / 落盘

- (void)decodeImageWithData:(NSData *)data
                      token:(PFImageTaskToken *)token
                 completion:(PFImageCompletion)completion {
    dispatch_async(self.decodeQueue, ^{
        // CGFloat 缓存图片在解码时不解压到精确像素，避免内存暴涨。
        UIImage *image = [UIImage imageWithData:data scale:UIScreen.mainScreen.scale];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (completion) {
                completion(image);
            }
        });
    });
}

- (void)decodeImageAtPath:(NSString *)path
                     token:(PFImageTaskToken *)token
                completion:(PFImageCompletion)completion {
    dispatch_async(self.decodeQueue, ^{
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

#pragma mark - 磁盘路径

- (NSString *)diskPathForKey:(NSString *)key {
    if (!self.diskCacheURL) {
        NSURL *caches = [[NSFileManager defaultManager] URLsForDirectory:NSCachesDirectory
                                                                 inDomains:NSUserDomainMask].firstObject;
        self.diskCacheURL = [caches URLByAppendingPathComponent:kPFImageLoaderDiskFolder isDirectory:YES];
    }
    // 用 MD5 之外的轻量做法：key 里的 "/" 会被替换，避免目录穿越。
    NSString *safeKey = [key stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
    return [self.diskCacheURL.path stringByAppendingPathComponent:safeKey];
}

@end
