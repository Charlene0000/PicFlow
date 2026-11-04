//
//  PFImageCache.m
//  PicFlow
//

#import "PFImageCache.h"
#import "PFLog.h"

static const NSUInteger kPFImageCacheCountLimit = 120;             // 最多缓存 120 张
static const NSUInteger kPFImageCacheCostLimit = 60 * 1024 * 1024; // 60MB 上限

@interface PFImageCache ()
@property (nonatomic, strong) NSCache<NSString *, UIImage *> *cache;
/// NSCache 没有公开的 count / cost 查询接口，这里用一把读写锁自己维护统计值，
/// 便于在 LLDB 里 po 出来观察内存增长（调试内存泄漏/暴涨时非常有用）。
@property (nonatomic, strong) NSLock *statLock;
@property (nonatomic, assign) NSUInteger statCount;
@property (nonatomic, assign) NSUInteger statCost;
@end

@implementation PFImageCache

+ (instancetype)sharedCache {
    static PFImageCache *cache = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cache = [[PFImageCache alloc] init];
    });
    return cache;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _cache = [[NSCache alloc] init];
        _cache.countLimit = kPFImageCacheCountLimit;
        _cache.totalCostLimit = kPFImageCacheCostLimit;
        _cache.name = @"com.picflow.imagecache";
        _statLock = [[NSLock alloc] init];
    }
    return self;
}

- (UIImage *)imageForKey:(NSString *)key {
    if (key.length == 0) {
        return nil;
    }
    UIImage *image = [self.cache objectForKey:key];
    if (image) {
        PFLogDebug(@"缓存命中 key=%@", key);
    }
    return image;
}

- (void)storeImage:(UIImage *)image forKey:(NSString *)key cost:(NSUInteger)cost {
    if (!image || key.length == 0) {
        return;
    }
    [self.cache setObject:image forKey:key cost:cost];

    [self.statLock lock];
    self.statCount += 1;
    self.statCost += cost;
    NSUInteger count = self.statCount;
    NSUInteger totalCost = self.statCost;
    [self.statLock unlock];

    PFLogDebug(@"写入缓存 key=%@ cost=%luKB 当前=%lu张/%luKB", key,
               (unsigned long)(cost / 1024), (unsigned long)count, (unsigned long)(totalCost / 1024));
}

- (void)clearMemory {
    [self.cache removeAllObjects];
    [self.statLock lock];
    self.statCount = 0;
    self.statCost = 0;
    [self.statLock unlock];
    PFLogInfo(@"图片缓存已清空");
}

- (NSUInteger)currentCount {
    [self.statLock lock];
    NSUInteger count = self.statCount;
    [self.statLock unlock];
    return count;
}

- (NSUInteger)currentCost {
    [self.statLock lock];
    NSUInteger cost = self.statCost;
    [self.statLock unlock];
    return cost;
}

@end
