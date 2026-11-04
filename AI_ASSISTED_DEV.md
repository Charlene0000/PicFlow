# AI 辅助开发记录（AI-Assisted Development Log）

> 这份文档用于证明：**AI 在本项目里的角色是「提速的初级同事」，而不是「替代决策的人」。**
> 每一条都记录了 AI 产出、我的问题、我的修复、验证方式。

---

## 使用原则

| 原则 | 落地方式 |
| --- | --- |
| **AI 只碰模式固定的部分** | Cell 样板、Auto Layout 约束语句、布局计算框架 |
| **业务决策自己拿** | 架构分层、错误模型、取消机制、边界 case |
| **AI 产出必须过三关** | ① 编译通过 ② 静态检查无警告 ③ 真机验证行为正确 |
| **每处修改都记录原因** | 本文档 |

---

## 记录 ①：Cell 的 Auto Layout 约束

### AI 产出

```objc
// ❌ AI 给的版本（首次编译能过，运行几屏后约束冲突）
- (void)configureWithItem:(PFPhotoItem *)item {
    self.imageView.image = [UIImage new];
    [self.imageView.heightAnchor constraintEqualToConstant:200].active = YES;
    [self.titleLabel.topAnchor constraintEqualToAnchor:self.imageView.bottomAnchor].active = YES;
}
```

### 我发现的问题

1. **约束只增不减**：每次 `configure` 都新建约束，cell 复用 N 次就有 N 条高度约束。
2. **高度写死 200**：瀑布流要求按图片原始宽高比动态计算高度。
3. **`[UIImage new]` 是占位图**：应该显示真实的异步加载结果。

### 我的修复

```objc
// ✅ 约束创建一次并持有
@property (nonatomic, strong, nullable) NSLayoutConstraint *imageHeightConstraint;

// setupSubviews:
self.imageHeightConstraint = [self.imageView.heightAnchor constraintEqualToConstant:0.0];
self.imageHeightConstraint.priority = UILayoutPriorityDefaultHigh;
self.imageHeightConstraint.active = YES;

// configure: 只改 constant
CGFloat imageHeight = [item estimatedHeightWithColumnWidth:columnWidth];
self.imageHeightConstraint.constant = imageHeight;
```

### 验证方式

- **控制台**：修复前滚动几屏刷 `Unable to simultaneously satisfy constraints`，修复后无输出。
- **断点 + LLDB**：在 `configure` 下断点，`po self.constraints.count`，修复前数值持续增长，修复后恒定。

---

## 记录 ②：图片加载的 block 引用

### AI 产出

```objc
// ❌ AI 给的版本（循环引用）
[[PFImageLoader sharedLoader] loadImageWithURL:url completion:^(UIImage *image) {
    self.imageView.image = image;   // 强引用 self
}];
```

### 我发现的问题

`self`（Cell）→ `loadToken` → `PFImageTaskToken` → `NSURLSessionDataTask` → `completion block` → `self`，形成**四层循环引用**。Cell 永远不释放。

### 我的修复

```objc
// ✅ weak 引用 + block 内 strong 化
__weak typeof(self) weakSelf = self;
self.loadToken = [[PFImageLoader sharedLoader] loadImageWithURL:url
                                                        cacheKey:key
                                                       completion:^(UIImage *image) {
    __strong typeof(weakSelf) strongSelf = weakSelf;
    if (!strongSelf) { return; }
    strongSelf.imageView.image = image;
}];
```

### 验证方式

- **断点**：在 `PFPhotoCell` 的 `dealloc` 下断点，反复进出列表页 —— 修复前 `dealloc` 不执行，修复后执行。
- **Instruments → Leaks**：修复前报 `PFPhotoCell` 泄漏，引用路径清晰指向 `block → self`。
- **Memory Graph**：修复后实例数稳定。
- **NSLog**：`dealloc` 里打印释放日志，控制台确认时机。

---

## 记录 ③：异步回调的时序

### AI 产出

