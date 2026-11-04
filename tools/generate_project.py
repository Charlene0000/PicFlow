#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generate_project.py
生成 PicFlow.xcodeproj/project.pbxproj

纯 Python 脚本，无第三方依赖。扫描源码目录，为每个 .h/.m 生成稳定 UUID，
拼装出完整可被 Xcode 打开的工程文件。

用法：python tools/generate_project.py
"""

import os
import hashlib
import sys
import collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT_NAME = "PicFlow"
SOURCE_DIR = os.path.join(ROOT, PROJECT_NAME)
CORE_DIR = os.path.join(ROOT, "core")
XCODEPROJ = os.path.join(ROOT, PROJECT_NAME + ".xcodeproj")

_used_uuids = set()


def uuid_for(key):
    """基于 key 生成稳定的 24 位大写十六进制 UUID，重复生成结果一致。"""
    h = hashlib.md5(key.encode("utf-8")).hexdigest().upper()
    candidate = h[:24]
    salt = 0
    while candidate in _used_uuids:
        salt += 1
        candidate = hashlib.md5((key + "#%d" % salt).encode("utf-8")).hexdigest().upper()[:24]
    _used_uuids.add(candidate)
    return candidate


def json_str(s):
    return '"%s"' % s.replace('\\', '\\\\').replace('"', '\\"')


def collect_sources():
    """
    收集需要加入工程的源文件。
    返回 (源文件列表, 头文件列表, 资源文件列表)，路径相对 ROOT。

    core/ 下的纯 C 源文件也要加入编译 —— ObjC 层通过 C 接口调用它们，
    这是「单一实现、iOS 与测试共用」的关键。
    """
    sources, headers, resources = [], [], []

    def scan(base_dir, prefix):
        for dirpath, dirnames, filenames in os.walk(base_dir):
            dirnames[:] = [d for d in dirnames if not d.startswith('.')]
            for fn in sorted(filenames):
                if fn.startswith('.'):
                    continue
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, ROOT).replace("\\", "/")
                if fn.endswith(".m") or fn.endswith(".c"):
                    sources.append(rel)
                elif fn.endswith(".h"):
                    headers.append(rel)
                elif fn.endswith((".plist", ".xcassets", ".storyboard", ".xib")):
                    resources.append(rel)

    scan(SOURCE_DIR, "app")
    if os.path.isdir(CORE_DIR):
        scan(CORE_DIR, "core")

    return sources, headers, resources


def build_directory_tree(paths):
    """
    由文件相对路径构造目录树。
    返回 dict: {dir_path: {"name":..., "files":[...], "dirs":[...]}}
    """
    tree = {}
    for rel in paths:
        parts = rel.split("/")
        dir_path = "/".join(parts[:-1])
        node = tree.setdefault(dir_path, {"name": parts[-2] if len(parts) > 1 else "",
                                         "files": [], "dirs": set()})
        node["files"].append((parts[-1], rel))
        # 逐级登记父目录
        for i in range(1, len(parts) - 1):
            parent = "/".join(parts[:i])
            child = "/".join(parts[:i + 1])
            pnode = tree.setdefault(parent, {"name": parts[i - 1], "files": [], "dirs": set()})
            pnode["dirs"].add(child)
            tree.setdefault(child, {"name": parts[i], "files": [], "dirs": set()})
    return tree


def main():
    if not os.path.isdir(SOURCE_DIR):
        print("找不到源码目录: %s" % SOURCE_DIR)
        return 1

    sources, headers, resources = collect_sources()
    if not sources:
        print("源码目录为空")
        return 1

    os.makedirs(XCODEPROJ, exist_ok=True)

    # ---- 核心对象 UUID ----
    id_project = uuid_for("object:PBXProject")
    id_target = uuid_for("object:PBXNativeTarget")
    id_product = uuid_for("object:PBXFileReference:app")
    id_root_group = uuid_for("object:PBXGroup:root")
    id_products_group = uuid_for("object:PBXGroup:products")
    id_sources_phase = uuid_for("object:PBXSourcesBuildPhase")
    id_resources_phase = uuid_for("object:PBXResourcesBuildPhase")
    id_frameworks_phase = uuid_for("object:PBXFrameworksBuildPhase")
    id_project_config_list = uuid_for("object:XCConfigurationList:project")
    id_target_config_list = uuid_for("object:XCConfigurationList:target")
    id_debug_project = uuid_for("object:XCBuildConfiguration:project:Debug")
    id_release_project = uuid_for("object:XCBuildConfiguration:project:Release")
    id_debug_target = uuid_for("object:XCBuildConfiguration:target:Debug")
    id_release_target = uuid_for("object:XCBuildConfiguration:target:Release")

    # ---- 文件引用 ----
    file_refs = {}    # rel -> uuid
    build_files = {}  # rel(m) -> uuid
    all_paths = sources + headers + resources

    for rel in all_paths:
        file_refs[rel] = uuid_for("file:" + rel)
    for rel in sources:
        build_files[rel] = uuid_for("buildfile:" + rel)

    # ---- 目录树 ----
    tree = build_directory_tree(all_paths)
    dir_uuids = {}
    for dir_path in tree:
        dir_uuids[dir_path] = uuid_for("group:" + dir_path)

    # ---- 渲染 group 节点 ----
    def render_groups(dir_path):
        node = tree[dir_path]
        lines = []
        gid = dir_uuids[dir_path]
        lines.append("\t\t%s /* %s */ = {" % (gid, node["name"]))
        lines.append("\t\t\tisa = PBXGroup;")
        lines.append("\t\t\tchildren = (")
        # 子目录在前，文件在后
        for child in sorted(node["dirs"]):
            child_name = tree[child]["name"]
            lines.append("\t\t\t\t%s /* %s */," % (dir_uuids[child], child_name))
        for fname, rel in sorted(node["files"]):
            lines.append("\t\t\t\t%s /* %s */," % (file_refs[rel], fname))
        lines.append("\t\t\t);")
        lines.append("\t\t\tname = %s;" % json_str(node["name"]))
        lines.append("\t\t\tsourceTree = \"<group>\";")
        lines.append("\t\t};")
        for child in sorted(node["dirs"]):
            lines.extend(render_groups(child))
        return lines

    # 只渲染顶层目录，render_groups 内部会递归渲染所有子目录，
    # 否则子目录 group 会被重复写入 pbxproj（UUID 重复，Xcode 打开报错）。
    top_dirs = [d for d in sorted(tree) if "/" not in d]
    group_lines = []
    for dir_path in top_dirs:
        group_lines.extend(render_groups(dir_path))

    # ---- 拼装 pbxproj ----
    out = []
    out.append("// !$*UTF8*$!")
    out.append("{")
    out.append("\tarchiveVersion = 1;")
    out.append("\tclasses = {")
    out.append("\t};")
    out.append("\tobjectVersion = 56;")
    out.append("\tobjects = {")

    out.append("")
    out.append("/* Begin PBXBuildFile section */")
    for rel in sources:
        out.append("\t\t%s /* %s in Sources */ = {isa = PBXBuildFile; fileRef = %s /* %s */; };" % (
            build_files[rel], os.path.basename(rel), file_refs[rel], os.path.basename(rel)))
    out.append("/* End PBXBuildFile section */")

    out.append("")
    out.append("/* Begin PBXFileReference section */")
    for rel in sources + headers:
        name = os.path.basename(rel)
        if name.endswith(".m"):
            ftype = "sourcecode.c.objc"
        elif name.endswith(".c"):
            ftype = "sourcecode.c.c"
        else:
            ftype = "sourcecode.c.h"
        out.append("\t\t%s /* %s */ = {isa = PBXFileReference; lastKnownFileType = %s; path = %s; sourceTree = \"<group>\"; };" % (
            file_refs[rel], name, ftype, json_str(name)))
    for rel in resources:
        name = os.path.basename(rel)
        out.append("\t\t%s /* %s */ = {isa = PBXFileReference; lastKnownFileType = text.plist.xml; path = %s; sourceTree = \"<group>\"; };" % (
            file_refs[rel], name, json_str(name)))
    out.append("\t\t%s /* %s.app */ = {isa = PBXFileReference; explicitFileType = wrapper.application; includeInIndex = 0; path = %s; sourceTree = BUILT_PRODUCTS_DIR; };" % (
        id_product, PROJECT_NAME + ".app", json_str(PROJECT_NAME + ".app")))
    out.append("/* End PBXFileReference section */")

    out.append("")
    out.append("/* Begin PBXFrameworksBuildPhase section */")
    out.append("\t\t%s /* Frameworks */ = {" % id_frameworks_phase)
    out.append("\t\t\tisa = PBXFrameworksBuildPhase;")
    out.append("\t\t\tbuildActionMask = 2147483647;")
    out.append("\t\t\tfiles = (")
    out.append("\t\t\t);")
    out.append("\t\t\trunOnlyForDeploymentPostprocessing = 0;")
    out.append("\t\t};")
    out.append("/* End PBXFrameworksBuildPhase section */")

    out.append("")
    out.append("/* Begin PBXGroup section */")
    out.extend(group_lines)
    out.append("\t\t%s = {" % id_root_group)
    out.append("\t\t\tisa = PBXGroup;")
    out.append("\t\t\tchildren = (")
    out.append("\t\t\t\t%s /* %s */," % (dir_uuids["PicFlow"], PROJECT_NAME))
    if "core" in dir_uuids:
        out.append("\t\t\t\t%s /* core */," % dir_uuids["core"])
    out.append("\t\t\t\t%s /* Products */," % id_products_group)
    out.append("\t\t\t);")
    out.append("\t\t\tsourceTree = \"<group>\";")
    out.append("\t\t};")
    out.append("\t\t%s /* Products */ = {" % id_products_group)
    out.append("\t\t\tisa = PBXGroup;")
    out.append("\t\t\tchildren = (")
    out.append("\t\t\t\t%s /* %s.app */," % (id_product, PROJECT_NAME + ".app"))
    out.append("\t\t\t);")
    out.append("\t\t\tname = Products;")
    out.append("\t\t\tsourceTree = \"<group>\";")
    out.append("\t\t};")
    out.append("/* End PBXGroup section */")

    out.append("")
    out.append("/* Begin PBXNativeTarget section */")
    out.append("\t\t%s /* %s */ = {" % (id_target, PROJECT_NAME))
    out.append("\t\t\tisa = PBXNativeTarget;")
    out.append("\t\t\tbuildConfigurationList = %s /* Build configuration list for PBXNativeTarget \"%s\" */;" % (id_target_config_list, PROJECT_NAME))
    out.append("\t\t\tbuildPhases = (")
    out.append("\t\t\t\t%s /* Sources */," % id_sources_phase)
    out.append("\t\t\t\t%s /* Frameworks */," % id_frameworks_phase)
    out.append("\t\t\t\t%s /* Resources */," % id_resources_phase)
    out.append("\t\t\t);")
    out.append("\t\t\tbuildRules = (")
    out.append("\t\t\t);")
    out.append("\t\t\tdependencies = (")
    out.append("\t\t\t);")
    out.append("\t\t\tname = %s;" % PROJECT_NAME)
    out.append("\t\t\tproductName = %s;" % PROJECT_NAME)
    out.append("\t\t\tproductReference = %s /* %s.app */;" % (id_product, PROJECT_NAME + ".app"))
    out.append("\t\t\tproductType = \"com.apple.product-type.application\";")
    out.append("\t\t};")
    out.append("/* End PBXNativeTarget section */")

    out.append("")
    out.append("/* Begin PBXProject section */")
    out.append("\t\t%s /* Project object */ = {" % id_project)
    out.append("\t\t\tisa = PBXProject;")
    out.append("\t\t\tattributes = {")
    out.append("\t\t\t\tBuildIndependentTargetsInParallel = 1;")
    out.append("\t\t\t\tLastUpgradeCheck = 1500;")
    out.append("\t\t\t\tTargetAttributes = {")
    out.append("\t\t\t\t\t%s = {" % id_target)
    out.append("\t\t\t\t\t\tCreatedOnToolsVersion = 15.0;")
    out.append("\t\t\t\t\t};")
    out.append("\t\t\t\t};")
    out.append("\t\t\t};")
    out.append("\t\t\tbuildConfigurationList = %s /* Build configuration list for PBXProject \"%s\" */;" % (id_project_config_list, PROJECT_NAME))
    out.append("\t\t\tcompatibilityVersion = \"Xcode 14.0\";")
    out.append("\t\t\tdevelopmentRegion = en;")
    out.append("\t\t\thasScannedForEncodings = 0;")
    out.append("\t\t\tknownRegions = (")
    out.append("\t\t\t\ten,")
    out.append("\t\t\t\tBase,")
    out.append("\t\t\t);")
    out.append("\t\t\tmainGroup = %s;" % id_root_group)
    out.append("\t\t\tproductRefGroup = %s /* Products */;" % id_products_group)
    out.append("\t\t\tprojectDirPath = \"\";")
    out.append("\t\t\tprojectRoot = \"\";")
    out.append("\t\t\ttargets = (")
    out.append("\t\t\t\t%s /* %s */," % (id_target, PROJECT_NAME))
    out.append("\t\t\t);")
    out.append("\t\t};")
    out.append("/* End PBXProject section */")

    out.append("")
    out.append("/* Begin PBXResourcesBuildPhase section */")
    out.append("\t\t%s /* Resources */ = {" % id_resources_phase)
    out.append("\t\t\tisa = PBXResourcesBuildPhase;")
    out.append("\t\t\tbuildActionMask = 2147483647;")
    out.append("\t\t\tfiles = (")
    out.append("\t\t\t);")
    out.append("\t\t\trunOnlyForDeploymentPostprocessing = 0;")
    out.append("\t\t};")
    out.append("/* End PBXResourcesBuildPhase section */")

    out.append("")
    out.append("/* Begin PBXSourcesBuildPhase section */")
    out.append("\t\t%s /* Sources */ = {" % id_sources_phase)
    out.append("\t\t\tisa = PBXSourcesBuildPhase;")
    out.append("\t\t\tbuildActionMask = 2147483647;")
    out.append("\t\t\tfiles = (")
    for rel in sources:
        out.append("\t\t\t\t%s /* %s in Sources */," % (build_files[rel], os.path.basename(rel)))
    out.append("\t\t\t);")
    out.append("\t\t\trunOnlyForDeploymentPostprocessing = 0;")
    out.append("\t\t};")
    out.append("/* End PBXSourcesBuildPhase section */")

    common_project_settings = [
        "ALWAYS_SEARCH_USER_PATHS = NO",
        "CLANG_ANALYZER_NONNULL = YES",
        "CLANG_ENABLE_MODULES = YES",
        "CLANG_ENABLE_OBJC_ARC = YES",
        "CLANG_WARN_BOOL_CONVERSION = YES",
        "CLANG_WARN_DIRECT_OBJC_ISA_USAGE = YES_ERROR",
        "CLANG_WARN_DOCUMENTATION_COMMENTS = YES",
        "CLANG_WARN_EMPTY_BODY = YES",
        "CLANG_WARN_ENUM_CONVERSION = YES",
        "CLANG_WARN_INFINITE_RECURSION = YES",
        "CLANG_WARN_INT_CONVERSION = YES",
        "CLANG_WARN_OBJC_LITERAL_CONVERSION = YES",
        "CLANG_WARN_OBJC_ROOT_CLASS = YES_ERROR",
        "CLANG_WARN_UNREACHABLE_CODE = YES",
        "CLANG_WARN__DUPLICATE_METHOD_MATCH = YES",
        "COPY_PHASE_STRIP = NO",
        "ENABLE_STRICT_OBJC_MSGSEND = YES",
        "GCC_C_LANGUAGE_STANDARD = gnu11",
        "GCC_NO_COMMON_BLOCKS = YES",
        "GCC_WARN_64_TO_32_BIT_CONVERSION = YES",
        "GCC_WARN_ABOUT_RETURN_TYPE = YES_ERROR",
        "GCC_WARN_UNDECLARED_SELECTOR = YES",
        "GCC_WARN_UNINITIALIZED_AUTOS = YES_AGGRESSIVE",
        "GCC_WARN_UNUSED_FUNCTION = YES",
        "GCC_WARN_UNUSED_VARIABLE = YES",
        "IPHONEOS_DEPLOYMENT_TARGET = 13.0",
        "MTL_ENABLE_DEBUG_INFO = INCLUDE_SOURCE",
        "SDKROOT = iphoneos",
        "TARGETED_DEVICE_FAMILY = \"1,2\"",
    ]
    debug_project = common_project_settings + [
        "DEBUG_INFORMATION_FORMAT = dwarf",
        "ENABLE_TESTABILITY = YES",
        "GCC_OPTIMIZATION_LEVEL = 0",
        "GCC_PREPROCESSOR_DEFINITIONS = (\n\t\t\t\t\t\"DEBUG=1\",\n\t\t\t\t\t\"$(inherited)\",\n\t\t\t\t)",
        "MTL_FAST_MATH = YES",
        "ONLY_ACTIVE_ARCH = YES",
    ]
    release_project = common_project_settings + [
        "DEBUG_INFORMATION_FORMAT = \"dwarf-with-dsym\"",
        "ENABLE_NS_ASSERTIONS = NO",
        "MTL_ENABLE_DEBUG_INFO = NO",
        "VALIDATE_PRODUCT = YES",
    ]
    target_settings = [
        "CODE_SIGN_STYLE = Automatic",
        "CURRENT_PROJECT_VERSION = 1",
        "GENERATE_INFOPLIST_FILE = NO",
        "INFOPLIST_FILE = %s/Resources/Info.plist" % PROJECT_NAME,
        "LD_RUNPATH_SEARCH_PATHS = (\n\t\t\t\t\t\"$(inherited)\",\n\t\t\t\t\t\"@executable_path/Frameworks\",\n\t\t\t\t)",
        "MARKETING_VERSION = 1.0.0",
        "PRODUCT_BUNDLE_IDENTIFIER = com.picflow.PicFlow",
        "PRODUCT_NAME = \"$(TARGET_NAME)\"",
        "TARGETED_DEVICE_FAMILY = \"1,2\"",
    ]

    def render_config(cfg_uuid, name, settings):
        lines = ["\t\t%s /* %s */ = {" % (cfg_uuid, name),
                 "\t\t\tisa = XCBuildConfiguration;",
                 "\t\t\tbuildSettings = {"]
        for s in settings:
            lines.append("\t\t\t\t%s;" % s)
        lines.append("\t\t\t};")
        lines.append("\t\t\tname = %s;" % name)
        lines.append("\t\t};")
        return lines

    out.append("")
    out.append("/* Begin XCBuildConfiguration section */")
    out.extend(render_config(id_debug_project, "Debug", debug_project))
    out.extend(render_config(id_release_project, "Release", release_project))
    out.extend(render_config(id_debug_target, "Debug", target_settings))
    out.extend(render_config(id_release_target, "Release", target_settings))
    out.append("/* End XCBuildConfiguration section */")

    out.append("")
    out.append("/* Begin XCConfigurationList section */")
    for list_uuid, label, debug_uuid, release_uuid in [
        (id_project_config_list, "PBXProject \\\"%s\\\"" % PROJECT_NAME, id_debug_project, id_release_project),
        (id_target_config_list, "PBXNativeTarget \\\"%s\\\"" % PROJECT_NAME, id_debug_target, id_release_target),
    ]:
        out.append("\t\t%s /* Build configuration list for %s */ = {" % (list_uuid, label))
        out.append("\t\t\tisa = XCConfigurationList;")
        out.append("\t\t\tbuildConfigurations = (")
        out.append("\t\t\t\t%s /* Debug */," % debug_uuid)
        out.append("\t\t\t\t%s /* Release */," % release_uuid)
        out.append("\t\t\t);")
        out.append("\t\t\tdefaultConfigurationIsVisible = 0;")
        out.append("\t\t\tdefaultConfigurationName = Release;")
        out.append("\t\t};")
    out.append("/* End XCConfigurationList section */")

    out.append("\t};")
    out.append("\trootObject = %s /* Project object */;" % id_project)
    out.append("}")

    pbx_path = os.path.join(XCODEPROJ, "project.pbxproj")
    with open(pbx_path, "w", encoding="utf-8") as f:
        f.write("\n".join(out))

    # ---- scheme ----
    schemes_dir = os.path.join(XCODEPROJ, "xcshareddata", "xcschemes")
    os.makedirs(schemes_dir, exist_ok=True)
    scheme = """<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion = "1500" version = "1.7">
   <BuildAction parallelizeBuildables = "YES" buildImplicitDependencies = "YES">
      <BuildActionEntries>
         <BuildActionEntry buildForTesting = "YES" buildForRunning = "YES" buildForProfiling = "YES" buildForArchiving = "YES" buildForAnalyzing = "YES">
            <BuildableReference
               BuildableIdentifier = "primary"
               BlueprintIdentifier = "%(target)s"
               BuildableName = "%(name)s.app"
               BlueprintName = "%(name)s"
               ReferencedContainer = "container:%(name)s.xcodeproj">
            </BuildableReference>
         </BuildActionEntry>
      </BuildActionEntries>
   </BuildAction>
   <TestAction buildConfiguration = "Debug" selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv = "YES">
      <Testables>
      </Testables>
   </TestAction>
   <LaunchAction buildConfiguration = "Debug" selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB" launchStyle = "0" useCustomWorkingDirectory = "NO" ignoresPersistentStateOnLaunch = "NO" debugDocumentVersioning = "YES" debugServiceExtension = "internal" allowLocationSimulation = "YES">
      <BuildableProductRunnable runnableDebuggingMode = "0">
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "%(target)s"
            BuildableName = "%(name)s.app"
            BlueprintName = "%(name)s"
            ReferencedContainer = "container:%(name)s.xcodeproj">
         </BuildableReference>
      </BuildableProductRunnable>
   </LaunchAction>
   <ProfileAction buildConfiguration = "Release" shouldUseLaunchSchemeArgsEnv = "YES" savedToolIdentifier = "" useCustomWorkingDirectory = "NO" debugDocumentVersioning = "YES">
      <BuildableProductRunnable runnableDebuggingMode = "0">
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "%(target)s"
            BuildableName = "%(name)s.app"
            BlueprintName = "%(name)s"
            ReferencedContainer = "container:%(name)s.xcodeproj">
         </BuildableReference>
      </BuildableProductRunnable>
   </ProfileAction>
   <AnalyzeAction buildConfiguration = "Debug">
   </AnalyzeAction>
   <ArchiveAction buildConfiguration = "Release" revealArchiveInOrganizer = "YES">
   </ArchiveAction>
