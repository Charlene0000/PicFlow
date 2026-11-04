# 在 Windows 上验证 PicFlow（无需 macOS / Xcode）

> 这份文档回答一个现实问题：**iOS 项目怎么在没有 Mac 的机器上自证？**

---

## 核心思路：把可验证的部分从 UIKit 里拆出来

iOS 工程的**编译与真机运行**必须有 Xcode，这一点无法绕过。但项目里真正容易出 bug、
也最值得验证的**算法逻辑**，没必要绑死在 UIKit 上。

所以 PicFlow 采用分层策略：

```
┌──────────────────────────────────────────────────────────┐
│  ObjC / UIKit 层      PicFlow/                            │
│  ─ 渲染、交互、Auto Layout、NSURLSession、Core Graphics   │
│  ─ 只做「取数据 → 调 C 引擎 → 画出来」                   │
└───────────────────────┬──────────────────────────────────┘
                        │  C 接口调用
                        ↓
┌──────────────────────────────────────────────────────────┐
│  纯 C 核心层          core/                              │
│  ─ picflow_layout.c    瀑布流布局算法                     │
│  ─ picflow_paging.c    分页状态机                         │
│  ─ picflow_cache.c     LRU 缓存 + 内存预算                │
│  ─ picflow_palette.c   HSL 配色引擎                       │
│  ─ picflow_catalog.c   自研图库                           │
│  ─ 零平台依赖，Windows / Linux / macOS 都能编译            │
└──────────────────────────────────────────────────────────┘
```

**关键点：iOS 端与测试端跑的是同一份实现。**
不存在「单元测试里是对的，真机上不对」这种最讨厌的情况。

---

## 一条命令跑完全部验证

```bash
python tools/verify_all.py
```

输出示例：

```
  1/5  C 核心算法单元测试            →  38 组测试，0 失败
  2/5  Objective-C 静态自检          →  37 文件 / 4070 行，无结构性问题
  3/5  自研图库服务集成测试          →  28 项断言，0 失败
  4/5  Xcode 工程生成与结构自检      →  UUID 唯一性自检通过
  5/5  瀑布流布局可视化              →  输出 SVG，排布自检通过
```

---

## 逐项说明

### 1. C 核心单元测试

```bash
python tools/run_tests.py
```

用 MinGW gcc 编译 4 个测试套件并运行。覆盖：

| 套件 | 组数 | 覆盖内容 |
| --- | --- | --- |
| `test_layout.c` | 11 | 列宽计算、最短列优先、绝对高度、无重叠、无越界、非法输入降级、边界情况 |
| `test_paging.c` | 10 | 并发保护、页码递增、末页判定（不足一页 / 返回 0 条 / 达上限）、失败保留数据、下拉刷新 |
| `test_cache.c` | 10 | 内存预算、LRU 淘汰顺序（最久未用而非最早插入）、字节上限、覆盖写、命中率、空指针安全 |
| `test_palette.c` | 7 | HSL 往返一致性、边界值、确定性、伪随机分布、互补/三角/黄金分割和谐关系、图库宽高比覆盖 |

**测试框架也是自研的**（`tests/picflow_test.h`），刻意不引入任何第三方框架 ——
这样在任何平台上都能用同一条命令跑，不需要联网下载依赖。

### 2. Objective-C 静态自检

```bash
python tools/check_objc.py
```

在无法编译 ObjC 的情况下，检查那些**编译期必然报错**的结构性问题：

- 括号配平（`{}` `()` `[]`）
- `@interface` / `@implementation` / `@protocol` 与 `@end` 数量配平
- `#import "xxx.h"` 指向的头文件真实存在
- 私有方法是否在头文件或 class extension 中声明
- **ObjC 调用的 C 函数是否在 `core/*.h` 中声明过** ← 这条能抓住「C 函数改名后 ObjC 侧忘同步」这类跨语言边界的典型错误

> 这不能替代真机编译（那由 CI 负责），但能在提交前拦下绝大多数低级错误。

### 3. 自研图库服务集成测试

```bash
python tests/test_mock_server.py
```

启动真实的 HTTP 服务并验证：

- `/catalog.json` 返回结构合法的 JSON，字段齐全
- 分页参数 `page` / `limit` 正确切片，`limit` 上限被钳制
- `/img/<id>.png` 返回**合法的 PNG**（逐 chunk 校验 CRC32）
- 不同条目生成不同尺寸、不同内容
- 越界 ID → 404，非法 ID → 400，未知路径 → 404
- 同一 ID 两次请求内容完全一致（可缓存）

### 4. Xcode 工程生成

```bash
python tools/generate_project.py
```

扫描源码 → 生成 `project.pbxproj` → **UUID 唯一性自检**。

> 为什么要脚本生成而不是手写 pbxproj？
> 手写时极易出现 UUID 重复或 `rootObject` 与 `PBXProject` 不一致，
> 这两类错误会让 Xcode 直接打不开工程，而且报错信息很不直观。
> 脚本化 + 自检把这个问题从「靠运气」变成「有保障」。