```objc
// ❌ AI 给的版本（图片错位）
- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView
                  cellForItemAtIndexPath:(NSIndexPath *)indexPath {
    PFPhotoCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:@"cell"
                                                                 forIndexPath:indexPath];
    PFPhotoItem *item = self.items[indexPath.item];
    [self.loader loadImageWithURL:item.url completion:^(UIImage *image) {
        cell.imageView.image = image;   // cell 可能已被复用给别的 item
    }];
    return cell;
}
```

### 我发现的问题

AI 的版本有两个致命问题：
1. **没有取消请求**：`prepareForReuse` 里什么都没做。
2. **没有校验归属**：回调里直接给 `cell` 赋值，而 `cell` 此时可能已经是另一个 item 的格子了。

而且 `self.items[indexPath.item]` 直接下标访问，**数据未加载完时越界崩溃**。

### 我的修复（三道防线）

```objc
// 防线 1：prepareForReuse 取消未完成的请求
- (void)prepareForReuse {
    [super prepareForReuse];
    if (self.loadToken) {
        [[PFImageLoader sharedLoader] cancelLoadWithToken:self.loadToken];
        self.loadToken = nil;
    }
    self.imageView.image = nil;
    self.titleLabel.text = nil;
    self.representedID = nil;
}

// 防线 2：回调时校验 representedID
if (![strongSelf.representedID isEqualToString:item.identifier]) {
    PFLogDebug(@"丢弃过期图片回调 id=%@ 当前=%@", item.identifier, strongSelf.representedID);
    return;
}

// 防线 3：数据访问做边界检查
- (PFPhotoItem *)itemAtIndexPath:(NSIndexPath *)indexPath {
    if (indexPath.item < 0 || indexPath.item >= (NSInteger)self.items.count) {
        PFLogWarning(@"indexPath 越界 item=%ld count=%lu",
                     (long)indexPath.item, (unsigned long)self.items.count);
        return nil;
    }
    return self.items[indexPath.item];
}
```

### 验证方式

- **时序日志**：打印「请求发起 id=A」与「回调到达 id=A, cell 当前 id=B」，确认错乱场景。
- **异常断点**：`All Objective-C Exceptions` 命中越界崩溃，调用栈直接指向崩溃行。
- **快速滚动验证**：Debug → Slow Animations（⌘T）下连续快速滚动，确认无错位、无崩溃。

---

## 记录 ④：自定义布局的失效时机

### AI 产出

```objc
// ❌ AI 给的版本
- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds {
    return YES;   // 任何变化都重算
}
```

以及在数据变化时只调 `invalidateLayout`，没清 `attributesCache`。

### 我发现的问题

1. **纯滚动也重算**：滚动会持续改变 bounds，`return YES` 导致每次滚动都重跑 `prepareLayout`，**滚动必然掉帧**。
2. **缓存未清**：数据源变化后 `attributesCache` 里残留旧 item 的 attributes，导致**列表错乱**。

### 我的修复

```objc
// 只在宽度变化时重算（旋转 / 分屏 / 折叠屏）
- (BOOL)shouldInvalidateLayoutForBoundsChange:(CGRect)newBounds {
    return CGRectGetWidth(newBounds) != CGRectGetWidth(self.collectionView.bounds);
}

// 数据变化时强制清缓存
- (void)invalidateLayoutWithContext:(UICollectionViewLayoutInvalidationContext *)context {
    if (context.invalidateEverything || context.invalidateDataSourceCounts) {
        [self.attributesCache removeAllObjects];
        [self.columnHeights removeAllObjects];
    }
    [super invalidateLayoutWithContext:context];
}

// 便捷方法
- (void)invalidateLayoutWithReset:(BOOL)reset {
    if (reset) {
        [self.attributesCache removeAllObjects];
        [self.columnHeights removeAllObjects];
    }
    [self invalidateLayout];
}
```

### 验证方式

