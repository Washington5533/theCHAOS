#!/usr/bin/env python3
"""
将 robot.png 的黑色背景转为透明，输出 robot_alpha.png
用法: python make_robot_transparent.py
"""
import os
from PIL import Image

def process(input_path, output_path, threshold=30, feather=15):
    """
    threshold: 低于此亮度的像素视为纯黑，完全透明
    feather:   边缘羽化范围 (threshold ~ threshold+feather)，平滑过渡
    """
    img = Image.open(input_path).convert("RGBA")
    pixels = img.load()
    w, h = img.size
    count = 0

    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            brightness = max(r, g, b)

            if brightness <= threshold:
                # 纯黑 → 完全透明
                pixels[x, y] = (r, g, b, 0)
                count += 1
            elif brightness <= threshold + feather:
                # 边缘羽化：亮度越低越透明
                t = (brightness - threshold) / feather  # 0~1
                new_alpha = int(t * 255)
                pixels[x, y] = (r, g, b, min(a, new_alpha))

    img.save(output_path)
    print(f"Done: {output_path}")
    print(f"  Size: {w}x{h}")
    print(f"  Transparent pixels: {count} ({count*100//(w*h)}%)")
    print(f"  Threshold: {threshold}, Feather: {feather}")


if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    assets_dir = os.path.join(script_dir, "..", "assets")

    input_file = os.path.join(assets_dir, "robot.png")
    output_file = os.path.join(assets_dir, "robot.png")  # 直接覆盖原文件

    if not os.path.exists(input_file):
        print(f"Error: {input_file} not found")
        exit(1)

    process(input_file, output_file)
