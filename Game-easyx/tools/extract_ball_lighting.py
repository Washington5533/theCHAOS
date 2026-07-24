#!/usr/bin/env python3
# ============================================================
# 球体光照数据提取工具 (Python版)
# 输出: Game/ball_lighting.h
# ============================================================
import math
import os

def compute_brightness(radius):
    """计算 Blinn-Phong 球体亮度数组, 返回 size*size 的 list"""
    size = radius * 2 + 2
    cx = cy = radius + 1

    # 光源方向 (左上)
    lx, ly, lz = -0.6, -0.6, 0.8
    l_len = math.sqrt(lx*lx + ly*ly + lz*lz)
    lx /= l_len; ly /= l_len; lz /= l_len

    data = []
    for y in range(size):
        for x in range(size):
            dx = (x - cx) / radius
            dy = (y - cy) / radius
            dist_sq = dx * dx + dy * dy

            bright = 0
            if dist_sq <= 1.0:
                dist = math.sqrt(dist_sq)
                if dist > 0.001:
                    nx, ny = dx / dist, dy / dist
                else:
                    nx, ny = 0.0, 0.0
                nz = math.sqrt(1.0 - dist_sq)

                # Lambertian 漫反射
                ndotl = nx*lx + ny*ly + nz*lz
                if ndotl < 0.0:
                    ndotl = 0.0
                ambient = 0.18
                diffuse = ambient + (1.0 - ambient) * ndotl

                # Blinn-Phong 镜面高光
                hx, hy, hz = lx, ly, lz + 1.0
                h_len = math.sqrt(hx*hx + hy*hy + hz*hz)
                ndoth = (nx*hx + ny*hy + nz*hz) / h_len
                if ndoth < 0.0:
                    ndoth = 0.0
                specular = math.pow(ndoth, 32.0) * 0.7

                # 边缘暗化 (Fresnel-like)
                edge = 1.0 - dist_sq * 0.3

                val = diffuse * edge + specular
                if val > 1.0:
                    val = 1.0
                bright = int(val * 255)
                if bright > 255:
                    bright = 255
            data.append(bright)
    return data, size


def write_array(f, name, radius, data, size):
    f.write(f"// {name} 球体 Blinn-Phong 亮度数据 (R={radius}, size={size})\n")
    f.write(f"static const unsigned char {name}[{size} * {size}] = {{\n")
    for y in range(size):
        f.write("    ")
        line = []
        for x in range(size):
            line.append(f"{data[y * size + x]:3d}")
        f.write(",".join(line))
        if y < size - 1:
            f.write(",")
        f.write("\n")
    f.write("};\n\n")


def main():
    out_path = os.path.join(os.path.dirname(__file__), "..", "Game", "ball_lighting.h")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    with open(out_path, "w") as f:
        f.write("// ============================================================\n")
        f.write("// 预计算球体光照数据 (自动生成, 勿手动编辑)\n")
        f.write(f"// 工具: tools/extract_ball_lighting.py\n")
        f.write("// ============================================================\n")
        f.write("#pragma once\n\n")

        data1, size1 = compute_brightness(50)
        write_array(f, "BALL1_BRIGHT", 50, data1, size1)

        data2, size2 = compute_brightness(40)
        write_array(f, "BALL2_BRIGHT", 40, data2, size2)

        f.write("// 辅助宏: 取亮度值\n")
        f.write("#define BALL_BRIGHT(arr, size, x, y) arr[(y)*(size) + (x)]\n")

    print(f"Generated: {out_path}")
    print(f"  BALL1: {size1}x{size1} = {len(data1)} bytes")
    print(f"  BALL2: {size2}x{size2} = {len(data2)} bytes")


if __name__ == "__main__":
    main()