- **Instruments → Animation Hitches**：修复前快速滚动有掉帧，修复后平滑。
- **日志**：`prepareLayout` 里的 `PFLogDebug(@"瀑布流布局完成 items=%lu ...")` 可以数出滚动过程中重算了几次 —— 修复后纯滚动应该是 0 次。
- **旋转验证**：Debug → Device → Rotate Left，确认列宽重算正确、无 item 错位。

---

## 记录 ⑤：生成 Xcode 工程文件

### AI 产出

手写 `project.pbxproj`（几百行 UUID 和嵌套结构）。

### 我发现的问题

手写的工程文件有两处硬伤，**Xcode 打开会直接报错**：
1. **UUID 重复**：同一个 group 节点被渲染两次（顶层循环 + 递归），UUID 撞车；
2. **`rootObject` 与 `PBXProject` 的 UUID 不一致**：rootObject 指向根 Group 的 ID，而 `PBXProject` 用了另一个 ID，工程加载失败。

### 我的修复

改为**脚本化生成 + 自检**：

```python
# tools/generate_project.py
# 1. 用 md5(key) 生成稳定 UUID，重复生成结果一致（便于 diff）
# 2. 递归渲染目录树，子目录只渲染一次
# 3. 生成后做 UUID 唯一性自检
check = open(pbx_path, encoding="utf-8").read()
ids = collections.Counter()
for line in check.splitlines():
    ...
dup = [k for k, v in ids.items() if v > 1]
if dup:
    print("✗ 发现重复 UUID: %s" % dup)
    sys.exit(2)
```

### 验证方式

- **脚本自检**：输出「✓ 工程生成完成，UUID 唯一性自检通过」。
- **Xcode 打开**：`open PicFlow.xcodeproj` 能正常加载，无警告。
- **结构自检脚本**：对所有 `.h` / `.m` 检查括号配平、`@interface`/`@implementation`/`@protocol` 与 `@end` 数量配平。

```bash
# 检查结果
ObjC 文件数: 25
✓ 所有 ObjC 文件结构自检通过（括号配平 + 声明/@end 配平 + 实现类存在）
```

---

## 汇总表（可直接在面试中口述）

| # | AI 产出的部分 | 我发现的问题 | 定位手段 | 我的修复 | 验证方式 |
| --- | --- | --- | --- | --- | --- |
| 1 | Cell 的 Auto Layout 约束 | 约束无限累积导致冲突 | 控制台冲突日志 + `po self.constraints.count` | 约束创建一次，复用只改 `constant` | 冲突日志消失、约束数恒定 |
| 2 | 图片加载 block | 循环引用导致 Cell 泄漏 | Instruments Leaks / Memory Graph / `dealloc` 断点 | `weak` + `strongSelf` | `dealloc` 执行、实例数稳定 |
| 3 | 异步图片回调 | 图片错位 + 数组越界崩溃 | 时序日志 + 异常断点 | 取消令牌 + `representedID` 校验 + 边界检查 | 快速滚动无错位无崩溃 |
| 4 | 布局失效逻辑 | 滚动掉帧 + 列表错乱 | Animation Hitches + 重算次数日志 | 只在宽度变化时失效 + 清缓存 | 滚动流畅、旋转正常 |
| 5 | `project.pbxproj` | UUID 重复无法打开 | Xcode 打开报错 | 脚本生成 + UUID 自检 | Xcode 正常加载 |

---

## 面试时的关键表述

> 「我用 AI 加速了样板代码的产出，但**每一个 AI 的产出我都做了三件事**：找出它没考虑到的边界、给它加防御、验证它的行为。
>
> 最有代表性的三个例子是：它不会处理 block 的循环引用、它生成的约束会不断累积、它不管异步回调和 cell 复用的时序关系。这三个问题**编译都没问题、运行初期也正常**，只有真正滚动起来、真正反复进出页面才会暴露 —— 所以我理解的 AI 辅助开发不是「生成完就交付」，而是「生成完还要用真实场景压测」。
>
> 我的判断标准很简单：**AI 写的代码如果我不愿意在 code review 里签字，那它就不能进主干。**」
