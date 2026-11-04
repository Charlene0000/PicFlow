#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
png_writer.py —— 手写 PNG 编码器（零第三方依赖）

为什么自己写 PNG 编码而不是用 Pillow？
  1. 项目要求「零外部依赖」，Pillow 是第三方库；
  2. 手写 PNG 只需要 zlib（Python 标准库）+ struct，代码量小且可读；
  3. 面试时能讲清楚 PNG 的 chunk 结构与 CRC 计算，本身就是加分点。

PNG 文件结构：
  8 字节签名：\\x89PNG\\r\\n\\x1a\\n
  若干 chunk：[长度(4B)][类型(4B)][数据][CRC32(4B)]
  必备 chunk：IHDR（图像头）、IDAT（压缩数据）、IEND（结束标记）

IHDR 数据（13 字节）：
  宽(4B) 高(4B) 位深(1B) 颜色类型(1B) 压缩(1B) 滤波(1B) 交错(1B)
"""

import struct
import zlib


def _chunk(chunk_type: bytes, data: bytes) -> bytes:
    """组装一个 PNG chunk：长度 + 类型 + 数据 + CRC32。"""
    return (struct.pack(">I", len(data)) +
            chunk_type +
            data +
            struct.pack(">I", zlib.crc32(chunk_type + data) & 0xFFFFFFFF))


def encode_png(width: int, height: int, pixels: bytes) -> bytes:
    """
    把 RGB 像素数据编码成 PNG。

    @param width  图像宽度
    @param height 图像高度
    @param pixels RGB 三通道原始像素，长度必须为 width*height*3
    """
    if len(pixels) != width * height * 3:
        raise ValueError("像素数据长度不匹配：期望 %d，实际 %d"
                         % (width * height * 3, len(pixels)))

    # 每行前置一个滤波类型字节：0 = None（不滤波）
    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)
        raw.extend(pixels[y * stride:(y + 1) * stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)

    return (b"\x89PNG\r\n\x1a\n" +
            _chunk(b"IHDR", ihdr) +
            _chunk(b"IDAT", zlib.compress(bytes(raw), 9)) +
            _chunk(b"IEND", b""))


def hsl_to_rgb(h: float, s: float, l: float):
    """HSL -> RGB，h 为 0~360 度，s/l 为 0~1。返回 (r, g, b)，每个 0~255。"""
    h = h % 360.0
    s = max(0.0, min(1.0, s))
    l = max(0.0, min(1.0, l))

    c = (1.0 - abs(2.0 * l - 1.0)) * s
    hp = h / 60.0
    x = c * (1.0 - abs(hp % 2.0 - 1.0))
    m = l - c / 2.0

    if hp < 1.0:
        r, g, b = c, x, 0.0
    elif hp < 2.0:
        r, g, b = x, c, 0.0
    elif hp < 3.0:
        r, g, b = 0.0, c, x
    elif hp < 4.0:
        r, g, b = 0.0, x, c
    elif hp < 5.0:
        r, g, b = x, 0.0, c
    else:
        r, g, b = c, 0.0, x

    return (int((r + m) * 255), int((g + m) * 255), int((b + m) * 255))


def render_artwork_png(seed: int, width: int, height: int,
                       palette_func=None) -> bytes:
    """
    按 seed 生成一张程序化图案并编码为 PNG。

    配色与图案规则与 iOS 端的 core/picflow_palette.c 保持一致
    （同样的 HSL 推导、同样的图案类型划分），
    这样 mock server 下发的图与本地生成的是同一套视觉语言。
    """
    if palette_func is None:
        # 简易回退配色（不依赖 C 引擎）
        hue = (seed * 47) % 360
        primary = hsl_to_rgb(hue, 0.62, 0.52)
        secondary = hsl_to_rgb(hue + 150, 0.53, 0.58)
        accent = hsl_to_rgb(hue + 38, 0.68, 0.60)
        background = hsl_to_rgb(hue, 0.14, 0.93)
        pattern = seed % 5
    else:
        spec = palette_func(seed)
        primary = tuple(int(v) for v in spec["primary"])
        secondary = tuple(int(v) for v in spec["secondary"])
        accent = tuple(int(v) for v in spec["accent"])
        background = tuple(int(v) for v in spec["background"])
        pattern = spec["pattern"]

    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            # 基础背景
            r, g, b = background

            if pattern == 0:      # 网格
                if (x // 24 + y // 24) % 2 == 0:
                    r, g, b = primary
            elif pattern == 1:    # 同心圆
                cx, cy = width / 2.0, height / 2.0
                d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
                ring = int(d / 14) % 2
                if ring == 0:
                    r, g, b = primary
            elif pattern == 2:    # 斜条纹
                if ((x + y) // 12) % 2 == 0:
                    r, g, b = secondary
            elif pattern == 3:    # 波形
                wave = int((height / 2.0 + 18 * _sin(x / 18.0)) / 24)
                if wave % 2 == 0:
                    r, g, b = accent
            else:                 # 三角
                if ((x + y) // 20) % 3 == 0:
                    r, g, b = accent

            pixels.extend((r, g, b))

    return encode_png(width, height, bytes(pixels))


def _sin(v: float) -> float:
    """轻量正弦（避免为 mock server 引入 math 之外的依赖场景问题）。"""
    import math
    return math.sin(v)