</Scheme>
""" % {"target": id_target, "name": PROJECT_NAME}
    with open(os.path.join(schemes_dir, PROJECT_NAME + ".xcscheme"), "w", encoding="utf-8") as f:
        f.write(scheme)

    workspace_dir = os.path.join(XCODEPROJ, "project.xcworkspace")
    os.makedirs(workspace_dir, exist_ok=True)
    with open(os.path.join(workspace_dir, "contents.xcworkspacedata"), "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n'
                '<Workspace version = "1.0">\n'
                '   <FileRef location = "self:">\n'
                '   </FileRef>\n'
                '</Workspace>\n')

    # ---- 自检：UUID 唯一性 ----
    check = open(pbx_path, encoding="utf-8").read()
    ids = collections.Counter()
    for line in check.splitlines():
        line = line.strip()
        if line.startswith(tuple("0123456789ABCDEF")) and " /* " in line and " = {" in line:
            ids[line.split(" ")[0]] += 1
    dup = [k for k, v in ids.items() if v > 1]
    if dup:
        print("✗ 发现重复 UUID: %s" % dup)
        return 2

    print("✓ 工程生成完成，UUID 唯一性自检通过")
    print("  工程路径: %s" % XCODEPROJ)
    print("  源文件 %d 个，头文件 %d 个，资源 %d 个" % (len(sources), len(headers), len(resources)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
