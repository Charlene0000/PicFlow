#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_all.py —— 一键验证全部可验证部分

在**没有 macOS / Xcode** 的机器上，这条命令能证明：
  1. C 核心算法正确（38 组单元测试）
  2. Objective-C 代码结构无误（静态自检）
  3. 自研图库服务的 HTTP 链路可用（28 项集成测试）
  4. Xcode 工程可正常生成（pbxproj 结构自检）
  5. 布局可视化可产出（排布结果肉眼可验）

用法：
    python tools/verify_all.py
"""

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PY = sys.executable


def run(title, args, cwd=ROOT):
    print("\n" + "=" * 64)
    print("  %s" % title)
    print("=" * 64)
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    output = (result.stdout or "") + (result.stderr or "")
    # 精简输出：只保留关键行
    for line in output.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        if any(key in stripped for key in (
            "共 ", "失败", "结果", "通过", "扫描文件", "代码行数", "类/协议",
            "方法数", "✓", "x ", "工程生成", "已生成", "布局自检", "编译告警",
        )):
            print("  " + stripped)
    if result.returncode != 0:
        print("\n  [失败] %s 返回码 %d" % (title, result.returncode))
        if output.strip():
            print("  ---- 完整输出 ----")
            for line in output.splitlines()[-40:]:
                print("  " + line)
    else:
        print("\n  [通过] %s" % title)
    return result.returncode == 0


def main():
    results = []

    results.append(("C 核心单元测试",
                    run("1/5  C 核心算法单元测试",
                        [PY, os.path.join("tools", "run_tests.py")])))
    results.append(("Objective-C 静态自检",
                    run("2/5  Objective-C 静态自检",
                        [PY, os.path.join("tools", "check_objc.py")])))
    results.append(("图库服务集成测试",
                    run("3/5  自研图库服务集成测试",
                        [PY, os.path.join("tests", "test_mock_server.py")])))
    results.append(("Xcode 工程生成",
                    run("4/5  Xcode 工程生成与结构自检",
                        [PY, os.path.join("tools", "generate_project.py")])))

    # 5. 布局可视化
    print("\n" + "=" * 64)
    print("  5/5  瀑布流布局可视化")
    print("=" * 64)
    ok = True
    try:
        import shutil
        cc = shutil.which("gcc") or shutil.which("cc") or shutil.which("clang")
        if not cc:
            print("  跳过：未找到 C 编译器")
        else:
            out_dir = os.path.join(ROOT, "build", "preview")
            os.makedirs(out_dir, exist_ok=True)
            exe = os.path.join(out_dir, "preview_svg")
            cmd = [cc, "-std=c99", "-O1", "-I", "core",
                   os.path.join("tools", "preview", "preview_svg.c"),
                   os.path.join("core", "picflow_layout.c"),
                   os.path.join("core", "picflow_palette.c"),
                   os.path.join("core", "picflow_catalog.c"),
                   "-o", exe, "-lm"]
            build = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
            if build.returncode != 0:
                print("  编译失败:\n" + build.stderr)
                ok = False
            else:
                for name, cols, width, count in (("iphone", 2, 375, 24), ("ipad", 3, 768, 24)):
                    svg = os.path.join(out_dir, name + ".svg")
                    r = subprocess.run([exe, svg, str(cols), str(width), str(count)],
                                       cwd=ROOT, capture_output=True, text=True)
                    if r.returncode != 0:
                        print("  生成失败: " + r.stderr)
                        ok = False
                    else:
                        for line in r.stdout.strip().splitlines():
                            print("  " + line)
    except Exception as exc:  # noqa: BLE001
        print("  异常: %s" % exc)
        ok = False
    results.append(("布局可视化", ok))
    if ok:
        print("\n  [通过] 5/5  瀑布流布局可视化")

    print("\n" + "=" * 64)
    print("  验证汇总")
    print("=" * 64)
    for name, passed in results:
        print("  %s  %s" % ("[通过]" if passed else "[失败]", name))
    failed = [name for name, passed in results if not passed]
    print("-" * 64)
    if failed:
        print("  失败 %d 项：%s" % (len(failed), "、".join(failed)))
        print("  注：iOS 工程的真实编译验证由 CI 的 ios-build 任务在 macOS 上完成")
        print("=" * 64)
        return 1
    print("  全部通过")
    print("  注：iOS 工程的真实编译验证由 CI 的 ios-build 任务在 macOS 上完成")
    print("=" * 64)
    return 0


if __name__ == "__main__":
    sys.exit(main())