### 5. 布局可视化（最直观的一项）

```bash
gcc -std=c99 -O1 -I core tools/preview/preview_svg.c \
    core/picflow_layout.c core/picflow_palette.c core/picflow_catalog.c \
    -o build/preview/preview_svg -lm

./build/preview/preview_svg build/preview/iphone.svg 2 375 24   # iPhone 双列
./build/preview/preview_svg build/preview/ipad.svg   3 768 24   # iPad 三列
```

用**同一个 C 布局引擎**渲染出真实排布结果，输出 SVG 用浏览器打开即可。
卡片位置、尺寸、配色、图案全部来自真实算法，不是示意图。

生成前会跑一次布局自检（无重叠、无越界），不通过就不出图 ——
避免给出错误的结果误导判断。

---

## 没有 Mac 时，如何证明 iOS 代码能编译？

用 **GitHub Actions 的 macOS runner**。仓库已内置 `.github/workflows/ci.yml`：

| 任务 | Runner | 作用 |
| --- | --- | --- |
| `core-tests` | Ubuntu / Windows / macOS | 三平台编译并运行 C 核心测试 + 图库集成测试 |
| `ios-build` | macos-latest (Xcode 15.4 / 16.2) | 用**真实 Xcode** 编译 iOS 工程并产出 `.app` |

推代码上去，CI 就能给出「在 Xcode 15.4 和 16.2 下均编译通过」的证据。
本地 Windows 上开发，Mac 上出包，两边都不耽误。

---

## 真机跑通 NSURLSession 链路

默认模式是**本地程序化生成**，完全不需要网络。但 JD 要求体现 `NSURLSession`，
所以项目自带一个自研图库服务：

```bash
python tools/mock_server.py
```

```
  图库接口 : http://127.0.0.1:8765/catalog.json?page=1&limit=24
  图片接口 : http://127.0.0.1:8765/img/0.png
```

然后在 App 里点左上角「本地」切到「远程」：

- **模拟器**：`127.0.0.1` 直接可用
- **真机**：改成电脑的局域网 IP（`http://192.168.x.x:8765`），
  即 `PicFlow/ViewModels/PFGalleryViewModel.m` 里的 `kPFGalleryRemoteURL`

**为什么自己写服务而不用现成的免费图床 API？**

| 维度 | 现成免费 API | 自研 mock server |
| --- | --- | --- |
| 稳定性 | 可能限流、挂掉、改接口 | 永远可用，代码在自己手里 |
| 可复现性 | 网络波动导致 CI 不稳定 | 完全确定性 |
| 授权风险 | 第三方图片版权不明 | 图片是自己程序化生成的，零风险 |
| 面试说服力 | 「我调了个接口」 | 「我自己写了个服务，还手写了 PNG 编码」 |

---

## 环境准备（Windows）

需要两样东西：

1. **Python 3.8+** —— 跑测试与工具链
   ```powershell
   winget install Python.Python.3.12
   ```

2. **MinGW-w64 (GCC)** —— 编译 C 核心
   ```powershell
   winget install BrechtSanders.WinLibs.POSIX.UCRT
   ```
   装完重开终端，确认：
   ```powershell
   gcc --version
   ```

两项都有之后，`python tools/verify_all.py` 就能跑通全部验证。

---

## 面试时怎么讲这件事

> 「我的开发机是 Windows，没有 Mac。但 iOS 项目最难的部分不是写 UIKit，
> 而是**算法逻辑的正确性** —— 而这部分没必要绑死在 UIKit 上。
>
> 所以我把瀑布流布局算法、分页状态机、LRU 缓存、HSL 配色引擎这四块
> 抽成了**纯 C 的核心层**，iOS 端通过 C 接口调用。
>
> 这样做带来三个结果：
> 第一，我在 Windows 上就能跑 38 组单元测试，**每次提交都有 CI 验证**；
> 第二，我写了个 SVG 可视化工具，用**同一个引擎**渲染出排布结果，肉眼就能验证对不对 ——
> 事实上这个工具帮我抓到一个 bug：高度回调漏乘了列宽，导致所有 item 高度只有零点几像素，
> 而当时的单元测试只断言了『无重叠』，没断言绝对高度，所以没发现；
> 第三，iOS 端和测试端跑的是**同一份实现**，不存在『测试对、真机不对』的问题。
>
> 至于 UIKit 那部分的编译验证，我用 GitHub Actions 的 macOS runner 做 ——
> 推上去 CI 就在 Xcode 15.4 和 16.2 两个版本下真实编译一遍。
>
> 我认为这体现的是一种**工程判断力**：知道哪些代码必须依赖平台，
> 哪些不必，以及怎么用最小的代价把质量保障做上去。」

这段话的杀伤力在于：它同时展示了**架构能力**、**质量意识**和**主动解决问题的能力**，
而且每一条都有可运行的证据，不是空谈。
