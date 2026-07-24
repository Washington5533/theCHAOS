"""
模拟霓虹发光球效果，与当前效果对比
"""
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

def generate_blinn_phong_ball(size, radius, light_dir=(0.3, -0.5, 0.8)):
    img = np.zeros((size, size), dtype=np.float64)
    cx, cy = size // 2, size // 2
    lx, ly, lz = light_dir
    ln = np.sqrt(lx*lx + ly*ly + lz*lz)
    lx, ly, lz = lx/ln, ly/ln, lz/ln
    for y in range(size):
        for x in range(size):
            dx, dy = x - cx, y - cy
            dist = np.sqrt(dx*dx + dy*dy)
            if dist > radius:
                continue
            nz = np.sqrt(radius*radius - dx*dx - dy*dy) / radius
            nx, ny = dx/radius, dy/radius
            diff = max(0, nx*lx + ny*ly + nz*lz)
            hx, hy, hz = lx, ly, lz + 1.0
            hn = np.sqrt(hx*hx + hy*hy + hz*hz)
            hx, hy, hz = hx/hn, hy/hn, hz/hn
            spec_dot = max(0, nx*hx + ny*hy + nz*hz)
            spec = spec_dot ** 32
            img[y, x] = min(255, int((diff * 0.6 + spec * 0.8) * 255))
    return img

def lerp_color(c1, c2, t):
    t = max(0, min(1, t))
    return tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(3))

def compute_ball_color_neon(ball_idx, omega):
    abs_omega = abs(omega)
    t = min(1.0, abs_omega / 8.0)
    if ball_idx == 0:
        cold = (180, 20, 20)
        hot = (255, 50, 50)
        white_hot = (255, 200, 180)
    else:
        cold = (20, 60, 220)
        hot = (80, 180, 255)
        white_hot = (200, 230, 255)
    if t < 0.7:
        return lerp_color(cold, hot, t / 0.7)
    else:
        return lerp_color(hot, white_hot, (t - 0.7) / 0.3)

def render_neon_ball(brightness, base_color, size, radius):
    """霓虹球渲染: 外发光 + 提亮球体 + 边缘rim light + 内部亮核"""
    cx, cy = size // 2, size // 2
    br, bg, bb = base_color
    
    # 创建 RGBA 画布
    canvas = np.zeros((size, size, 4), dtype=np.float64)
    
    # 1. 外发光 (多层)
    glow_r = int(radius * 1.8)
    for layer in range(4, 0, -1):
        layer_r = glow_r * layer / 4
        alpha = 0.06 * (1.0 - layer / 4 * 0.5)
        gr, gg, gb = br * alpha, bg * alpha, bb * alpha
        for y in range(size):
            for x in range(size):
                dx, dy = x - cx, y - cy
                if dx*dx + dy*dy <= layer_r*layer_r:
                    canvas[y, x, 0] = min(255, canvas[y, x, 0] + gr)
                    canvas[y, x, 1] = min(255, canvas[y, x, 1] + gg)
                    canvas[y, x, 2] = min(255, canvas[y, x, 2] + gb)
                    canvas[y, x, 3] = min(255, canvas[y, x, 3] + alpha * 255)
    
    # 2. 球体主体 (提亮 + rim light)
    for y in range(size):
        for x in range(size):
            dx, dy = x - cx, y - cy
            dist = np.sqrt(dx*dx + dy*dy)
            if dist > radius:
                continue
            b = brightness[y, x]
            if b <= 0:
                continue
            # rim light
            dist_norm = dist / radius
            rim = pow(dist_norm, 3.0) * 0.5
            neon_b = (b / 255.0) * 1.3 + rim
            neon_b = min(1.0, neon_b)
            cr = min(255, br * neon_b)
            cg = min(255, bg * neon_b)
            cb = min(255, bb * neon_b)
            canvas[y, x, 0] = cr
            canvas[y, x, 1] = cg
            canvas[y, x, 2] = cb
            canvas[y, x, 3] = 255
    
    # 3. 内部亮核
    core_r = int(radius * 0.25)
    core_alpha = 0.5
    for y in range(size):
        for x in range(size):
            dx, dy = x - cx, y - cy
            if dx*dx + dy*dy <= core_r*core_r:
                cr = br * core_alpha + 255 * (1 - core_alpha)
                cg = bg * core_alpha + 255 * (1 - core_alpha)
                cb = bb * core_alpha + 255 * (1 - core_alpha)
                canvas[y, x, 0] = min(255, cr)
                canvas[y, x, 1] = min(255, cg)
                canvas[y, x, 2] = min(255, cb)
                canvas[y, x, 3] = 255
    
    return np.clip(canvas, 0, 255).astype(np.uint8)

