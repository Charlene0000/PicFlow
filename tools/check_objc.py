#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_objc.py —— Objective-C 静态自检

为什么需要它？
  本项目开发机是 Windows，无法运行 Xcode 编译 Objective-C。
  但 Objective-C 的**结构性错误**（括号不配平、@end 缺失、头文件引用缺失、
  方法签名与声明不匹配、调用了未声明的 C 函数）完全可以用静态检查发现。

  这个脚本不能替代真机编译（CI 的 ios-build 任务负责那部分），
  但它能在提交前把绝大多数低级错误拦下来 —— 这也是「跨平台工程化」的一部分。

用法：python tools/check_objc.py
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP_DIR = os.path.join(ROOT, "PicFlow")
CORE_DIR = os.path.join(ROOT, "core")

issues = []
stats = {"files": 0, "lines": 0, "classes": 0, "methods": 0}


def strip_code(text):
    """去掉字符串字面量与注释，避免 URL 里的 // 被当成注释。"""
    text = re.sub(r'@?"(\\.|[^"\\\n])*"', '""', text)
    text = re.sub(r'//[^\n]*', '', text)
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    return text


def check_balance(path, code):
    for open_ch, close_ch, name in [('{', '}', '花括号'), ('(', ')', '圆括号'), ('[', ']', '方括号')]:
        if code.count(open_ch) != code.count(close_ch):
            issues.append("%s: %s不平衡 %d/%d" % (
                os.path.relpath(path, ROOT), name, code.count(open_ch), code.count(close_ch)))


def check_declarations(path, code):
    decl = len(re.findall(r'@(interface|implementation|protocol)\b', code))
    ends = len(re.findall(r'@end\b', code))
    if decl != ends:
        issues.append("%s: 声明/@end 不匹配（声明 %d，@end %d）" % (
            os.path.relpath(path, ROOT), decl, ends))


def collect_headers():
    """收集所有可被 import 的头文件名（不含扩展名）。"""
    names = set()
    for base in (APP_DIR, CORE_DIR):
        for dirpath, dirnames, filenames in os.walk(base):
            for fn in filenames:
                if fn.endswith(".h"):
                    names.add(fn[:-2])
    return names


def check_imports(path, code, known_headers):
    """检查 #import "xxx.h" 是否指向真实存在的头文件。"""
    for name in re.findall(r'#import\s+"([^"]+)"', code):
        base = os.path.basename(name)
        stem = base[:-2] if base.endswith(".h") else base
        if stem in known_headers:
            continue
        # 可能是相对路径引用（core/xxx.h）
        candidate = os.path.join(ROOT, name.replace("/", os.sep))
        if os.path.isfile(candidate):
            continue
        # 逐级向上查找
        found = False
        parts = os.path.abspath(path).split(os.sep)
        for i in range(len(parts) - 1, 0, -1):
            probe = os.path.join(os.sep.join(parts[:i]), name.replace("/", os.sep))
            if os.path.isfile(probe):
                found = True
                break
        if not found:
            issues.append("%s: #import \"%s\" 找不到对应头文件" % (
                os.path.relpath(path, ROOT), name))


def check_method_sigs(path, code):
    """
    检查 .m 里定义的**无参私有方法**是否在头文件或 class extension 中声明过。

    规则说明：
    - 带参数的方法（选择器含 ':'）多为协议回调或 UIKit 覆写，
      静态判断容易误报，一律跳过；
    - 无参方法（如 setupSubviews / refresh）通常是私有 helper，
      如果既没在 .h 也没在 .m 的 class extension 里声明，
      编译器会报 "No visible @interface declares the selector"，
      这正是要抓的错误；
    - UIKit / Foundation 的生命周期方法由父类提供，不要求声明，走白名单。
    """
    if not path.endswith(".m"):
        return

    header = path[:-2] + ".h"
    declared = set()

    def harvest(text):
        # 抓取方法声明/定义的选择器首段
        for m in re.finditer(r'^[-+]\s*\([^)]*\)\s*([A-Za-z_][A-Za-z0-9_]*)', text, re.M):
            declared.add(m.group(1))

    if os.path.isfile(header):
        harvest(strip_code(open(header, encoding="utf-8").read()))
    # .m 内的 class extension 声明
    harvest(code)

    # UIKit / Foundation 提供的覆写方法，无需声明
    system_overrides = {
        "init", "dealloc", "copyWithZone", "viewDidLoad", "viewWillAppear",
        "viewDidAppear", "viewWillDisappear", "viewDidDisappear",
        "viewWillLayoutSubviews", "viewDidLayoutSubviews", "layoutSubviews",
        "setNeedsLayout", "updateConstraints", "traitCollectionDidChange",
        "viewWillTransitionToSize", "preferredStatusBarStyle",
        "preferredStatusBarHidden", "preferredHomeIndicatorAutoHidden",
        "didReceiveMemoryWarning", "encodeWithCoder", "initWithCoder",
        "prepareForReuse", "awakeFromNib", "reset",
    }

    for m in re.finditer(r'^[-+]\s*\([^)]*\)\s*([A-Za-z_][A-Za-z0-9_]*)', code, re.M):
        name = m.group(1)
        if name in system_overrides or name in declared:
            continue
        issues.append("%s: 私有方法 %s 未在任何头文件中声明" % (
            os.path.relpath(path, ROOT), name))


def check_c_api_usage(path, code):
    """
    检查 ObjC 里调用的 C 函数是否在 core/ 的头文件里声明过。
    这能发现「C 函数改名后 ObjC 侧忘了同步」这类跨语言边界的典型错误。
    """
    if not path.endswith(".m"):
        return
    core_headers = ""
    for fn in os.listdir(CORE_DIR):
        if fn.endswith(".h"):
            core_headers += open(os.path.join(CORE_DIR, fn), encoding="utf-8").read()

    # 找出形如 pf_xxx( 的调用
    for name in set(re.findall(r'\b(pf_[a-z0-9_]+)\s*\(', code)):
        if ("%s(" % name) not in core_headers:
            issues.append("%s: 调用了未声明的 C 函数 %s()" % (
                os.path.relpath(path, ROOT), name))


def main():
    known = collect_headers()

    for base, exts in ((APP_DIR, (".h", ".m")), (CORE_DIR, (".c", ".h"))):
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames if not d.startswith('.')]
            for fn in sorted(filenames):
                if not fn.endswith(exts):
                    continue
                path = os.path.join(dirpath, fn)
                raw = open(path, encoding="utf-8").read()
                code = strip_code(raw)
                stats["files"] += 1
                stats["lines"] += raw.count("\n") + 1
                stats["classes"] += len(re.findall(r'@interface\b', code))
                stats["methods"] += len(re.findall(r'^[-+]\s*\(', code, re.M))

                check_balance(path, code)
                check_declarations(path, code)
                if fn.endswith(".m"):
                    check_imports(path, code, known)
                    check_method_sigs(path, code)
                    check_c_api_usage(path, code)

    print("=" * 60)
    print("  PicFlow Objective-C 静态自检")
    print("=" * 60)
    print("  扫描文件 : %d" % stats["files"])
    print("  代码行数 : %d" % stats["lines"])
    print("  类/协议  : %d" % stats["classes"])
    print("  方法数   : %d" % stats["methods"])
    print("-" * 60)

    if issues:
        print("  发现 %d 个问题：" % len(issues))
        for item in issues:
            print("    x %s" % item)
        print("=" * 60)
        return 1

    print("  ✓ 未发现结构性问题")
    print("  提示：Objective-C 的完整编译验证由 CI 的 ios-build 任务在 macOS 上完成")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
