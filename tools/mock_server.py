#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mock_server.py —— 自研图库 Mock 服务（零第三方依赖）

用途：
  项目默认使用「本地程序化生成」，完全不需要网络。
  但 JD 要求体现 NSURLSession 的使用，所以这里提供一个**自建**的图库服务，
  让你能在真机上跑通完整的 HTTP 链路：

      内存缓存未命中 -> NSURLSession 下载 -> 后台解码 -> 落盘 -> 回填内存缓存

  与「调用别人的付费 API」不同：
  - 服务端代码完全自研，可读可改；
  - 图片由服务端用同一套 HSL 配色规则程序化生成，不是第三方素材；
  - 无 API Key、无配额、无外网依赖，CI 与离线环境都能跑。

启动：
    python tools/mock_server.py            # 默认 127.0.0.1:8765
    python tools/mock_server.py --port 9000 --host 0.0.0.0

接口：
    GET /catalog.json?page=1&limit=24   返回图库 JSON 列表
    GET /img/<id>.png                   返回程序化生成的 PNG 图片

真机访问：
    模拟器用 127.0.0.1 即可；
    真机需要用电脑的局域网 IP（例如 http://192.168.1.5:8765），
    此时把 App 里的 kPFGalleryRemoteURL 改成对应地址即可。
"""

import argparse
import json
import os
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from png_writer import render_artwork_png  # noqa: E402

# ---- 图库配置（与 core/picflow_catalog.c 的规模保持一致）----
CATALOG_SIZE = 64
DEFAULT_LIMIT = 24
MAX_LIMIT = 64

# 图片宽高比序列：与 C 端同样的「低差异分布」思路，覆盖 0.62 ~ 1.78
ASPECT_SEQUENCES = [
    0.62, 1.78, 0.94, 1.42, 0.71, 1.15, 1.66, 0.83,
    1.28, 0.66, 1.53, 0.89, 1.72, 0.76, 1.09, 0.63,
]

ADJECTIVES = ["晨雾", "深蓝", "暖阳", "静谧", "斑驳", "薄暮", "清冽", "流光",
              "孤岛", "旷野", "琉璃", "潮汐", "山岚", "幽篁", "疏星", "暮色",
              "微光", "远山", "潮生", "拾光"]
NOUNS = ["海岸", "巷陌", "天际", "原野", "松林", "湖面", "屋顶", "长街",
         "荒原", "峡谷", "渡口", "山脊", "河畔", "灯塔", "沙丘", "云海",
         "石桥", "旷野", "旧城", "竹径"]
AUTHOR_PREFIX = ["青", "白", "墨", "素", "丹", "朱", "玄"]
AUTHOR_SUFFIX = ["川", "山", "舟", "砚", "墨", "弦", "崖", "屿"]


def build_catalog():
    """构建图库条目列表。"""
    entries = []
    for i in range(CATALOG_SIZE):
        seed = (i + 1) * 2654435761 % (2 ** 32)
        ratio = ASPECT_SEQUENCES[i % len(ASPECT_SEQUENCES)]
        adj = ADJECTIVES[(seed // 11) % len(ADJECTIVES)]
        noun = NOUNS[(seed // 13) % len(NOUNS)]
        pa = AUTHOR_PREFIX[(seed // 17) % len(AUTHOR_PREFIX)]
        ps = AUTHOR_SUFFIX[(seed // 19) % len(AUTHOR_SUFFIX)]
        entries.append({
            "id": i,
            "seed": seed,
            "title": "%s%s" % (adj, noun),
            "author": "%s%s" % (pa, ps),
            "width": 640,
            "height": int(640 * ratio),
            "aspectRatio": round(ratio, 4),
        })
    return entries


CATALOG = build_catalog()


class Handler(BaseHTTPRequestHandler):
    server_version = "PicFlowMock/1.0"

    def _send(self, status, content_type, body, extra_headers=None):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        if extra_headers:
            for key, value in extra_headers.items():
                self.send_header(key, value)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        query = parse_qs(parsed.query)

        if path == "/catalog.json":
            self._serve_catalog(query)
        elif path.startswith("/img/"):
            self._serve_image(path)
        elif path == "/":
            self._send(200, "text/html; charset=utf-8",
                       ("<h2>PicFlow Mock Server</h2>"
                        "<p>GET /catalog.json?page=1&amp;limit=24</p>"
                        "<p>GET /img/0.png</p>").encode("utf-8"))
        else:
            self._send(404, "application/json; charset=utf-8",
                       json.dumps({"error": "not found"}).encode("utf-8"))

    def _serve_catalog(self, query):
        try:
            page = max(1, int(query.get("page", ["1"])[0]))
            limit = int(query.get("limit", [str(DEFAULT_LIMIT)])[0])
        except ValueError:
            self._send(400, "application/json; charset=utf-8",
                       json.dumps({"error": "invalid params"}).encode("utf-8"))
            return

        limit = max(1, min(limit, MAX_LIMIT))
        start = (page - 1) * limit
        end = min(start + limit, CATALOG_SIZE)

        items = []
        for entry in CATALOG[start:end]:
            item = dict(entry)
            # 图片地址指向本服务，App 用 NSURLSession 直接下载
            item["id"] = "%d" % entry["id"]
            item["url"] = "http://%s/img/%d.png" % (self.headers.get("Host", "127.0.0.1:8765"), entry["id"])
            items.append(item)

        body = json.dumps(items, ensure_ascii=False, indent=2).encode("utf-8")
        self._send(200, "application/json; charset=utf-8", body)

    def _serve_image(self, path):
        name = os.path.basename(path)
        try:
            index = int(name.split(".")[0])
        except ValueError:
            self._send(400, "application/json; charset=utf-8",
                       json.dumps({"error": "bad id"}).encode("utf-8"))
            return

        if index < 0 or index >= CATALOG_SIZE:
            self._send(404, "application/json; charset=utf-8",
                       json.dumps({"error": "out of range"}).encode("utf-8"))
            return

        entry = CATALOG[index]
        width = 480
        height = max(1, int(width * (entry["height"] / entry["width"])))
        png = render_artwork_png(entry["seed"], width, height)
        # 演示缓存命中：设置 ETag 与 Cache-Control
        self._send(200, "image/png", png, {
            "Cache-Control": "max-age=86400",
            "ETag": '"pf-%d-%dx%d"' % (index, width, height),
        })

    def log_message(self, fmt, *args):
        # 精简日志，只保留关键请求
        sys.stderr.write("[mock] %s\n" % (fmt % args))


def main():
    parser = argparse.ArgumentParser(description="PicFlow 自研图库 Mock 服务")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print("=" * 56)
    print("  PicFlow Mock Server 已启动")
    print("  图库接口 : http://%s:%d/catalog.json?page=1&limit=24" % (args.host, args.port))
    print("  图片接口 : http://%s:%d/img/0.png" % (args.host, args.port))
    print("  真机访问 : http://<你的局域网IP>:%d/catalog.json" % args.port)
    print("  按 Ctrl+C 停止")
    print("=" * 56)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n已停止")
        server.server_close()


if __name__ == "__main__":
    main()
