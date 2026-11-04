#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
run_tests.py —— 跨平台测试运行器

在 Windows / Linux / macOS 上用同一套命令跑 C 核心测试：
    python tools/run_tests.py

不需要任何第三方依赖，也不需要 Xcode。
Windows 上使用 MinGW gcc，Linux/macOS 上使用系统 cc/gcc。
"""

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORE = os.path.join(ROOT, "core")
TESTS = os.path.join(ROOT, "tests")
BUILD = os.path.join(ROOT, "build", "tests")

CORE_SOURCES = [
    os.path.join(CORE, "picflow_layout.c"),
    os.path.join(CORE, "picflow_paging.c"),
    os.path.join(CORE, "picflow_cache.c"),
    os.path.join(CORE, "picflow_palette.c"),
    os.path.join(CORE, "picflow_catalog.c"),
]

TEST_SUITES = [
    ("test_layout", os.path.join(TESTS, "test_layout.c")),
    ("test_paging", os.path.join(TESTS, "test_paging.c")),
    ("test_cache",  os.path.join(TESTS, "test_cache.c")),
    ("test_palette", os.path.join(TESTS, "test_palette.c")),
]


def find_compiler():
    """按优先级探测 C 编译器。"""
    candidates = []
    if os.name == "nt":
        candidates = ["gcc", "clang", "cc"]
    else:
        candidates = ["cc", "gcc", "clang"]
    for name in candidates:
        try:
            result = subprocess.run([name, "--version"],
                                    capture_output=True, text=True, timeout=20)
            if result.returncode == 0:
                return name, result.stdout.splitlines()[0]
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
    return None, None


def main():
    compiler, version = find_compiler()
    if compiler is None:
        print("✗ 未找到 C 编译器。")
        print("  Windows: 安装 MinGW-w64（choco install mingw）")
        print("  macOS  : xcode-select --install")
        print("  Linux  : sudo apt install build-essential")
        return 2

    print("=" * 60)
    print("  PicFlow Core 测试套件")
    print("  编译器: %s (%s)" % (compiler, version))
    print("=" * 60)

    os.makedirs(BUILD, exist_ok=True)

    failed_suites = []
    for name, source in TEST_SUITES:
        exe_path = os.path.join(BUILD, name + (".exe" if os.name == "nt" else ""))
        cmd = [
            compiler, "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter",
            "-O1", "-g",
            "-I", CORE, "-I", TESTS,
            source,
        ] + CORE_SOURCES + ["-o", exe_path, "-lm"]

        print("\n[编译] %s" % name)
        build = subprocess.run(cmd, capture_output=True, text=True)
        if build.returncode != 0:
            print(build.stdout)
            print(build.stderr)
            failed_suites.append((name, "编译失败"))
            continue

        # 编译告警提示（不阻断，但值得暴露）
        if build.stderr.strip():
            print("  编译告警:")
            for line in build.stderr.strip().splitlines()[:20]:
                print("    " + line)

        run = subprocess.run([exe_path], capture_output=True, text=True)
        sys.stdout.write(run.stdout)
        if run.stderr.strip():
            sys.stderr.write(run.stderr)
        if run.returncode != 0:
            failed_suites.append((name, "测试未通过"))

    print("\n" + "=" * 60)
    if failed_suites:
        print("  结果: 失败 %d 个套件" % len(failed_suites))
        for name, reason in failed_suites:
            print("    x %s -> %s" % (name, reason))
        print("=" * 60)
        return 1
    print("  结果: 全部通过")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
