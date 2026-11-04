//
//  SceneDelegate.m
//  PicFlow
//

#import "SceneDelegate.h"
#import "Controllers/PFGalleryViewController.h"
#import "Utils/PFLog.h"

@implementation SceneDelegate

- (void)scene:(UIScene *)scene
willConnectToSession:(UISceneSession *)session
      options:(UISceneConnectionOptions *)connectionOptions {
    if (![scene isKindOfClass:[UIWindowScene class]]) {
        return;
    }
    UIWindowScene *windowScene = (UIWindowScene *)scene;

    self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
    PFGalleryViewController *gallery = [[PFGalleryViewController alloc] init];
    UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:gallery];
    // 关闭大标题是为了让首屏瀑布流占满可视区域，规避 iOS 11+ 大标题滚动收缩时的布局跳动。
    nav.navigationBar.prefersLargeTitles = NO;
    self.window.rootViewController = nav;
    [self.window makeKeyAndVisible];
    PFLogInfo(@"Scene 连接完成，root=%@", NSStringFromClass(nav.class));
}

@end
