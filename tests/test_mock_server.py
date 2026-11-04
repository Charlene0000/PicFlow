#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_mock_server.py —— 自研图库服务的集成测试

验证真实 HTTP 链路：
  1. /catalog.json 返回结构合法的 JSON，且字段齐全
  2. /img/<id>.png 返回合法的 PNG（签名 + chunk + CRC32 全部校验）
  3. 分页参数生效（page/limit 正确切片）
  4. 非法参数与越界 ID 被正确拒绝
  5. HTTP 缓存头存在（ETag / Cache-Control）

这些测试证明「App 通过 NSURLSession 拿到的数据是可靠的」，
不需要真机也不需要外部网络。

运行：python tests/test_mock_server.py
"""

import json
import os
import struct
import sys
import threading
import urllib.request
import zlib
from http.server import ThreadingHTTPServer

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))

from mock_server import Handler, CATALOG_SIZE  # noqa: E402

PASSED = 0
FAILED = 0


def check(condition, message):
    global PASSED, FAILED
    if condition:
        PASSED += 1
        print("    [ OK ] %s" % message)
    else:
        FAILED += 1
        print("    [FAIL] %s" % message)


def fetch(url):
    """绕过环境代理直接请求本地服务。"""
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    with opener.open(url, timeout=10) as response:
        return response.status, dict(response.headers), response.read()


def validate_png(data):
    """校验 PNG 结构：签名、IHDR、各 chunk 的 CRC32。"""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return False, "PNG 签名错误"
    width, height = struct.unpack(">II", data[16:24])
    if width <= 0 or height <= 0:
        return False, "尺寸非法"
    offset = 8
    seen_ihdr = False
    seen_iend = False
    while offset < len(data):
        if offset + 8 > len(data):
            return False, "chunk 头越界"
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        chunk_type = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        if offset + 12 + length > len(data):
            return False, "chunk 数据越界"
        crc = struct.unpack(">I", data[offset + 8 + length:offset + 12 + length])[0]
        if (zlib.crc32(chunk_type + payload) & 0xFFFFFFFF) != crc:
            return False, "chunk %s CRC 校验失败" % chunk_type.decode("ascii", "replace")
        if chunk_type == b"IHDR":
            seen_ihdr = True
        if chunk_type == b"IEND":
            seen_iend = True
        offset += 12 + length
    if not seen_ihdr:
        return False, "缺少 IHDR"
    if not seen_iend:
        return False, "缺少 IEND"
    return True, "%dx%d" % (width, height)


def main():
    print("=" * 60)
    print("  PicFlow 自研图库服务 集成测试")
    print("=" * 60)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    port = server.server_address[1]
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base = "http://127.0.0.1:%d" % port
    print("  测试服务端口: %d\n" % port)

    try:
        # 1. 图库 JSON
        print("[ 1 ] 图库 JSON 接口")
        status, headers, body = fetch(base + "/catalog.json?page=1&limit=5")
        check(status == 200, "HTTP 状态码 200")
        check("application/json" in headers.get("Content-Type", ""), "Content-Type 为 JSON")
        catalog = json.loads(body.decode("utf-8"))
        check(isinstance(catalog, list), "返回数组")
        check(len(catalog) == 5, "limit=5 生效，实际 %d 条" % len(catalog))
        first = catalog[0]
        for field in ("id", "seed", "title", "author", "width", "height", "url"):
            check(field in first, "字段 %s 存在" % field)
        check(first["width"] > 0 and first["height"] > 0, "宽高为正数")
        check(first["url"].startswith("http"), "图片 URL 可用")

        # 2. 分页切片
        print("\n[ 2 ] 分页切片")
        _, _, body2 = fetch(base + "/catalog.json?page=2&limit=5")
        page2 = json.loads(body2.decode("utf-8"))
        check(len(page2) == 5, "第 2 页也是 5 条")
        check(page2[0]["id"] != first["id"], "第 2 页与第 1 页内容不同")
        _, _, body3 = fetch(base + "/catalog.json?page=1&limit=999")
        check(len(json.loads(body3.decode("utf-8"))) <= 64, "limit 上限被钳制")

        # 3. PNG 图片
        print("\n[ 3 ] PNG 图片接口")
        status, headers, png = fetch(base + "/img/0.png")
        check(status == 200, "HTTP 状态码 200")
        check(headers.get("Content-Type") == "image/png", "Content-Type 为 image/png")
        check("ETag" in headers, "带 ETag 头")
        check("Cache-Control" in headers, "带 Cache-Control 头")
        ok, detail = validate_png(png)
        check(ok, "PNG 结构与 CRC 校验通过 (%s, %d 字节)" % (detail, len(png)))

        # 4. 不同 ID 生成不同尺寸
        print("\n[ 4 ] 不同图库条目生成不同尺寸")
        _, _, png1 = fetch(base + "/img/1.png")
        ok1, detail1 = validate_png(png1)
        check(ok1, "id=1 的 PNG 合法 (%s)" % detail1)
        check(detail != detail1, "不同条目尺寸不同：%s vs %s" % (detail, detail1))
        check(png != png1, "不同条目内容不同")

        # 5. 错误处理
        print("\n[ 5 ] 错误处理")
        try:
            fetch(base + "/img/9999.png")
            check(False, "越界 ID 应返回错误")
        except urllib.error.HTTPError as e:
            check(e.code == 404, "越界 ID 返回 404")
        try:
            fetch(base + "/img/abc.png")
            check(False, "非法 ID 应返回错误")
        except urllib.error.HTTPError as e:
            check(e.code == 400, "非法 ID 返回 400")
        try:
            fetch(base + "/nonexistent")
            check(False, "未知路径应返回错误")
        except urllib.error.HTTPError as e:
            check(e.code == 404, "未知路径返回 404")

        # 6. 确定性：同一 ID 两次请求内容一致
        print("\n[ 6 ] 确定性")
        _, _, png_a = fetch(base + "/img/3.png")
        _, _, png_b = fetch(base + "/img/3.png")
        check(png_a == png_b, "同一 ID 两次请求内容完全一致（可缓存）")

    finally:
        server.shutdown()
        server.server_close()

    print("\n" + "-" * 60)
    print("  通过 %d 项，失败 %d 项" % (PASSED, FAILED))
    print("=" * 60)
    return 0 if FAILED == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
