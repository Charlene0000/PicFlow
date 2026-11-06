# PicFlow

> 高性能图片瀑布流客户端 · Objective-C + UIKit 纯代码实现（无 Storyboard）· MVVM · 自定义 UICollectionViewLayout

---

## 目录

- [简介](#简介)
- [功能特性](#功能特性)
- [技术栈](#技术栈)
- [系统架构](#系统架构)
- [目录结构](#目录结构)
- [环境要求](#环境要求)
- [快速开始](#快速开始)
- [测试与验证](#测试与验证)
- [核心模块设计](#核心模块设计)
- [开发规范](#开发规范)
- [持续集成](#持续集成)
- [文档索引](#文档索引)
- [已知限制与路线图](#已知限制与路线图)
- [许可证](#许可证)

---

## 简介

PicFlow 是一个面向 iOS 的图片瀑布流浏览客户端，采用 Objective-C + UIKit 纯代码开发，架构为 MVVM。项目将瀑布流布局算法、分页状态机、LRU 缓存等平台无关逻辑抽象为可跨平台编译的 C 核心层，并由 Objective-C / UIKit 层负责渲染与交互。

项目不依赖任何第三方库，图库数据默认由 Core Graphics 程序化生成，也可切换到自研 mock server，保证在无外部 API 的环境下可完整运行与测试。

---

## 功能特性

- **自定义瀑布流布局**：基于 `UICollectionViewLayout` 实现最短列优先排布，支持列数自适应、布局属性缓存与可视区域相交裁剪。
- **三级图片加载链路**：NSCache 内存缓存 → 磁盘缓存 → NSURLSession 网络下载，配合后台解码与任务取消。
- **MVVM 分层**：ViewModel 不依赖 UIKit 视图类，可直接进行单元测试。
- **多机型适配**：基于 `safeAreaLayoutGuide` 与 `traitCollection` 完成 iPhone / iPad、旋转与分屏适配。
- **零外部依赖**：无 CocoaPods、无第三方 SDK；图库由 Core Graphics 程序化生成或自研 mock server 提供。
- **跨平台 C 核心层**：布局、分页、缓存、配色、图库等核心算法可在 Windows / macOS / Linux 编译测试。
- **完整验证工具链**：C 单元测试、Python 集成测试、Objective-C 静态自检、布局 SVG 可视化、一键验证脚本。
- **CI**：GitHub Actions 执行 macOS 真编译与三平台 C 核心测试。

---

## 技术栈

| 层级 | 技术 |
| --- | --- |
| 语言 | Objective-C、C、Python（工具链） |
| UI | UIKit、Auto Layout、UICollectionView、Core Graphics |
| 架构 | MVVM |
| 网络 | NSURLSession、NSJSONSerialization |
| 缓存 | NSCache、磁盘缓存 |
| 构建 | Xcode、自研工程生成脚本 |
| 测试 | 自研零依赖 C 测试框架、Python 集成测试 |
| CI | GitHub Actions |

---

## 系统架构

### 分层架构

```text
┌─────────────────────────────────────────────────────────────┐
│                        View 层                              │
│  PFGalleryViewController   PFPhotoDetailViewController      │
│  PFPhotoCell               PFWaterfallLayout                │
│  职责：渲染 + 转发用户意图，不写业务判断                       │
└───────────────────────────┬─────────────────────────────────┘
                            │ block 回调（onStateChanged / onDataChanged）
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                     ViewModel 层                            │
│  PFGalleryViewModel                                         │
│  · 持有 items / state / errorMessage                        │
│  · 分页逻辑、并发防重、错误文案                              │
│  · 零 UIKit 视图依赖（仅依赖 Foundation 与 Model）           │
└───────────────────────────┬─────────────────────────────────┘
                            │ 调用
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                  Model / Network 层                         │
│  PFPhotoItem（纯数据）   PFNetworkManager（NSURLSession）    │
│  PFImageCache（NSCache） PFImageLoader（三级加载）           │
└─────────────────────────────────────────────────────────────┘
```

**依赖方向约定**：`View → ViewModel → Model/Network`，箭头不可反向。ViewModel 中不出现任何 `UIView` 子类。

### 平台无关 C 核心层

布局算法、分页状态机、LRU 缓存、HSL 配色与图库数据均下沉至 `core/`，以纯 C 实现，便于跨平台编译与测试。Objective-C 层通过薄封装调用 C 接口。

---

## 目录结构

```text
PicFlow/
├── PicFlow.xcodeproj/                  # 工程文件（由脚本生成，可直接打开）
├── .github/workflows/ci.yml            # CI：macOS 真编译 + 三平台 C 核心测试
├── .gitignore                          # 忽略 DerivedData / xcuserdata / build
├── README.md                           # 本文档
├── AI_ASSISTED_DEV.md                  # AI 辅助开发记录（问题→定位→修复）
├── core/                               # 平台无关纯 C 核心层（Windows 可编译可测）
│   ├── picflow_layout.h / .c           # 瀑布流布局算法（最短列优先）
│   ├── picflow_paging.h / .c           # 分页加载状态机
│   ├── picflow_cache.h / .c            # LRU 缓存 + 内存预算决策器
│   ├── picflow_palette.h / .c          # HSL 配色引擎
│   └── picflow_catalog.h / .c          # 自研图库（零外部 API）
├── tests/                              # 可在 Windows 直接运行的测试
│   ├── picflow_test.h                  # 自研零依赖测试框架
│   ├── test_layout.c / test_paging.c
│   ├── test_cache.c / test_palette.c
│   └── test_mock_server.py             # 图库服务集成测试（28 项）
├── tools/
│   ├── verify_all.py                   # 一键验证全部
│   ├── run_tests.py                    # C 核心测试运行器
│   ├── check_objc.py                   # Objective-C 静态自检
│   ├── generate_project.py             # 工程生成脚本（含 UUID 唯一性自检）
│   ├── mock_server.py                  # 自研图库服务（零依赖）
│   ├── png_writer.py                   # 手写 PNG 编码器（不用 Pillow）
│   └── preview/preview_svg.c           # 布局可视化工具
├── docs/
│   └── WINDOWS_VERIFY.md               # 无 Mac 环境下的验证指南
└── PicFlow/                            # ObjC / UIKit 层
    ├── main.m                          # 程序入口
    ├── AppDelegate.h / .m              # 全局配置、内存告警监听
    ├── SceneDelegate.h / .m            # 窗口与根控制器装配
    ├── Models/                         # Model 层：纯数据，无 UI 依赖
    │   └── PFPhotoItem.h / .m
    ├── Network/                        # Network 层：所有网络出口收口于此
    │   └── PFNetworkManager.h / .m     # NSURLSession 封装 + 二进制下载通道
    ├── ViewModels/                     # ViewModel 层：业务逻辑与状态
    │   └── PFGalleryViewModel.h / .m   # 复用 C 层分页状态机
    ├── Views/                          # View 层：可复用 UI 组件
    │   ├── PFWaterfallLayout.h / .m    # 自定义瀑布流布局（调用 C 引擎）
    │   ├── PFPhotoCell.h / .m          # 瀑布流 Cell
    │   └── PFArtworkRenderer.h / .m    # 程序化图片生成（Core Graphics）
    ├── Controllers/                    # Controller 层：组装 View + ViewModel
    │   ├── PFGalleryViewController.h / .m
    │   └── PFPhotoDetailViewController.h / .m
    ├── Utils/                          # 工具层：日志、缓存、图片加载
    │   ├── PFLog.h / .m                # 统一日志
    │   ├── PFImageCache.h / .m         # NSCache 内存缓存
    │   └── PFImageLoader.h / .m        # 双通道加载 + C 层内存预算
    └── Resources/
        └── Info.plist
```

**命名规范**：`PF` 前缀 + 模块名 + 类型，`.h` 与 `.m` 成对。

---

## 环境要求

| 项 | 版本 / 说明 |
| --- | --- |
| Xcode | 14.0+（推荐 15.0） |
| 部署目标 | iOS 13.0 |
| 设备族 | iPhone + iPad（`TARGETED_DEVICE_FAMILY = 1,2`） |
| 依赖 | 无第三方库，零 CocoaPods |
| 数据源 | 零外部依赖：默认本地程序化生成，可选自研 mock server |
| 测试环境 | Python 3（仅工具链与集成测试需要） |

---

## 快速开始

### 1. 获取代码

```bash
git clone <https://github.com/Charlene0000/PicFlow>
cd PicFlow
```

### 2. 生成 Xcode 工程

```bash
python tools/generate_project.py
```

脚本会扫描源码目录，为每个文件生成稳定 UUID 并拼装 `project.pbxproj`，同时执行 UUID 唯一性自检，避免生成重复 ID 导致 Xcode 打开报错。

### 3. 打开并运行

```bash
open PicFlow.xcodeproj
```

在 Xcode 中选择模拟器或真机运行即可。工程默认使用本地程序化图片数据，无需启动任何外部服务。

### 4. 可选：启动自研 mock server

如需验证网络链路：

```bash
python tools/mock_server.py
```

然后在工程配置中切换数据源为 mock server 地址。

---

## 测试与验证

### C 核心单元测试

```bash
python tools/run_tests.py
```

覆盖布局算法、分页状态机、LRU 缓存、HSL 配色等模块，零外部依赖，可在 Windows / macOS / Linux 运行。

### 集成测试

```bash
python tests/test_mock_server.py
```

针对自研图库服务执行 28 项集成测试。

### 一键验证

```bash
python tools/verify_all.py
```

依次执行 C 核心测试、集成测试、Objective-C 静态自检与工程生成自检，适合本地提交前或 CI 前使用。

### Objective-C 静态自检

```bash
python tools/check_objc.py
```

### 布局可视化

`tools/preview/preview_svg.c` 可将布局结果导出为 SVG，便于人工检查排布是否符合预期。

### 无 Mac 环境验证

参见 [`docs/WINDOWS_VERIFY.md`](docs/WINDOWS_VERIFY.md)。

---

## 核心模块设计

### 网络层：`PFNetworkManager`

**设计要点**

| 能力 | 实现方式 |
| --- | --- |
| 请求发起 | `NSURLSession` + `dataTaskWithURL:completionHandler:` |
| JSON 解析 | `NSJSONSerialization`，解析失败走失败回调 |
| 状态码校验 | HTTP 非 2xx 转为业务错误 |
| 错误翻译 | 将 `NSURLErrorDomain` 底层错误翻译为超时 / 断网 / 已取消等业务文案 |
| 线程模型 | 网络回调统一切回主线程 |
| 可取消 | 返回 `NSURLSessionDataTask`，调用方可主动取消 |

**自定义错误码**

```objc
typedef NS_ENUM(NSInteger, PFNetworkErrorCode) {
    PFNetworkErrorCodeBadURL          = -1000,
    PFNetworkErrorCodeBadResponse     = -1001,
    PFNetworkErrorCodeEmptyData       = -1002,
    PFNetworkErrorCodeParseFailed     = -1003,
    PFNetworkErrorCodeTimeout         = -1004,
    PFNetworkErrorCodeNoConnection    = -1005,
    PFNetworkErrorCodeCancelled       = -1006,
    PFNetworkErrorCodeUnknown         = -1999,
};
```

网络层统一收口于单例入口，便于后续替换底层实现而不影响业务代码。错误码自定义使上层与 `NSURLErrorDomain` 底层常量解耦。

---

### 瀑布流布局：`PFWaterfallLayout`

`UICollectionViewFlowLayout` 的 `itemSize` 为整个 section 共用，无法表达每个 item 高度不同的瀑布流。因此实现 `UICollectionViewLayout` 子类，自行接管 attributes 计算。

**必须实现的四个方法**

| 方法 | 职责 |
| --- | --- |
| `prepareLayout` | 遍历所有 item，计算并缓存 frame |
| `collectionViewContentSize` | 返回内容总尺寸 |
| `layoutAttributesForElementsInRect:` | 返回与可视区域相交的 attributes |
| `layoutAttributesForItemAtIndexPath:` | 返回单个 item 的 attributes |

**核心策略：最短列优先 + 布局缓存**

```objc
// 核心排布逻辑（节选）
NSInteger targetColumn = [self shortestColumnIndex];
CGFloat itemHeight = [self heightForIndexPath:indexPath itemWidth:itemWidth];
CGFloat x = sectionInsets.left + targetColumn * (itemWidth + self.interitemSpacing);
CGFloat y = [self.columnHeights[targetColumn] doubleValue];

UICollectionViewLayoutAttributes *attrs =
    [UICollectionViewLayoutAttributes layoutAttributesForCellWithIndexPath:indexPath];
attrs.frame = CGRectMake(x, y, itemWidth, itemHeight);

[self.attributesCache addObject:attrs];
self.columnHeights[targetColumn] = @(CGRectGetMaxY(attrs.frame) + self.lineSpacing);
```

**性能要点**

1. `attributesCache` 在 `prepareLayout` 中一次计算并缓存，滚动时只做矩形相交过滤，不重复计算布局。
2. `layoutAttributesForElementsInRect:` 只返回可视区域内的 attributes，降低 UIKit 每帧处理量。
3. `shouldInvalidateLayoutForBoundsChange:` 仅在宽度变化时触发重算，纯滚动不重算，避免掉帧。

---

### 图片加载与缓存

#### `NSCache` 与 `NSDictionary` 的取舍

| 维度 | `NSCache` | `NSDictionary` |
| --- | --- | --- |
| 内存压力 | 系统内存吃紧时自动清理 | 不会自动清理 |
| 内存计量 | 支持 `totalCostLimit`，按字节数控制 | 只能按条目数 |
| 线程安全 | 内部加锁 | 需自行加锁 |
| 引用语义 | 不阻止对象释放 | 强持有 value |

#### 缓存限额

```objc
static const NSUInteger kPFImageCacheCountLimit = 120;             // 最多 120 张
static const NSUInteger kPFImageCacheCostLimit = 60 * 1024 * 1024; // 60MB 上限
```

#### 三级加载链路

```text
① NSCache 内存缓存  → 命中则同步回调
        ↓ 未命中
② 磁盘缓存（Caches 目录） → 后台队列解码 → 回填内存缓存
        ↓ 未命中
③ NSURLSession 网络下载 → 后台队列解码 → 落盘 + 回填内存缓存
```

**关键实现**

- **后台解码**：网络下载完成后在后台并发队列解码，主线程只做 `imageView.image = image` 赋值。
- **任务取消**：`PFImageLoader` 提供取消令牌，Cell 复用时主动取消未完成请求。
- **过期回调校验**：回调时校验 `representedID` 与当前绑定 ID 是否一致，不一致则丢弃，防止图片错位。
- **内存告警**：`AppDelegate` 监听 `UIApplicationDidReceiveMemoryWarningNotification`，主动清空内存缓存。

---

### 多机型与安全区适配

**安全区**

```objc
UILayoutGuide *safeArea = self.view.safeAreaLayoutGuide;
[self.collectionView.topAnchor constraintEqualToAnchor:safeArea.topAnchor].active = YES;
[self.collectionView.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor].active = YES;
[self.collectionView.trailingAnchor constraintEqualToAnchor:safeArea.trailingAnchor].active = YES;
[self.collectionView.bottomAnchor constraintEqualToAnchor:safeArea.bottomAnchor].active = YES;
```

**列数适配**：基于 `traitCollection.horizontalSizeClass` 判断，而非硬编码设备型号。

```objc
- (NSInteger)preferredColumnCount {
    if (self.traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular) {
        return 3;   // iPad
    }
    return 2;       // iPhone
}
```

**旋转 / 分屏**：在 `traitCollectionDidChange:` 与 `viewWillTransitionToSize:withTransitionCoordinator:` 中重置布局并刷新数据。

**布局失效方式**

| 调用 | 效果 | 使用场景 |
| --- | --- | --- |
| `invalidateLayout` | 重新走 `prepareLayout`，保留已有缓存 | 轻微调整 |
| `invalidateLayoutWithReset:YES` | 清空 `attributesCache` 后重算 | 数据源变化（新增 / 删除 item） |
| `collectionView.reloadData` | 通知 UIKit 数据全变 | 数据整体替换 |

---

## 开发规范

### 分支模型

```text
main
├── feature/network-layer      →  网络层 + 模型层
├── feature/waterfall-layout   →  自定义布局 + Cell
├── feature/image-cache        →  内存缓存 + 加载器
├── feature/view-layer         →  MVVM 视图层
└── feature/project-setup      →  工程配置
```

- 每个功能一个 `feature/xxx` 分支，不直接在 `main` 上开发。
- 合并统一使用 `--no-ff`，保留分支拓扑，便于通过 `git log --graph` 追溯开发过程。
- 打 tag `v1.0.0` 标记可运行版本。

### 提交信息规范

遵循 Conventional Commits：`feat/fix/refactor/chore/build/docs` + 作用域 + 中文描述。

示例：

```text
feat(network): 基于 NSURLSession 封装网络层与统一错误模型
feat(cache): 实现 NSCache 图片内存缓存与三级加载链路
feat(view): 自定义 UICollectionViewLayout 实现高性能瀑布流
build: 新增 Xcode 工程生成脚本与工程配置
```

### 代码风格

- 统一 `PF` 前缀，`.h` / `.m` 成对。
- ViewModel 不引入 UIKit 视图类。
- 网络出口统一收口于 `PFNetworkManager`。
- block 中引用外部对象使用 `weak` + `strongSelf`，避免循环引用。
- 对外暴露数据接口做边界检查。

---

## 持续集成

`.github/workflows/ci.yml` 执行以下任务：

- macOS 环境下真实编译 Xcode 工程；
- 三平台（Windows / macOS / Linux）C 核心层编译与单元测试；
- 集成测试与静态自检。

本地提交前建议执行：

```bash
python tools/verify_all.py
```

---

## 文档索引

| 文档 | 说明 |
| --- | --- |
| [`AI_ASSISTED_DEV.md`](AI_ASSISTED_DEV.md) | AI 辅助开发记录：问题 → 定位 → 修复 |
| [`docs/WINDOWS_VERIFY.md`](docs/WINDOWS_VERIFY.md) | 无 Mac 环境下的验证指南 |
| `tools/preview/preview_svg.c` | 布局可视化工具源码 |

---

## 已知限制与路线图

### 已知限制

1. 磁盘缓存当前无容量上限，长期使用需要补充 LRU 淘汰策略。
2. 图片解码目前为懒解码，大图滚动时仍有优化空间。
3. 工程暂未包含 XCTest target；ViewModel 已做到零 UIKit 依赖，可直接补充测试用例。

### 路线图

- [ ] 使用 `CGImageSourceCreateThumbnailAtIndex` 在后台生成目标尺寸缩略图，降低内存与解码耗时。
- [ ] 为磁盘缓存增加容量上限与 LRU 淘汰。
- [ ] 补充 ViewModel 单元测试 target。
- [ ] 接入 `UICollectionViewDataSourcePrefetching` 预取即将出现的 cell。
- [ ] CI 接入 clang analyzer 或 OCLint 静态检查。

---

## 许可证

本项目为个人实战项目，未声明开源许可证。如需开源，建议补充 MIT License。

---

## 致谢

- 感谢 Core Graphics、NSURLSession、NSCache 等系统框架提供的稳定基础能力。
- 项目中的 AI 辅助开发记录见 [`AI_ASSISTED_DEV.md`](AI_ASSISTED_DEV.md)。
