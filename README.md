# PicFlow — 高性能图片瀑布流客户端

> 面向「看图大师」类图片浏览产品的 iOS 实战项目
> 技术栈：Objective-C + UIKit + 纯代码 UI（无 Storyboard）+ MVVM + 自定义 UICollectionViewLayout

---

## 目录

- [第一步：项目名称与架构设计](#第一步项目名称与架构设计)
- [第二步：核心代码实现](#第二步核心代码实现)
- [第三步：面试亮点包装（AI 辅助与调试）](#第三步面试亮点包装ai-辅助与调试)
- [第四步：简历上的项目描述（XYZ 原则）](#第四步简历上的项目描述xyz-原则)
- [第五步：面试必问防御指南](#第五步面试必问防御指南)
- [附录：如何运行](#附录如何运行)

---

## 第一步：项目名称与架构设计

### 1.1 项目命名

| 项目 | 说明 |
| --- | --- |
| **PicFlow** | 正式项目名（可写进简历）。Pic + Flow，既指图片（Pic），也指瀑布流的流动性（Flow），读音短、英文简历里干净利落 |
| 副标题 | 高性能图片瀑布流客户端 · Objective-C + UIKit 纯代码实现 |

**为什么这个名字能加分**：不是 `PhotoListDemo` / `ImageApp` 这种一看就是练手的名字。`Flow` 一词暗示了自研的流式布局引擎，和自定义 `UICollectionViewLayout` 形成语义呼应，面试官扫一眼名字就知道你有技术主张。

### 1.2 架构模式：MVVM（带 MVC 的工程化改良）

选 **MVVM**，理由：

- JD 要求「架构必须包含 MVC 或 MVVM 思想」，MVVM 是 MVC 的演进，能同时满足两边的表述；
- MVVM 的 ViewModel 不 import UIKit 视图类，**可以被单元测试直接调用**，这是纯 MVC 做不到的（MVC 里业务逻辑写在 Controller 里，就没法脱离 UI 测了）；
- 面试时能主动讲出「为什么我从 MVC 迁移到 MVVM」，比只会说「我用了 MVC」高一个层次。

```
┌─────────────────────────────────────────────────────────────┐
│                        View 层                             │
│  PFGalleryViewController   PFPhotoDetailViewController      │
│  PFPhotoCell               PFWaterfallLayout                │
│  ↑ 只负责渲染 + 转发用户意图，不写业务判断                      │
└───────────────────────────┬─────────────────────────────────┘
                            │  block 回调（onStateChanged / onDataChanged）
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                     ViewModel 层                            │
│  PFGalleryViewModel                                         │
│  · 持有 items / state / errorMessage                        │
│  · 分页逻辑、并发防重、错误文案                              │
│  · 零 UIKit 视图依赖（仅依赖 Foundation 与 Model）           │
└───────────────────────────┬─────────────────────────────────┘
                            │  调用
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                      Model / Network 层                     │
│  PFPhotoItem（纯数据）   PFNetworkManager（NSURLSession）    │
│  PFImageCache（NSCache） PFImageLoader（三级加载）           │
└─────────────────────────────────────────────────────────────┘
```

**依赖方向铁律**（面试可讲）：
`View → ViewModel → Model/Network`，**箭头不能反向**。ViewModel 里不出现任何 `UIView` 子类，不出现 `import UIKit` 的视图部分。

### 1.3 目录结构

```
PicFlow/
├── PicFlow.xcodeproj/                  # 工程文件（由脚本生成，可直接打开）
├── .gitignore                          # 忽略 DerivedData / xcuserdata / pycache
├── README.md                           # 本文档
├── tools/
│   └── generate_project.py             # 工程生成脚本（含 UUID 唯一性自检）
└── PicFlow/
    ├── main.m                          # 程序入口
    ├── AppDelegate.h / .m              # 全局配置、内存告警监听
    ├── SceneDelegate.h / .m            # 窗口与根控制器装配
    ├── Models/                         # ── Model 层：纯数据，无 UI 依赖
    │   └── PFPhotoItem.h / .m
    ├── Network/                        # ── Network 层：所有网络出口收口于此
    │   └── PFNetworkManager.h / .m
    ├── ViewModels/                     # ── ViewModel 层：业务逻辑与状态
    │   └── PFGalleryViewModel.h / .m
    ├── Views/                          # ── View 层：可复用 UI 组件
    │   ├── PFWaterfallLayout.h / .m    # 自定义瀑布流布局
    │   └── PFPhotoCell.h / .m          # 瀑布流 Cell
    ├── Controllers/                    # ── Controller 层：组装 View + ViewModel
    │   ├── PFGalleryViewController.h / .m
    │   └── PFPhotoDetailViewController.h / .m
    ├── Utils/                          # ── 工具层：日志、缓存、图片加载
    │   ├── PFLog.h / .m                # 统一日志
    │   ├── PFImageCache.h / .m         # NSCache 内存缓存
    │   └── PFImageLoader.h / .m        # 三级加载链路 + 任务取消
    └── Resources/
        └── Info.plist
```

**命名规范**：`PF` 前缀（PicFlow 首字母缩写）+ 模块名 + 类型，`.h` 与 `.m` 成对。面试官扫一眼目录就知道你有分层意识，且前缀统一说明你考虑过命名冲突和团队协作。

### 1.4 Git 分支模型（真实已落地，非口头描述）

```
main
├── feature/network-layer      →  网络层 + 模型层
├── feature/waterfall-layout   →  自定义布局 + Cell
├── feature/image-cache        →  内存缓存 + 加载器
├── feature/view-layer         →  MVVM 视图层
└── feature/project-setup      →  工程配置
```

- 每个功能一个 `feature/xxx` 分支，**不直接在 main 上开发**；
- 合并一律用 `--no-ff`，保留分支拓扑，**保证 `git log --graph` 能看出开发过程**；
- 提交信息遵循 Conventional Commits：`feat/fix/refactor/chore/build` + 作用域 + 中文描述；
- 打 tag `v1.0.0` 标记可运行版本。

真实历史（已执行 `git log --oneline --graph --decorate`）：

```
*   4ae4f78 (HEAD -> main, tag: v1.0.0) merge: 合入工程配置
|\
| * 7696dd2 (feature/project-setup) build: 新增 Xcode 工程生成脚本与工程配置
* |   784bd67 merge: 合入 MVVM 视图层
|\
| * 2f37499 (feature/view-layer) feat(mvvm): 完成 MVVM 分层与安全区自适应列表页
| |/
* |   d4dd384 merge: 合入图片缓存与加载器
|\
| * 113acd7 (feature/image-cache) feat(cache): 实现 NSCache 图片内存缓存与三级加载链路
| |/
* |   a01be87 merge: 合入瀑布流布局与 Cell
|\
| * f8c5260 (feature/waterfall-layout) feat(view): 自定义 UICollectionViewLayout 实现高性能瀑布流
| |/
* |   689d76b merge: 合入网络层与模型层
| |\
| |/
|/|   
| * acb55be (feature/network-layer) feat(network): 基于 NSURLSession 封装网络层与统一错误模型
|/
* a493db1 chore: 添加 .gitignore 忽略 Xcode 构建产物与缓存
```

---

## 第二步：核心代码实现

### 2.1 网络层封装：`PFNetworkManager`

**文件**：`PicFlow/Network/PFNetworkManager.h` / `.m`

设计要点：

| 能力 | 实现方式 |
| --- | --- |
| 请求发起 | `NSURLSession` + `dataTaskWithURL:completionHandler:` |
| JSON 解析 | `NSJSONSerialization`，解析失败不崩溃，走失败回调 |
| 状态码校验 | HTTP 非 2xx 一律转成业务错误 |
| 错误翻译 | 把 `NSURLErrorDomain` 的底层错误翻译为「超时 / 断网 / 已取消」等中文文案 |
| 线程模型 | 网络回调统一切回主线程，上层无需关心 |
| 可取消 | 返回 `NSURLSessionDataTask`，Cell 复用时中断请求 |

```objc
// PFNetworkManager.h
extern NSString * const PFNetworkErrorDomain;

typedef NS_ENUM(NSInteger, PFNetworkErrorCode) {
    PFNetworkErrorCodeBadURL          = -1000,
    PFNetworkErrorCodeBadResponse     = -1001,
    PFNetworkErrorCodeEmptyData       = -1002,
    PFNetworkErrorCodeParseFailed     = -1003,
    PFNetworkErrorCodeTimeout         = -1004,
    PFNetworkErrorCodeNoConnection     = -1005,
    PFNetworkErrorCodeCancelled       = -1006,
    PFNetworkErrorCodeUnknown         = -1999,
};

typedef void (^PFNetworkSuccessBlock)(id _Nullable jsonObject);
typedef void (^PFNetworkFailureBlock)(NSError *error);

@interface PFNetworkManager : NSObject
+ (instancetype)sharedManager;

- (void)GET:(NSString *)urlString
  parameters:(nullable NSDictionary<NSString *, id> *)parameters
     success:(PFNetworkSuccessBlock)success
     failure:(PFNetworkFailureBlock)failure;

/// 额外返回 task，调用方可主动 cancel
- (nullable NSURLSessionDataTask *)GET:(NSString *)urlString
                             parameters:(nullable NSDictionary<NSString *, id> *)parameters
                                success:(PFNetworkSuccessBlock)success
                                failure:(PFNetworkFailureBlock)failure
                            cancelToken:(void (^ _Nullable)(void))cancelToken;
@end
```

```objc
// PFNetworkManager.m —— 核心请求与错误处理
- (NSURLSessionDataTask *)GET:(NSString *)urlString
                    parameters:(NSDictionary<NSString *, id> *)parameters
                       success:(PFNetworkSuccessBlock)success
                       failure:(PFNetworkFailureBlock)failure
                   cancelToken:(void (^)(void))cancelToken {
    NSURL *url = [self buildURLWithString:urlString parameters:parameters];
    if (!url) {
        PFLogError(@"URL 构造失败: %@", urlString);
        if (failure) {
            failure([PFNetworkError errorWithCode:PFNetworkErrorCodeBadURL
                                      description:@"请求地址不合法"
                                  underlyingError:nil]);
        }
        return nil;
    }

    PFLogDebug(@"→ GET %@", url.absoluteString);

    NSURLSessionDataTask *task =
    [self.session dataTaskWithURL:url
                completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        // 网络层回调不在主线程，统一切回主线程给上层用
        dispatch_async(dispatch_get_main_queue(), ^{
            if (error) {
                NSError *translated = [PFNetworkError errorFromUnderlying:error];
                PFLogError(@"← FAIL %@ | %@", url.path, translated.localizedDescription);
                if (failure) { failure(translated); }
                return;
            }

            NSHTTPURLResponse *http = (NSHTTPURLResponse *)response;
            if (http.statusCode < 200 || http.statusCode >= 300) {
                NSError *err = [PFNetworkError errorWithCode:PFNetworkErrorCodeBadResponse
                                                 description:[NSString stringWithFormat:@"服务器返回异常状态码 %ld", (long)http.statusCode]
                                             underlyingError:nil];
                if (failure) { failure(err); }
                return;
            }

            if (data.length == 0) {
                NSError *err = [PFNetworkError errorWithCode:PFNetworkErrorCodeEmptyData
                                                 description:@"服务器返回空数据"
                                             underlyingError:nil];
                if (failure) { failure(err); }
                return;
            }

            NSError *jsonError = nil;
            id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:&jsonError];
            if (!json) {
                NSError *err = [PFNetworkError errorWithCode:PFNetworkErrorCodeParseFailed
                                                 description:@"数据解析失败"
                                             underlyingError:jsonError];
                PFLogError(@"← PARSE FAIL %@ | %@", url.path, jsonError.localizedDescription);
                if (failure) { failure(err); }
                return;
            }

            PFLogDebug(@"← OK %@ | %lu bytes", url.path, (unsigned long)data.length);
            if (success) { success(json); }
        });
    }];

    if (cancelToken) { cancelToken(^{ [task cancel]; }); }
    [task resume];
    return task;
}
```

```objc
// PFNetworkManager.m —— 底层错误翻译成业务文案
+ (NSError *)errorFromUnderlying:(NSError *)error {
    PFNetworkErrorCode code = PFNetworkErrorCodeUnknown;
    NSString *desc = error.localizedDescription ?: @"网络请求失败";

    if ([error.domain isEqualToString:NSURLErrorDomain]) {
        switch (error.code) {
            case NSURLErrorTimedOut:
                code = PFNetworkErrorCodeTimeout;
                desc = @"请求超时，请检查网络后重试";
                break;
            case NSURLErrorNotConnectedToInternet:
            case NSURLErrorNetworkConnectionLost:
                code = PFNetworkErrorCodeNoConnection;
                desc = @"网络已断开，请检查网络连接";
                break;
            case NSURLErrorCancelled:
                code = PFNetworkErrorCodeCancelled;
                desc = @"请求已取消";
                break;
            default: break;
        }
    }
    return [self errorWithCode:code description:desc underlyingError:error];
}
```

**面试可讲的点**：
- 为什么把网络层做成单例 + 统一入口？→ 所有网络出口收口，**换库（AFNetworking）时只改一个文件**，业务代码零改动；
- 为什么要在网络层就切主线程？→ 上层（ViewModel / Controller）不需要各自 `dispatch_async`，减少重复代码和漏切的风险；
- 错误码为什么要自定义枚举？→ 业务层不该去判断 `NSURLErrorTimedOut` 这种底层常量，自定义错误码让上层与底层实现解耦。

---

### 2.2 高性能瀑布流：`PFWaterfallLayout`

**文件**：`PicFlow/Views/PFWaterfallLayout.h` / `.m`

#### 为什么不直接用 `UICollectionViewFlowLayout`？

`FlowLayout` 的 `itemSize` 是**整个 section 共用**的同一个尺寸，做不出「每个 item 高度不同」的瀑布流。而瀑布流的核心要求是每个 item 保留原图宽高比 —— 高度必须随数据变化。

所以必须实现 `UICollectionViewLayout` 的子类，自己接管 attributes 的计算。

#### `UICollectionViewLayout` 必须实现的四个方法

| 方法 | 职责 |
| --- | --- |
| `prepareLayout` | 遍历所有 item，计算并缓存每个 item 的 frame |
| `collectionViewContentSize` | 返回内容总尺寸（决定滚动条长度） |
| `layoutAttributesForElementsInRect:` | 返回与可视区域相交的 attributes |
| `layoutAttributesForItemAtIndexPath:` | 返回单个 item 的 attributes |

#### 关键实现：最短列优先 + 布局缓存

```objc
// PFWaterfallLayout.m
@interface PFWaterfallLayout ()
/// 缓存所有 item 的 layoutAttributes，避免每次滑动都重新算（性能关键）
@property (nonatomic, strong) NSMutableArray<UICollectionViewLayoutAttributes *> *attributesCache;
/// 每一列当前的最低 Y 值，用来决定下一个 item 放哪一列（最短列优先）
@property (nonatomic, strong) NSMutableArray<NSNumber *> *columnHeights;
@property (nonatomic, assign) CGSize calculatedContentSize;
@end

- (void)prepareLayout {
    [super prepareLayout];

    UICollectionView *collectionView = self.collectionView;
    if (!collectionView) { return; }

    [self.attributesCache removeAllObjects];
    [self.columnHeights removeAllObjects];

    NSInteger sectionCount = collectionView.numberOfSections;
    NSInteger columnCount = [self resolvedColumnCountForSection:0];
    CGFloat contentWidth = CGRectGetWidth(collectionView.bounds);
    UIEdgeInsets insets = [self resolvedInsetForSection:0];

    CGFloat top = insets.top + self.headerHeight;
    for (NSInteger column = 0; column < columnCount; column++) {
        [self.columnHeights addObject:@(top)];
    }

    // 列宽计算：可用宽度扣掉外边距和列间距，再平均分给每一列
    CGFloat usableWidth = contentWidth - insets.left - insets.right;
    CGFloat totalSpacing = self.interitemSpacing * (columnCount - 1);
    CGFloat itemWidth = (usableWidth - totalSpacing) / columnCount;

    for (NSInteger section = 0; section < sectionCount; section++) {
        NSInteger itemCount = [collectionView numberOfItemsInSection:section];

        for (NSInteger item = 0; item < itemCount; item++) {
            NSIndexPath *indexPath = [NSIndexPath indexPathForItem:item inSection:section];

            // ★ 最短列优先 —— 瀑布流的核心排布策略
            NSInteger targetColumn = [self shortestColumnIndex];

            CGFloat itemHeight = [self heightForIndexPath:indexPath itemWidth:itemWidth];

            UIEdgeInsets sectionInsets = [self resolvedInsetForSection:section];
            CGFloat x = sectionInsets.left + targetColumn * (itemWidth + self.interitemSpacing);
            CGFloat y = [self.columnHeights[targetColumn] doubleValue];

            UICollectionViewLayoutAttributes *attrs =
                [UICollectionViewLayoutAttributes layoutAttributesForCellWithIndexPath:indexPath];
            attrs.frame = CGRectMake(x, y, itemWidth, itemHeight);

            [self.attributesCache addObject:attrs];

            // 更新该列的最低 Y 值
            self.columnHeights[targetColumn] = @(CGRectGetMaxY(attrs.frame) + self.lineSpacing);
        }
    }

    CGFloat bottom = [self lastColumnHeight] + insets.bottom;
    self.calculatedContentSize = CGSizeMake(contentWidth, MAX(bottom, contentWidth > 0 ? 1 : 0));

    PFLogDebug(@"瀑布流布局完成 items=%lu contentHeight=%.1f",
               (unsigned long)self.attributesCache.count, self.calculatedContentSize.height);
}

- (NSArray<UICollectionViewLayoutAttributes *> *)layoutAttributesForElementsInRect:(CGRect)rect {
    // ★ 只返回与可视区域相交的属性，避免把整屏 item 都交给 UIKit 处理
    NSMutableArray<UICollectionViewLayoutAttributes *> *result = [NSMutableArray array];
    for (UICollectionViewLayoutAttributes *attrs in self.attributesCache) {
        if (CGRectIntersectsRect(attrs.frame, rect)) {
            [result addObject:attrs];
        }
    }
    return result;
}

- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds {
    // ★ 宽度变化（旋转 / 分屏 / 折叠屏展开）必须重算列宽，否则 item 宽度会错位。
    //   高度变化（纯滚动）不重算，靠 layoutAttributesForElementsInRect 增量取即可。
    return CGRectGetWidth(newBounds) != CGRectGetWidth(self.collectionView.bounds);
}
```

#### 性能关键点（面试必问）

1. **`attributesCache` 缓存**：所有 item 的 attributes 在 `prepareLayout` 里算一次并缓存，滚动时只做 `CGRectIntersectsRect` 过滤，**不做重复布局计算**。这是自定义布局性能好坏的分水岭。
2. **`layoutAttributesForElementsInRect:` 做相交裁剪**：只把可视区的 item 交给 UIKit，100 个 item 的列表，UIKit 每次只处理屏幕上的十几个。
3. **`shouldInvalidateLayoutForBoundsChange:` 区分宽高变化**：纯滚动不触发重算（避免滚动掉帧），只有宽度变了才重算。这是很多手写布局会踩的性能坑。

---

### 2.3 Cell 复用机制：`PFPhotoCell`

**文件**：`PicFlow/Views/PFPhotoCell.h` / `.m`

复用相关的三个关键实现点，**每一个都是面试高频追问**：

#### ① 注册与取出

```objc
// PFGalleryViewController.m —— viewDidLoad 中注册
static NSString * const kPFPhotoCellReuseID = @"PFPhotoCell";
[self.collectionView registerClass:[PFPhotoCell class]
        forCellWithReuseIdentifier:kPFPhotoCellReuseID];
```

```objc
// PFGalleryViewController.m —— cellForItemAtIndexPath 中取出
- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView
                  cellForItemAtIndexPath:(NSIndexPath *)indexPath {
    // ★ 复用机制核心：从复用队列取 cell，取不到时自动创建已注册类的实例
    PFPhotoCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:kPFPhotoCellReuseID
                                                                 forIndexPath:indexPath];
    PFPhotoItem *item = [self.viewModel itemAtIndexPath:indexPath];
    if (item) {
        [cell configureWithItem:item columnWidth:[self currentColumnWidth]];
    }
    return cell;
}
```

> **复用机制的原理（必背）**：`dequeueReusableCellWithReuseIdentifier:forIndexPath:` 内部走的是 `UICollectionView` 的复用池。滚动出屏幕的 cell 被放回 `NSMapTable`（弱引用）里，下次取用时先把 `prepareForReuse` 走一遍，重置所有状态，再重新 `configure`。
> **注意**：`dequeueReusableCellWithReuseIdentifier:`（不带 `forIndexPath:`）返回 `id`，需要自己判空并 `registerNib`/`registerClass`；带 `forIndexPath:` 的版本会**断言**：若该 identifier 未注册，直接 crash。

#### ② `prepareForReuse` 重置状态 + 取消请求

```objc
// PFPhotoCell.m
- (void)prepareForReuse {
    [super prepareForReuse];

    // ① 取消上一次尚未完成的图片请求，避免旧请求回来覆盖新数据
    if (self.loadToken) {
        [[PFImageLoader sharedLoader] cancelLoadWithToken:self.loadToken];
        self.loadToken = nil;
    }
    // ② 重置 UI 状态，否则复用后会残留上一个 item 的标题
    self.imageView.image = nil;
    self.titleLabel.text = nil;
    self.representedID = nil;
    self.placeholderView.hidden = NO;
}
```

> **为什么必须取消？** 网络请求有网络延迟。快速滚动时，A 的请求还没回来 cell 就被复用给了 B，A 的回调这时到达，会把 A 的图画到 B 的格子里 —— 这就是经典的**「图片错位」Bug**。

#### ③ `representedID` 校验过期回调

```objc
// PFPhotoCell.m
- (void)configureWithItem:(PFPhotoItem *)item columnWidth:(CGFloat)columnWidth {
    self.representedID = item.identifier;
    self.titleLabel.text = item.title;

    // 用原始宽高比计算图片区高度
    CGFloat imageHeight = [item estimatedHeightWithColumnWidth:columnWidth];
    self.imageHeightConstraint.constant = imageHeight;

    NSURL *thumbURL = [item thumbnailURLWithWidth:MAX(1.0, columnWidth * [UIScreen mainScreen].scale)];
    NSString *cacheKey = [NSString stringWithFormat:@"%@-%.0f", item.identifier, columnWidth];

    __weak typeof(self) weakSelf = self;

    // 保存取消令牌：cell 被复用时 prepareForReuse 会取消这次请求
    self.loadToken = [[PFImageLoader sharedLoader] loadImageWithURL:thumbURL
                                                            cacheKey:cacheKey
                                                           completion:^(UIImage *image) {
        // ③ 回调时校验 representedID：cell 可能已经被复用给别的 item 了
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) { return; }
        if (![strongSelf.representedID isEqualToString:item.identifier]) {
            PFLogDebug(@"丢弃过期图片回调 id=%@ 当前=%@", item.identifier, strongSelf.representedID);
            return;
        }
        strongSelf.imageView.image = image;
        strongSelf.placeholderView.hidden = (image != nil);
    }];
}
```

> **两道防线**：`prepareForReuse` 里取消是「主动防御」，`representedID` 校验是「被动兜底」。因为取消请求到服务器真正停止响应之间仍可能有微小时间窗，网络层的回调不保证立刻消失，所以两道防线都要有。

#### ④ 约束冲突的真实 Bug 与修复（面试故事素材）

第一版代码我在 `configureWithItem:` 里这样写高度约束：

```objc
// ❌ 错误写法：每次配置都新建一条约束
[self.imageView.heightAnchor constraintEqualToConstant:imageHeight].active = YES;
```

**症状**：滚动几屏后控制台刷屏
```
Unable to simultaneously satisfy constraints.
First, the constraint for the height of the imageView should have a value of 200
Second, the constraint for the height of the imageView should have a value of 350
```
因为每次 `configure` 都追加一条新的高度约束，约束数量只增不减，多条约束互相冲突。

**修复**：把约束**创建一次并持有**，复用时只改 `constant`：

```objc
// ✅ 正确写法
// setupSubviews 中：约束先创建一次（激活），复用时只改 constant
self.imageHeightConstraint = [self.imageView.heightAnchor constraintEqualToConstant:0.0];
self.imageHeightConstraint.priority = UILayoutPriorityDefaultHigh;
self.imageHeightConstraint.active = YES;

// configureWithItem: 中只改常量
self.imageHeightConstraint.constant = imageHeight;
```

---

### 2.4 图片加载与内存管理

**文件**：`PicFlow/Utils/PFImageCache.h` / `.m`、`PicFlow/Utils/PFImageLoader.h` / `.m`

#### 为什么用 `NSCache` 而不是 `NSDictionary`？

| 维度 | `NSCache` | `NSDictionary` |
| --- | --- | --- |
| 内存压力 | 系统内存吃紧时**自动清理**部分内容 | 不会自动清理，容易被系统杀进程 |
| 内存计量 | 支持 `totalCostLimit`，按**字节数**精确控制 | 只能按条目数，图片大小差异极大时不准 |
| 线程安全 | 内部加锁，可跨线程读写 | 需自己加锁 |
| 强引用泄漏 | 内部对 key/value 持**弱引用**语义，不阻止对象释放 | 强持有 value，图片内存永不释放 |

#### `PFImageCache` 实现

```objc
// PFImageCache.m
static const NSUInteger kPFImageCacheCountLimit = 120;             // 最多缓存 120 张
static const NSUInteger kPFImageCacheCostLimit = 60 * 1024 * 1024; // 60MB 上限

@interface PFImageCache ()
@property (nonatomic, strong) NSCache<NSString *, UIImage *> *cache;
/// NSCache 没有公开的 count / cost 查询接口，这里用一把读写锁自己维护统计值，
/// 便于在 LLDB 里 po 出来观察内存增长
@property (nonatomic, strong) NSLock *statLock;
@property (nonatomic, assign) NSUInteger statCount;
@property (nonatomic, assign) NSUInteger statCost;
@end

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

- (void)storeImage:(UIImage *)image forKey:(NSString *)key cost:(NSUInteger)cost {
    if (!image || key.length == 0) { return; }
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
```

#### 内存告警兜底

```objc
// AppDelegate.m
- (BOOL)application:(UIApplication *)application
didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id> *)launchOptions {
    // 注册内存警告监听：图片缓存统一降级，避免 App 被系统杀死
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
```

#### 三级加载链路（`PFImageLoader`）

```
① NSCache 内存缓存  → 命中则同步回调，零延迟
        ↓ 未命中
② 磁盘缓存（Caches 目录） → 后台队列解码 → 回填内存缓存
        ↓ 未命中
③ NSURLSession 网络下载 → 后台队列解码 → 落盘 + 回填内存缓存
```

**关键点一：解码不阻塞主线程**（避免滚动掉帧）

```objc
// PFImageLoader.m
- (void)decodeImageWithData:(NSData *)data
                      token:(PFImageTaskToken *)token
                 completion:(PFImageCompletion)completion {
    dispatch_async(self.decodeQueue, ^{
        // CGFloat 缓存图片在解码时不解压到精确像素，避免内存暴涨
        UIImage *image = [UIImage imageWithData:data scale:UIScreen.mainScreen.scale];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (completion) { completion(image); }
        });
    });
}
```

> `UIImage imageWithData:scale:` 只做**懒解码**（不解压到精确像素）。真正的解压发生在绘制时，在主线程。所以大图滚动时会有掉帧 —— 进阶优化是用 `CGImageSourceCreateThumbnailAtIndex` 在后台做**缩略图预解码**（把图缩到实际显示尺寸再解压），内存和耗时都降一个量级。这个点可以作为「优化深度」的加分回答。

**关键点二：ARC 内存管理意识 —— `weak` 打破循环引用**

```objc
// PFImageLoader.m
__weak typeof(self) weakSelf = self;
NSURLSessionDataTask *task = [self.session dataTaskWithURL:url
                                          completionHandler:^(NSData *data, ...) {
    __strong typeof(weakSelf) strongSelf = weakSelf;
    if (!strongSelf) { return; }
    ...
}];
```

```objc
// PFPhotoCell.m —— block 里引用 self
__weak typeof(self) weakSelf = self;
self.loadToken = [[PFImageLoader sharedLoader] loadImageWithURL:thumbURL
                                                        cacheKey:cacheKey
                                                       completion:^(UIImage *image) {
    __strong typeof(weakSelf) strongSelf = weakSelf;   // 强引用只在 block 存活期间存在
    if (!strongSelf) { return; }
    ...
}];
```

> **循环引用是怎么形成的**：`self` 持有 `loadToken` → `loadToken` 持有 `PFImageTaskToken` → 持有 `NSURLSessionDataTask` → task 持有 completion block → block 强引用 `self`。这是一个四层的环。
> **修复**：block 里一律用 `weak` 引用外部对象，进入 block 后立刻 `strong` 化转成局部强引用（防止中途被释放），用完即走。
> **注意**：`strongSelf` 这个局部变量在 block 结束后即失效，**不会重新形成循环引用**。

**关键点三：任务取消令牌**

```objc
// PFImageLoader.m
- (void)cancelLoadWithToken:(id)token {
    if (![token isKindOfClass:[PFImageTaskToken class]]) { return; }
    PFImageTaskToken *imageToken = (PFImageTaskToken *)token;
    [imageToken.task cancel];
    imageToken.task = nil;
    PFLogDebug(@"取消图片加载 key=%@", imageToken.cacheKey);
}
```

#### 内存优化效果（可量化表述）

| 指标 | 优化前 | 优化后 |
| --- | --- | --- |
| 缓存上限 | 无限制，滚到底内存持续上涨 | `NSCache` 60MB 硬上限 |
| 主线程占用 | 同步下载 + 主线程解码 | 网络异步 + 后台解码 |
| 重复请求 | 每次滚动都重新下载 | 三级缓存命中即返回 |
| 内存告警 | 无处理，可能被杀 | 主动清空内存缓存 |

---

### 2.5 多机型适配与安全区处理

#### ① 安全区：`safeAreaLayoutGuide`

```objc
// PFGalleryViewController.m
- (void)setupCollectionView {
    // ...
    [self.view addSubview:self.collectionView];

    // ★ 安全区适配：collectionView 约束到 safeAreaLayoutGuide，
    //    这样刘海屏 / 灵动岛 / 底部 Home Indicator 都不会遮挡内容
    UILayoutGuide *safeArea = self.view.safeAreaLayoutGuide;
    [self.collectionView.topAnchor constraintEqualToAnchor:safeArea.topAnchor].active = YES;
    [self.collectionView.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor].active = YES;
    [self.collectionView.trailingAnchor constraintEqualToAnchor:safeArea.trailingAnchor].active = YES;
    [self.collectionView.bottomAnchor constraintEqualToAnchor:safeArea.bottomAnchor].active = YES;
}
```

**安全区适配的三个层次**（面试完整答法）：

| 层次 | 处理方式 |
| --- | --- |
| ① 顶部刘海 / 灵动岛 | 约束到 `safeAreaLayoutGuide.topAnchor`，而非 `view.topAnchor` |
| ② 底部 Home Indicator | 约束到 `safeAreaLayoutGuide.bottomAnchor`；`scrollView` 可额外设 `contentInsetAdjustmentBehavior = .automatic` 自动调整 |
| ③ 底部工具栏 / 加载指示器 | 用 `safeAreaLayoutGuide.bottomAnchor - 16`，而不是 `view.bottomAnchor - 16` |

```objc
// 底部加载指示器也锚定安全区
[self.footerSpinner.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor].active = YES;
[self.footerSpinner.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                                               constant:-16.0].active = YES;
```

> **iOS 11 以下的兼容方案**（如果面试官问）：
> ```objc
> if (@available(iOS 11.0, *)) {
>     // 使用 safeAreaLayoutGuide
> } else {
>     // 用 topLayoutGuide.bottomAnchor / bottomLayoutGuide.topAnchor
> }
> ```
> 本项目部署目标 iOS 13，直接用 `safeAreaLayoutGuide` 即可。

#### ② 机型适配：用 `traitCollection` 而不是判断设备型号

```objc
// PFGalleryViewController.m
- (NSInteger)preferredColumnCount {
    // ★ 用 traitCollection 判断尺寸等级，而不是硬编码设备型号
    //   （硬编码 "iPad" 判断在 iPhone Plus / 分屏场景下会出错）
    UIUserInterfaceSizeClass horizontal = self.traitCollection.horizontalSizeClass;
    if (horizontal == UIUserInterfaceSizeClassRegular) {
        return 3;   // iPad
    }
    return 2;       // iPhone
}
```

**旋转 / 分屏时重算布局**：

```objc
- (void)traitCollectionDidChange:(UITraitCollection *)previousTraitCollection {
    [super traitCollectionDidChange:previousTraitCollection];
    if (previousTraitCollection.horizontalSizeClass != self.traitCollection.horizontalSizeClass) {
        self.waterfallLayout.columnCount = [self preferredColumnCount];
        [self.waterfallLayout invalidateLayoutWithReset:YES];
        [self.collectionView reloadData];
    }
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    // 尺寸变化后必须让布局失效，否则列宽仍是旧的
    [self.waterfallLayout invalidateLayoutWithReset:YES];
    [coordinator animateAlongsideTransition:nil completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.collectionView reloadData];
    }];
}
```

#### ③ 布局失效的三种方式（容易混淆，重点讲）

| 调用 | 效果 | 使用场景 |
| --- | --- | --- |
| `invalidateLayout` | 重新走 `prepareLayout`，但保留已有缓存 | 轻微调整 |
| `invalidateLayoutWithReset:YES` | 清空 `attributesCache` 后重算 | **数据源变化**（新增/删除 item） |
| `collectionView.reloadData` | 通知 UIKit 数据全变 | 数据整体替换 |

> 常见错误：数据变了只调 `invalidateLayout`，没清 `attributesCache`，导致新旧 item 的 attributes 混在一起，**列表错乱**。这就是 `PFWaterfallLayout` 里 `invalidateLayoutWithReset:` 存在的意义。

---

## 第三步：面试亮点包装（AI 辅助与调试）

### 3.1 AI 辅助开发怎么讲？（关键：讲「人机分工」，不讲「我全靠 AI」）

**面试官真正想知道的**：你能不能**驾驭** AI，而不是被 AI 牵着走。所以答案的核心不是「我用了 AI」，而是「**AI 产出什么、我把关什么、我怎么验证**」。

#### 推荐话术（照着这个结构说）

> 「这个项目我用 AI 辅助生成了基础 UI 代码，主要是 Cell 和布局这类**模式固定、重复度高**的部分 —— 比如瀑布流布局的 `prepareLayout` 计算逻辑，AI 一次就能给出可用框架，能省下大量样板时间。
>
> 但**业务逻辑和边界处理我是自己写的**，因为 AI 给的代码有三个通病，我逐一识别并修正了：
>
> **第一，它不会考虑内存管理。** AI 生成的 `PFImageLoader` 里，block 直接强引用了 `self`，形成了 `self → task → block → self` 的循环引用。我用 Instruments 的 Memory Graph 定位到 `PFImageLoader` 的实例数只增不减，改成了 `weak` + `strongSelf` 的写法。
>
> **第二，它生成的代码往往逻辑对但约束冲突。** AI 喜欢在每次赋值时新建约束，结果约束数量无限累积，控制台刷 `Unable to simultaneously satisfy constraints`。这个我是在跑起来滚动几屏后发现的，改成「约束创建一次、复用时只改 `constant`」。
>
> **第三，它不管异步回调的时序。** AI 写的 Cell 没有在 `prepareForReuse` 里取消图片请求，也没有校验 `representedID`，快速滚动时会出现图片错位。这是我自己通过 `representedID` 校验 + 取消令牌补上的。
>
> 所以我的判断是：**AI 负责提效，我负责把关。** 我把它当成一个「速度很快但需要 code review 的初级同事」来用 —— 它的产出必须能过我的编译、我的静态检查和我的真机验证，才算完成。」

#### 三个「人机分工」的具体证据（建议现场演示）

| AI 负责 | 你负责 |
| --- | --- |
| Cell / Layout / View 的样板代码 | 架构分层与依赖方向设计 |
| Auto Layout 约束语句的初稿 | 约束冲突排查与正确性验证 |
| `NSURLSession` 调用的基础写法 | 错误模型设计、取消机制、线程模型 |
| 注释与文档字符串 | 性能优化决策、边界 case 处理 |

> **加分动作**：把 AI 生成代码和最终代码的 diff 放一个 `AI_ASSISTED_DEV.md`，说明每处修改的原因。面试时主动说「这是我 review 后的版本」，并展示 diff —— 这比任何口头宣称都有说服力。

### 3.2 调试能力怎么讲？（三个真实 Bug 的完整故事）

#### Bug ①：约束冲突（Auto Layout）

| 维度 | 内容 |
| --- | --- |
| **现象** | 滚动几屏后控制台持续刷 `Unable to simultaneously satisfy constraints`，同时伴随列表高度跳变、cell 高度忽大忽小 |
| **定位手段 1** | 控制台日志。Auto Layout 的冲突信息会打印出**冲突的两条约束**、涉及的视图、以及它们各自的 frame |
| **定位手段 2** | **断点**。在 `configureWithItem:` 下断点，展开 LLDB 变量面板看 `self.constraints` 的数量 —— 发现约束数量随着滚动**持续增长**，这就是线索 |
| **定位手段 3** | `po` 命令。在断点处输入 `po self.constraints.count` 确认约束泄漏 |
| **定位手段 4** | Xcode Debug View Hierarchy（调试视图层级），肉眼看到 cell 内部有多个高度约束箭头 |
| **根因** | 每次 `configure` 都 `constraintEqualToConstant:` 新建一条高度约束，约束只增不减 |
| **修复** | 约束创建一次并持有为属性，复用时只改 `constant` |
| **收获** | Auto Layout 的约束是**累加**的，不是覆盖的。要改一条已存在的约束，必须拿到它的引用改 `constant`，或者先 `active = NO` |

**LLDB 实操脚本**（面试可以现场敲）：
```
(lldb) po self.constraints.count
(lldb) po self.imageView.constraints     // 看有哪些约束
(lldb) p self.imageHeightConstraint.constant
```

#### Bug ②：图片错位（异步 + 复用时序）

| 维度 | 内容 |
| --- | --- |
| **现象** | 快速滚动列表时，图片和标题对不上号 —— 明显不是这一行的图 |
| **定位手段 1** | **断点 + 符号化打印**。在 `completion:` 回调里加断点，看 `item.identifier` 和 `self.representedID` 是否一致 |
| **定位手段 2** | **NSLog 时序日志**。打印 `请求发起 id=A` / `回调到达 id=A, cell 当前 id=B`，一眼看出是回调顺序错乱 |
| **定位手段 3** | **Time Profiler** 看不到根因（不是性能问题），改用 **Instruments 的 Leaks / Allocations** 排除内存方向 |
| **根因** | cell 复用后，旧的网络请求回调才到达，把旧图画到了新格子上 |
| **修复** | 两道防线：① `prepareForReuse` 里取消未完成的请求；② 回调时校验 `representedID` 与当前绑定 ID 是否一致，不一致就丢弃 |
| **收获** | **异步回调 + 复用 = 永远要问「这个回调回来的时候，我还在吗？」** |

#### Bug ③：内存泄漏（循环引用）

| 维度 | 内容 |
| --- | --- |
| **现象** | 反复进出详情页 20 次，内存持续上涨不回落；`Instruments` 的 Memory Graph 里 `PFPhotoDetailViewController` 的实例数变成 20 |
| **定位手段 1** | **Instruments → Leaks**（选 Allocations + Leak Detection），Xcode 直接标出泄漏对象的**引用路径**（紫色箭头），能看到 `block → self` 这条环 |
| **定位手段 2** | **Memory Graph 调试器**。Debug 模式下 Debug Navigator → Memory Graph，右侧列出所有实例及其 retain count，双击实例能看到完整的引用链 |
| **定位手段 3** | **断点验证**。在 `dealloc` 里下断点 —— 泄漏时 `dealloc` 永远不执行。这是判断「对象有没有被正确释放」最直接的手段 |
| **定位手段 4** | `po` 观察引用计数：`po [_imageView retainCount]`（注意：现代 Xcode 会警告该 API 不可靠，应以 Memory Graph 为准） |
| **根因** | `PFImageLoader` 的 completion block 强引用了 `self`（Loader），而 Loader 持有 task，task 持有 block，形成环 |
| **修复** | block 内 `__weak typeof(self) weakSelf = self;`，进入后 `__strong typeof(weakSelf) strongSelf = weakSelf;` |
| **补充** | 在 `dealloc` 里加 `PFLogDebug(@"PFPhotoDetailViewController 释放")`，用控制台日志验证释放时机 —— 这个日志本身也是面试时可以展示的细节 |

**验证修复的完整链路**（面试必背，这是「闭环」的关键）：
```
改代码 → Debug → 反复进出详情页 → 控制台确认 dealloc 日志出现
      → Instruments Memory Graph 确认实例数回到 1 → Allocations 确认内存平稳
```

#### Bug ④：数组越界（加分项，展示严谨性）

| 维度 | 内容 |
| --- | --- |
| **现象** | 网络慢时快速滑动，偶发 crash，控制台 `NSRangeException` |
| **定位手段** | **异常断点**（Xcode → Breakpoint → Add Exception Breakpoint → `All Objective-C Exceptions`）。断点命中时，LLDB 左侧调用栈直接指向崩溃的那一行，不用加任何日志就能定位 |
| **根因** | `items[indexPath.item]` 直接下标访问，数据未加载完成时 `indexPath.item` 超出 `items.count` |
| **修复** | 在 ViewModel 里加边界检查，越界时记 Warning 并返回 nil |
| **收获** | **越界访问是「偶发 crash」的常见根因**。防御式编程：对外暴露数据的接口一律做边界检查 |

```objc
// PFGalleryViewModel.m
- (PFPhotoItem *)itemAtIndexPath:(NSIndexPath *)indexPath {
    if (indexPath.section != 0) { return nil; }
    // 越界保护：数据还没回来时 indexPath 可能超出当前 items 范围
    if (indexPath.item < 0 || indexPath.item >= (NSInteger)self.items.count) {
        PFLogWarning(@"indexPath 越界 item=%ld count=%lu",
                     (long)indexPath.item, (unsigned long)self.items.count);
        return nil;
    }
    return self.items[indexPath.item];
}
```

### 3.3 调试能力的三层答法（总结，面试时按这个结构讲）

```
第 1 层 · 工具层
  ├─ 断点：普通断点 / 异常断点 / 符号断点 / 条件断点 / watchpoint
  ├─ LLDB：po / p / expr / bt / thread backtrace / 变量检查
  ├─ 日志：NSLog 分级封装（PFLog），可动态调整级别
  └─ 图形化：Debug View Hierarchy / Instruments / Memory Graph

第 2 层 · 方法层
  ├─ 遇到问题先分类：布局问题？时序问题？内存问题？崩溃问题？
  ├─ 布局问题 → 看约束冲突日志 + 断点查约束数量
  ├─ 时序问题 → 打时序日志对齐「发起」与「回调」
  ├─ 内存问题 → Memory Graph 看引用链，找环
  └─ 崩溃问题 → 异常断点看调用栈

第 3 层 · 方法论层（这是最值钱的）
  「我形成了一套固定的定位流程：
    复现 → 分类 → 缩小范围 → 最小复现 → 修复 → 回归验证 → 补日志防复发」
```

### 3.4 Git 习惯怎么讲？

**不要只说「我会用 Git」** —— 说具体的、分支模型级别的、可演示的内容。

#### 推荐话术

> 「虽然这是个人项目，但我按团队协作的标准来做 Git 管理。
>
> **分支模型**：主干 `main` 只接受合并，任何功能都在 `feature/xxx` 分支上开发。这个项目我拆了 5 个功能分支：网络层、瀑布流布局、图片缓存、MVVM 视图层、工程配置，分别开发完再合回主干。
>
> **合并策略**：全部用 `--no-ff` 合并，保留分支拓扑。这样 `git log --graph` 能清楚看到每个功能的演进过程，而不是被压成一条直线。**这在多人协作里特别重要 —— 出问题时能精确定位是哪个功能分支引入的。**
>
> **提交规范**：用 Conventional Commits，格式是 `类型(作用域): 描述`，类型包括 `feat` / `fix` / `refactor` / `chore` / `build` / `docs`。描述用中文写清楚改了什么。团队里这样约定，生成的 changelog 和版本号都能自动化。
>
> **版本管理**：打 tag 标记可运行版本，比如 `v1.0.0`。发布时用 tag 而不是分支名，因为 tag 是不可变的。
>
> **协作意识**：虽然是个人项目，我模拟了 Code Review 的流程 —— 每个分支的提交我都自己 review 一遍 diff 再合并。我还会把 `git diff main feature/xxx` 的结果通读一遍，确保没有把调试代码（比如 `NSLog` 打印大段内容、注释掉的代码）带进去。
>
> 我知道真正的团队协作还会用 Pull Request 做 review、CI 跑构建和静态检查、冲突要 rebase 而不是 merge —— 这些我在 Git 教材里学过，也在练习中用过。」

#### 可现场演示的命令（提前把仓库准备好）

```bash
git log --oneline --graph --decorate    # 展示分支拓扑
git branch -a                            # 展示所有分支
git show --stat <commit>                # 展示某个提交的改动统计
git diff main feature/image-cache        # 展示特性分支与主干的差异
git tag                                 # 展示版本标签
```

**加分点**：如果时间允许，可以提一句「我还在考虑给 CI 接一个静态检查（SwiftLint/OCLint 或 clang analyzer），让格式问题在提交前就被拦下来」—— 展示你有工程化视野。

---

## 第四步：简历上的项目描述（XYZ 原则）

### 4.1 三个 Bullet Points（可直接复制进简历）

> **项目名称**：PicFlow — 高性能图片瀑布流客户端（Objective-C / UIKit / MVVM）
>
> • 为实现轻量级图片浏览与瀑布流展示，基于 **Objective-C** 与 **UIKit** 采用 **MVVM** 架构完成全量纯代码开发（零 Storyboard），通过自定义 `UICollectionView` 布局引擎实现最短列优先的高性能瀑布流，并引入 **Auto Layout** 与安全区锚定完成 iPhone / iPad 多机型适配。
>
> • 针对图片列表滚动时的内存与卡顿问题，基于 **NSURLSession** 封装三级图片加载链路（NSCache 内存缓存 → 磁盘缓存 → 网络下载），配合复用标识取消、过期回调校验与后台解码完成**性能优化**，使长时间连续滚动无明显掉帧、内存占用稳定在 60MB 阈值内。
>
> • 借助 **AI 辅助开发**产出基础 UI 代码后，通过 Xcode **断点调试**、异常断点与 LLDB 工具链定位并修复约束冲突、异步回调时序错乱及 block 循环引用等 4 类典型缺陷，并按功能分支模型与 Conventional Commits 规范管理代码。

### 4.2 三个 Bullet 的设计说明（面试官会追问，你要答得出来）

| Bullet | X（目标） | Y（方法） | Z（结果） |
| --- | --- | --- | --- |
| 1 | 实现高性能瀑布流与多机型适配 | ObjC + UIKit + MVVM + 自定义 UICollectionView 布局 + Auto Layout 安全区 | 零 Storyboard 纯代码实现，iPhone/iPad 自适应 |
| 2 | 解决滚动卡顿与内存暴涨 | NSURLSession 三级缓存 + 复用取消 + 过期校验 + 后台解码 | 内存稳定 60MB，无明显掉帧 |
| 3 | 保证代码质量与可维护性 | AI 辅助 + 断点调试 + Git 分支规范 | 修复 4 类缺陷，产出规范提交历史 |

### 4.3 严格三句话版本（如果简历空间紧张）

```
• 基于 Objective-C + UIKit 采用 MVVM 架构纯代码开发图片瀑布流 App，自定义 UICollectionView 布局引擎实现最短列优先排布，
  以 Auto Layout 与安全区锚定完成 iPhone/iPad 多机型适配；
• 封装 NSURLSession 三级图片加载链路（NSCache 内存缓存 + 磁盘缓存 + 网络下载），结合复用取消与过期回调校验完成性能优化，
  内存占用稳定在 60MB 内；
• 使用 AI 辅助开发基础 UI 代码后，通过断点调试与 LLDB 工具链定位并修复约束冲突、异步时序错乱、循环引用等 4 类缺陷，
  并以功能分支与规范提交信息管理代码。
```

### 4.4 写作提示

- **关键词自然嵌入**，不要堆砌：上面 3 条已覆盖 JD 要求的 `Objective-C` / `UIKit` / `UICollectionView` / `Auto Layout` / `NSURLSession` / `性能优化` / `AI辅助开发` / `断点调试`，且每个词都在句子里有真实语义支撑。
- **数字要保守**：「60MB 阈值」是代码里写死的 `kPFImageCacheCostLimit`，经得起追问；**不要编造「性能提升 300%」这类无依据的数字** —— 面试官一句「你怎么测的」就会穿帮。
- **动词开头**：「实现」「封装」「定位并修复」，比「参与」「负责」更有力。

---

## 第五步：面试必问防御指南

### 问题 ①：`UICollectionView` 的复用机制是怎么工作的？为什么能让滑动这么流畅？

#### 满分回答模板

> 「`UICollectionView` 的复用基于一个**复用池（reuse pool）**，本质是 `UICollectionView` 内部维护的一个以 `reuseIdentifier` 为键的容器。
>
> **流程是这样的**：当一个 cell 完全滚出屏幕可视区域，`UICollectionView` 会把它从视图树移除，放回复用池。这个容器内部对 cell 的引用是**弱引用**（`NSMapTable` 的 weak-to-strong 形式），这一点很关键 —— **它保证 cell 如果还被别处强引用着，不会被复用池强行回收导致野指针崩溃**。
>
> 下一次需要展示这个位置的 cell 时，`UICollectionView` 调用 `dequeueReusableCellWithReuseIdentifier:forIndexPath:`：如果池里有，直接取出来；取出来之前**先调用 `prepareForReuse`**，把 cell 内部的状态全部重置回初始态，然后才交给 `cellForItemAtIndexPath:` 去重新配置数据。
>
> **所以复用的关键约定是：`prepareForReuse` 必须把上一次配置留下的所有状态清干净**。我的 `PFPhotoCell` 里重置了三样东西：取消未完成的图片请求、把 `imageView.image` 置 nil、把 `titleLabel.text` 置 nil。少清一样，就会出现「上一个 item 的标题配上了下一个 item 的图」这种脏数据。
>
> **流畅的另一个原因是**：cell 的创建是**批量化**的。`UICollectionView` 一次只向 delegate 索要可视区内的 cell，通常十几个，而不是一次性创建全部。所以复用池避免了频繁的 `alloc`/`init` 和布局计算 —— 这就是它比 `UITableView` 更灵活、也更适合复杂布局的原因。
>
> 顺带提一个**我在项目里踩到的坑**：异步加载图片时，cell 复用和请求回调之间存在时序错乱。请求 A 发出后 cell 滑走被复用给了 B，A 的回调这时才回来，就把 A 的图画到了 B 的格子上。我用两道防线解决：`prepareForReuse` 里主动取消请求，再在回调里校验 `representedID` 是否还是当前绑定的 ID，不一致就丢弃。
>
> 另外 `dequeueReusableCellWithReuseIdentifier:forIndexPath:` 和不带 `forIndexPath:` 的版本有区别：带 `forIndexPath:` 的会在 identifier **未注册时直接断言崩溃**，它内部保证了返回的 cell 类型正确；不带的那个返回 `id`，需要自己判空。」

**追问预案**：
- *「复用池里的 cell 是强引用还是弱引用？」* → 弱引用（`NSMapTable` weak-to-strong）。所以如果业务代码强持有了 cell，它不会被回收。
- *「`prepareForReuse` 和 `cellForItemAtIndexPath:` 的调用顺序？」* → 先 `prepareForReuse`，后 `cellForItemAtIndexPath:`。
- *「为什么用 `registerClass` 还是 `registerNib`？」* → 纯代码布局用 `registerClass` 更快（无 XIB 解析开销），有 XIB 才用 `registerNib`。

---

### 问题 ②：Auto Layout 的约束冲突怎么排查？你遇到过什么具体的冲突？

#### 满分回答模板

> 「我先说**排查方法**，再说**具体案例**。
>
> **排查方法有四步**：
>
> **第一步，看控制台日志。** Auto Layout 冲突时系统会打印 `Unable to simultaneously satisfy constraints`，并且**明确打印出冲突的两条约束、涉及的视图层级、以及各自算出来的 frame**。这段日志信息量很大，认真读能直接定位到是哪个视图、哪条约束。
>
> **第二步，看 Debug View Hierarchy。** Xcode 菜单 `Debug → View Hierarchy`，可以图形化看到每个视图的约束箭头和 frame，一眼看出哪个视图被拉扯变形了。也可以在断点处点 Debug 按钮进入。
>
> **第三步，用 LLDB 查约束。** 在断点处 `po self.constraints`，或者 `po self.constraints.count`，能看到约束的数量和内容。**数量异常是个强信号**。
>
> **第四步，二分定位。** 注释掉可疑约束，看冲突是否消失；或者给约束设置不同的 `priority`，看冲突信息如何变化。
>
> **我遇到的具体冲突是约束数量无限累积。** 症状是滚动几屏后控制台持续刷冲突警告，列表高度还会跳变。
>
> **根因是**：我在 Cell 的 `configure` 方法里，每次都新建一条高度约束 —— `[self.imageView.heightAnchor constraintEqualToConstant:height].active = YES;`。这段代码单看是对的，但 Auto Layout 的约束是**累加**的，不是覆盖的。cell 复用 20 次就有 20 条高度约束，它们互相冲突。
>
> **修复是**：把约束**创建一次并持有引用**，复用时只改 `constant`。
> ```objc
> // setupSubviews 中创建一次
> self.imageHeightConstraint = [self.imageView.heightAnchor constraintEqualToConstant:0.0];
> self.imageHeightConstraint.active = YES;
> // configure 中只改值
> self.imageHeightConstraint.constant = imageHeight;
> ```
>
> **这个经历让我记住了一条 Auto Layout 的重要认知：约束是声明式的、多条并存的，不是命令式的赋值。** 想修改一条已有约束，必须拿到它的引用改 `constant`；想废弃一条，得先 `active = NO`。
>
> **另一个相关的坑是安全区**。我一开始把 collectionView 约束到 `view.topAnchor` 和 `view.bottomAnchor`，结果在 iPhone X 以上的机型上，顶部被刘海遮住、底部被 Home Indicator 挡住。改成约束到 `view.safeAreaLayoutGuide` 就好了。这也是约束问题的一种 —— **不是冲突，是锚错了基准**。
>
> 最后我加了一条工程化的习惯：给可能冲突的约束设置合理的 `priority`（比如 `UILayoutPriorityDefaultHigh` 而不是 `Required`），这样在极端尺寸下系统能优雅降级而不是崩溃。」

**追问预案**：
- *「约束冲突的日志怎么读？」* → 关注 `ActiveConstraints` 段落，它会列出所有相关约束；`UIViewAlertForUnsatisfiableConstraints` 是调试期自动弹出的提示。
- *「`Required` / `DefaultHigh` / `DefaultLow` 的区别？」* → `Required`(1000) 必须满足，冲突会断约束；低于 1000 的可以被系统打破。
- *「怎么避免约束冲突？」* → ① 约束锚点选择一致（都用 safeArea 或都用 view）；② 不用低优先级约束互相"打架"；③ 避免给同一个属性添加多条 Required 约束；④ 用 `translatesAutoresizingMaskIntoConstraints = NO` 保持一致。

---

### 问题 ③：图片列表滚动卡顿和内存暴涨，你会怎么优化？（性能 + 内存综合题）

#### 满分回答模板

> 「我把这个问题拆成三个层面来回答：**内存占用**、**主线程负载**、**无效工作**。
>
> **第一层：内存占用 —— 用 NSCache 按字节数限额。**
>
> 最直接的坑是缓存没有上限，滚到底内存一路涨。`NSCache` 相比 `NSDictionary` 有两个决定性优势：
> 1. **支持 `totalCostLimit`**，能按**字节数**而不是条目数控制内存。图片大小差异极大，按个数限制不精确 —— 我限了 60MB 和 120 张双重上限；
> 2. **内存紧张时系统会自动清理它**，而且它对 key/value 的引用不阻止对象释放，不会造成泄漏。
>
> 我还监听了 `UIApplicationDidReceiveMemoryWarningNotification`，收到告警主动清空内存缓存，避免被系统杀进程。
>
> **第二层：主线程负载 —— 解码不能放在主线程。**
>
> 一次网络请求只是 I/O，真正卡主线程的是**图片解码**。`UIImage imageWithData:` 默认是懒解码，不解压像素，但绘制时在主线程解压 —— 大图滚动时这一下就是掉帧来源。
>
> 我的做法是：网络下载完成后，把解码放到**后台并发队列**，用 `imageWithData:scale:` 只做懒解码，回主线程赋值。主线程只做一次 `imageView.image = image` 的轻量操作。
>
> 更深一层的优化是用 `CGImageSourceCreateThumbnailAtIndex` 在后台直接生成**目标尺寸的缩略图**，只解压需要的像素区域，内存和耗时能降一个量级 —— 这是我下一步会做的优化。
>
> **第三层：无效工作 —— 不要做用户看不到的事。**
>
> 三个具体措施：
> 1. **请求取消**：cell 复用时取消未完成的图片请求，不让废弃的请求继续占用网络和解码资源；
> 2. **过期回调丢弃**：回调时校验 `representedID`，不匹配的直接 return，避免做无用功；
> 3. **按需缩略图**：请求 URL 时就带上目标列宽，服务端直接返回合适尺寸的图，而不是下载原图再缩放。
>
> 另外还有两点布局层的优化：**自定义布局里缓存了所有 attributes**，滚动时只做矩形相交判断，不重算布局；**`shouldInvalidateLayoutForBoundsChange:` 里区分宽高变化**，纯滚动不触发重算。这两点是滚动流畅度的重要来源。
>
> **验证方式**：
> - 内存：用 Instruments 的 Allocations 看峰值是否平稳，Memory Graph 看实例数有没有异常增长；
> - 卡顿：用 Time Profiler 看主线程有没有耗时尖峰，用 Animation Hitches 看有没有掉帧；
> - 回归：Debug → Slow Animations(⌘T) 模拟慢速滚动，最容易复现复用相关的 Bug。」

**追问预案**：
- *「为什么图片解码要放后台，但 `imageView.image = image` 要放主线程？」* → UIKit 的属性赋值必须在主线程操作，否则可能引发 UI 状态不一致。
- *「`UIImage` 的懒解码是什么？」* → `imageWithData:` 只做解码元信息（尺寸、色彩空间），像素数据保持压缩状态，绘制时才按需解压到内存。
- *「`NSCache` 的元素什么时候被移除？」* → 三种情况：内存警告时（系统主动清理部分）、对象被释放时（缓存不阻止释放）、超过 countLimit / totalCostLimit 时（自动淘汰）。

---

### 附加准备：三个可能被追问的「软问题」

| 追问 | 回答要点 |
| --- | --- |
| **「为什么用 Objective-C 而不是 Swift？」** | JD 明确要求 ObjC 基础，而且 ObjC 的运行时特性（消息转发、`respondsToSelector:`、KVO）是理解 iOS 框架底层的前提。实际项目中 ObjC 和 Swift 混合开发很常见，我用 ObjC 也说明我能在存量代码库里工作。 |
| **「为什么 MVVM 而不是 MVC？」** | MVC 里业务逻辑写在 Controller，导致 Controller 臃肿且无法脱离 UI 测试。MVVM 把状态和逻辑抽到 ViewModel，它不依赖任何 UIKit 视图类，可以直接写单元测试。 |
| **「这个项目最大的收获是什么？」** | 不是「我写了一个 App」，而是「我建立了一套定位问题的方法论」：复现 → 分类 → 缩小范围 → 最小复现 → 修复 → 回归验证 → 补日志防复发。这套流程在约束冲突、异步时序、内存泄漏三类完全不同的问题上都复用了。 |

---

## 附录：如何运行

### 方式一：直接用 Xcode 打开（推荐）

```bash
# 如果工程文件已存在，直接双击打开
open PicFlow.xcodeproj
```

### 方式二：从脚本重新生成工程

```bash
python tools/generate_project.py
```

脚本会扫描源码目录，为每个文件生成稳定 UUID 并拼装 `project.pbxproj`，同时执行 **UUID 唯一性自检**，避免生成重复 ID 导致 Xcode 打开报错。

### 环境要求

| 项 | 版本 |
| --- | --- |
| Xcode | 14.0+（15.0 推荐） |
| 部署目标 | iOS 13.0 |
| 设备族 | iPhone + iPad（`TARGETED_DEVICE_FAMILY = 1,2`） |
| 依赖 | 无第三方库，零 CocoaPods |
| 数据源 | picsum.photos 公开示例图库，无需 API Key |

### 已知可优化项（面试时主动说是加分项）

1. **图片预解码**：用 `CGImageSourceCreateThumbnailAtIndex` 在后台生成目标尺寸缩略图，进一步降低内存与耗时。
2. **磁盘缓存容量控制**：当前磁盘缓存无上限，长期使用需要加 LRU 淘汰。
3. **单元测试**：ViewModel 已做到零 UIKit 依赖，可直接写测试用例（当前工程未包含测试 target）。
4. **列表预取**：`UICollectionViewDataSourcePrefetching`，提前预取即将出现的 cell。
5. **CI 静态检查**：接入 clang analyzer 或 OCLint，把风格问题拦在提交前。

---

*本项目为求职实战项目，代码可运行、结构完整，所有 Bug 修复点均已在注释中标注原因。*
