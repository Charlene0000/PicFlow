//
//  AppDelegate.m
//  PicFlow
//

#import "AppDelegate.h"
#import "Utils/PFLog.h"
#import "Utils/PFImageCache.h"

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application
didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id> *)launchOptions {
    PFLogInfo(@"PicFlow 启动 | system=%@ model=%@", [[UIDevice currentDevice] systemVersion],
              [[UIDevice currentDevice] model]);

    // 注册内存警告监听：图片缓存统一降级，避免 App 被系统杀死。
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(pf_handleMemoryWarning)
                                                 name:UIApplicationDidReceiveMemoryWarningNotification
                                               object:nil];
    return YES;
}

- (void)pf_handleMemoryWarning {
    PFLogWarning(@"收到内存警告，清理图片内存缓存");
    [[PFImageCache sharedCache] clearMemory];
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - Scene

- (UISceneConfiguration *)application:(UIApplication *)application
configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession
                              options:(UISceneConnectionOptions *)options {
    return [[UISceneConfiguration alloc] initWithName:@"Default Configuration"
                                          sessionRole:connectingSceneSession.role];
}

@end