def render_normal_ball(brightness, base_color, size):
    img = np.zeros((size, size, 3), dtype=np.uint8)
    br, bg, bb = base_color
    for y in range(size):
        for x in range(size):
            b = brightness[y, x]
            if b > 0:
                img[y, x] = [min(255, int(br * b / 255)), min(255, int(bg * b / 255)), min(255, int(bb * b / 255))]
    return img

print("生成霓虹球 vs 普通球对比预览...")

size1, r1 = 102, 50
size2, r2 = 82, 40
bright1 = generate_blinn_phong_ball(size1, r1)
bright2 = generate_blinn_phong_ball(size2, r2)

omegas = [0.0, 3.0, 6.0, 8.0]
canvas_w, canvas_h = 900, 600
canvas = Image.new('RGB', (canvas_w, canvas_h), (10, 10, 20))
draw = ImageDraw.Draw(canvas)

# ── 上半部分: 当前效果 ──
draw.text((20, 10), "BEFORE: 普通 Blinn-Phong 球", fill=(180, 180, 180))
draw.text((20, 30), "Ball 1 (暗红→亮红)", fill=(200, 100, 100))

for i, omega in enumerate(omegas):
    color = lerp_color((80,30,30), (255,60,60), min(1, abs(omega)/8.0/0.7)) if abs(omega)/8.0 < 0.7 else lerp_color((255,60,60),(255,220,180),(abs(omega)/8.0-0.7)/0.3)
    ball = render_normal_ball(bright1, color, size1)
    pil = Image.fromarray(ball).crop((1, 1, size1-1, size1-1))
    canvas.paste(pil, (30 + i * 200, 55))
    draw.text((30 + i*200, 160), f"w={omega:.0f}", fill=(120,120,120))

draw.text((20, 190), "Ball 2 (暗蓝→亮蓝)", fill=(100, 150, 255))
for i, omega in enumerate(omegas):
    color = lerp_color((30,40,80), (140,200,255), min(1, abs(omega)/8.0/0.7)) if abs(omega)/8.0 < 0.7 else lerp_color((140,200,255),(220,240,255),(abs(omega)/8.0-0.7)/0.3)
    ball = render_normal_ball(bright2, color, size2)
    pil = Image.fromarray(ball).crop((1, 1, size2-1, size2-1))
    canvas.paste(pil, (30 + i * 200, 215))
    draw.text((30 + i*200, 300), f"w={omega:.0f}", fill=(120,120,120))

# ── 下半部分: 霓虹效果 ──
draw.text((20, 330), "AFTER: 霓虹发光球 (新增: 外发光 + rim light + 亮核)", fill=(100, 255, 150))
draw.text((20, 350), "Ball 1 (霓虹红)", fill=(255, 80, 80))

for i, omega in enumerate(omegas):
    color = compute_ball_color_neon(0, omega)
    ball = render_neon_ball(bright1, color, size1, r1)
    pil = Image.fromarray(ball, 'RGBA').convert('RGB')
    pil = pil.crop((1, 1, size1-1, size1-1))
    canvas.paste(pil, (30 + i * 200, 375))
    draw.text((30 + i*200, 480), f"w={omega:.0f}", fill=(120,120,120))

draw.text((20, 500), "Ball 2 (霓虹蓝)", fill=(80, 180, 255))
for i, omega in enumerate(omegas):
    color = compute_ball_color_neon(1, omega)
    ball = render_neon_ball(bright2, color, size2, r2)
    pil = Image.fromarray(ball, 'RGBA').convert('RGB')
    pil = pil.crop((1, 1, size2-1, size2-1))
    canvas.paste(pil, (30 + i * 200, 520))
    draw.text((30 + i*200, 590), f"w={omega:.0f}", fill=(120,120,120))

output_path = r"c:\Users\wst\Desktop\Game-easyx\tools\ball_preview_neon.png"
canvas.save(output_path)
print(f"已保存: {output_path}")
