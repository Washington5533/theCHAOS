# Checkpoint 8 — 球体精致动态渲染方案

> 当前 M1/M2 球体使用纯色填充圆，缺乏立体感和动态反馈。本文档系统性列出所有可行的渲染升级方案，包括技术原理、实现路径、性能评估和推荐优先级。

---

## 目录

- [零、现状分析](#零现状分析)
- [一、立体光照 — 3D 球体感](#一立体光照--3d-球体感)
- [二、速度响应变色](#二速度响应变色)
- [三、外发光 Bloom](#三外发光-bloom)
- [四、交互反馈特效](#四交互反馈特效)
- [五、运动残影](#五运动残影)
- [六、表面纹理与细节](#六表面纹理与细节)
- [七、贴图方案](#七贴图方案)
- [八、动效方案](#八动效方案)
- [九、性能评估总览](#九性能评估总览)
- [十、推荐实施路线](#十推荐实施路线)

---

## 零、现状分析

### 当前代码

```cpp
// Pendulum.cpp:88-98
void Pendulum::drawFillCircle(SDL_Renderer *r, int cx, int cy,
                              int radius, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int y = -radius; y <= radius; y++) {
        int w = (int)std::sqrt(radius * radius - y * y);
        SDL_RenderDrawLine(r, cx - w, cy + y, cx + w, cy + y);
    }
}
```

### 当前球体参数

| 球体 | 半径 | 颜色 | 说明 |
|------|------|------|------|
| 支点 (pivot) | 4 | 黄色 (255,255,100) | 固定，较小 |
| M1 关节球 | 50 | 红色 (255,100,100) | 大球，角色可行走 |
| M2 末端球 | 40 | 蓝色 (100,200,255) | 大球，角色可行走 |

### 问题

1. **纯色填充** — 没有高光、阴影、渐变，完全是 2D 色块
2. **无动态反馈** — 无论 ω=0 还是 ω=8 rad/s（表面速度 400 px/s），球看起来一模一样
3. **无交互反馈** — 角色踩上、钩中、被甩出时球没有任何视觉变化
4. **缺乏深度感** — 球和杆之间没有遮挡/阴影关系，层次不清

### 约束条件

- **渲染器**：SDL2 Software Renderer（无 GPU shader）
- **目标帧率**：60 FPS
- **球数量**：3（支点不计入优化目标）
- **当前物理**：32 substep/帧，double 精度
- **可用技术**：逐像素 alpha 混合、预计算纹理、SDL2 的 `SDL_RenderCopy` / `SDL_CreateTexture`

---

## 一、立体光照 — 3D 球体感

### 原理

用径向渐变模拟漫反射光照 + 镜面高光，让 2D 圆看起来像 3D 球体。

```
         ☀ 光源方向 (左上)
          \
     ╭─────────╮
     │  ○        │ ← 高光点 (亮白, alpha 80%)
     │   ●       │ ← 亮面 (基色 + 亮度)
     │    ███    │ ← 中间调 (基色)
     │     ████  │ ← 暗面 (基色暗化)
     │      █████│ ← 边缘暗 (环境光遮蔽)
     ╰─────────╯
        ↑ 半球暗面
```

### 方案 A：逐行径向插值（推荐）

每行根据到球心的距离计算像素颜色，逐行画出。这是最直接的方式。

```cpp
// 替代 drawFillCircle — 带径向渐变 + 高光的实心圆
void drawSphere3D(SDL_Renderer* r, int cx, int cy, int radius,
                  SDL_Color base, SDL_Color highlight,
                  double lightAngle = -M_PI / 4)  // 光源从左上角来
{
    for (int y = -radius; y <= radius; y++) {
        int w = (int)std::sqrt(radius * radius - y * y);

        for (int x = -w; x <= w; x++) {
            double dx = (double)x / radius;
            double dy = (double)y / radius;
            double distSq = dx * dx + dy * dy;
            if (distSq > 1.0) continue;

            // 表面法线 (球体表面向外)
            double dist = std::sqrt(distSq);
            double nx = dx / dist;
            double ny = dy / dist;
            double nz = std::sqrt(1.0 - distSq);  // 朝向观察者

            // Lambertian 漫反射 (光源方向 dot 法线)
            double lx = std::cos(lightAngle);
            double ly = std::sin(lightAngle);
            double lz = 0.8;  // 光源也在前方
            double lLen = std::sqrt(lx*lx + ly*ly + lz*lz);
            double NdotL = (nx*lx + ny*ly + nz*lz) / lLen;
            if (NdotL < 0.0) NdotL = 0.0;

            // 环境光 + 漫反射
            double ambient = 0.15;
            double diffuse = ambient + (1.0 - ambient) * NdotL;

            // 镜面高光 (Blinn-Phong)
            double hx = nx + lx;
            double hy = ny + ly;
            double hz = nz + lz;
            double hLen = std::sqrt(hx*hx + hy*hy + hz*hz);
            double NdotH = (nx*hx + ny*hy + nz*hz) / hLen;
            if (NdotH < 0.0) NdotH = 0.0;
            double specular = std::pow(NdotH, 32.0) * 0.6;

            // 最终颜色
            int r = (int)(base.r * diffuse + highlight.r * specular);
            int g = (int)(base.g * diffuse + highlight.g * specular);
            int b = (int)(base.b * diffuse + highlight.b * specular);
            if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;

            SDL_SetRenderDrawColor(r, r, g, b, 255);
            SDL_RenderDrawPoint(r, cx + x, cy + y);
        }
    }
}
```

**性能**：每帧 M1(50px) + M2(40px) ≈ 7,854 + 5,027 ≈ **12,881 像素**，每个像素 ~5 次浮点运算。在 60fps 下约 **0.8M 浮点运算/帧**，可接受。

**优化**：可预计算为 `SDL_Texture`（方案 B）。

### 方案 B：预计算纹理（性能最优）

启动时生成一次纹理，每帧只需 `SDL_RenderCopy`。

```cpp
// 预生成球体纹理
SDL_Texture* generateBallTexture(SDL_Renderer* r, int radius,
                                  SDL_Color base, SDL_Color highlight)
{
    int size = radius * 2 + 1;
    SDL_Texture* tex = SDL_CreateTexture(r,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, size, size);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    SDL_SetRenderTarget(r, tex);  // 渲染到纹理
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
    SDL_RenderClear(r);

    // ... 逐像素绘制 (同方案A, 坐标偏移 radius) ...

    SDL_SetRenderTarget(r, nullptr);  // 切回屏幕
    return tex;
}

// 每帧绘制:
SDL_Rect dst = {cx - R, cy - R, R*2, R*2};
SDL_RenderCopy(r, ballTex_, nullptr, &dst);
```

**性能**：每球 1 次 `SDL_RenderCopy`，GPU 处理，开销几乎为零。

**代价**：纹理需要随颜色变化而重建（见方案二的动态纹理重建）。

### 方案 C：分环近似（兼容性好）

绘制多层同心圆，每层颜色/透明度不同，近似渐变。不需要逐像素操作。

```cpp
void drawSphereRings(SDL_Renderer* r, int cx, int cy, int radius,
                      SDL_Color base)
{
    // 从外到内画 6 层
    for (int i = 5; i >= 0; i--) {
        int rr = radius * (i + 1) / 5;
        double t = (double)i / 5.0;
        // 亮面 (左上) → 暗面 (右下)
        int sr = base.r + (int)((255 - base.r) * t * 0.3);
        int sg = base.g + (int)((255 - base.g) * t * 0.3);
        int sb = base.b + (int)((255 - base.b) * t * 0.3);
        SDL_SetRenderDrawColor(r, sr, sg, sb, 255);
        drawFillCircle(r, cx, cy, rr, {sr, sg, sb, 255});
    }
    // 高光点 (左上偏移)
    int hx = cx - radius / 4;
    int hy = cy - radius / 4;
    SDL_SetRenderDrawColor(r, 255, 255, 255, 150);
    drawFillCircle(r, hx, hy, radius / 5, {255, 255, 255, 150});
}
```

**性能**：6 次圆填充 ≈ 6 × 7,850 = 47,100 像素/球（R=50），仅比原始方案多 6 倍，仍然很轻。

| 方案 | 视觉质量 | 性能 | 灵活性 | 实现难度 |
|------|---------|------|--------|---------|
| A 逐像素 | ★★★★☆ | ★★★☆☆ | ★★★★★ | 中 |
| B 预计算纹理 | ★★★★★ | ★★★★★ | ★★★☆☆ | 中 |
| C 分环近似 | ★★★☆☆ | ★★★★☆ | ★★☆☆☆ | 低 |

**推荐**：先用方案 C 快速验证效果，满意后迁移到方案 B（预计算纹理）。

---

## 二、速度响应变色

### 原理

球体颜色随角速度 ω 动态变化，给玩家直观的"这个球很危险/很安全"的视觉信号。

```
冷色 (慢)  ──────────────────→  暖色 (快)
 ω=0                          ω≥8 rad/s

M1 (红色系):
 暗红 (80,30,30)  →  正红 (255,60,60)  →  橙红 (255,140,40)  →  白热 (255,220,180)

M2 (蓝色系):
 暗蓝 (30,40,80)  →  正蓝 (60,120,255) →  亮蓝 (140,200,255) →  白热 (200,230,255)
```

### 实现

```cpp
struct BallVisual {
    SDL_Color baseColor;
    SDL_Color highlight;
    double prevOmega = 0.0;  // 上一帧 ω, 用于平滑过渡
};

// 每帧在 Pendulum::draw() 中调用
SDL_Color computeSpeedColor(double omega, double maxOmega,
                            SDL_Color cold, SDL_Color hot)
{
    double t = fabs(omega) / maxOmega;  // 0..1
    if (t > 1.0) t = 1.0;

    // 两段插值: 冷→热 (0..0.7) + 热→白热 (0.7..1.0)
    if (t < 0.7) {
        double s = t / 0.7;
        return {
            (Uint8)(cold.r + (hot.r - cold.r) * s),
            (Uint8)(cold.g + (hot.g - cold.g) * s),
            (Uint8)(cold.b + (hot.b - cold.b) * s),
            255
        };
    } else {
        double s = (t - 0.7) / 0.3;
        return {
            (Uint8)(hot.r + (255 - hot.r) * s),
            (Uint8)(hot.g + (255 - hot.g) * s),
            (Uint8)(hot.b + (255 - hot.b) * s),
            255
        };
    }
}

// 颜色平滑过渡 (避免颜色跳变)
SDL_Color lerpColor(SDL_Color a, SDL_Color b, double t) {
    return {
        (Uint8)(a.r + (b.r - a.r) * t),
        (Uint8)(a.g + (b.g - a.g) * t),
        (Uint8)(a.b + (b.b - a.b) * t),
        255
    };
}
```

### 与方案一的整合

如果使用预计算纹理（方案一.B），颜色变化时需要重建纹理。优化策略：

- **增量更新**：仅在颜色变化 > 阈值（如 Δt > 0.05）时重建
- **双缓冲纹理**：保留上一帧纹理，重建时用旧纹理过渡
- **混合方案**：纹理存亮度信息，用 `SDL_SetTextureColorMod` 调色

```cpp
// 最轻量的动态调色：纹理存灰度 + SDL 调色
SDL_SetTextureColorMod(ballTex_, r, g, b);  // 实时调色, 零开销
```

---

## 三、外发光 Bloom

### 原理

在球体后方绘制多层半透明的大圆，模拟光晕扩散。

```
      ╭───────────────────╮  ← alpha=20, r = R×2.5
      │  ╭─────────────╮  │  ← alpha=40, r = R×2.0
      │  │  ╭───────╮  │  │  ← alpha=80, r = R×1.4
      │  │  │ 球体  │  │  │  ← alpha=255, r = R
      │  │  ╰───────╯  │  │
      │  ╰─────────────╯  │
      ╰───────────────────╯
```

### 实现

```cpp
void drawBloom(SDL_Renderer* r, int cx, int cy, int radius,
               SDL_Color glowColor, double intensity)
{
    // intensity: 0=无光晕, 1=最大光晕 (映射到 speed/energy)
    int layers = 4;
    for (int i = layers; i >= 1; i--) {
        double t = (double)i / layers;
        int glowR = (int)(radius * (1.0 + t * 2.0));  // R → 3R
        int alpha = (int)(20.0 * t * intensity);

        SDL_SetRenderDrawColor(r, glowColor.r, glowColor.g, glowColor.b, alpha);
        // 画一个粗环或实心圆
        drawFillCircleAlpha(r, cx, cy, glowR, {glowColor.r, glowColor.g, glowColor.b, (Uint8)alpha});
    }
}
```

### 动态脉动

光晕随球体动能脉动：

```cpp
double kinetic = Physics::kinetic(state_, params_);
double maxKinetic = 50000.0;  // 调优值
double pulse = std::min(kinetic / maxKinetic, 1.0);
pulse = 0.2 + 0.8 * pulse;  // 基础 20% + 动能驱动 80%
```

### 性能注意

4 层光晕 × 2 个球 × 每层 ~50,000 像素（最外层 R=150）≈ 400,000 像素/帧，开销较大。优化：

- 减少层数到 3 层
- 光晕最大半径限制到 2× 球半径
- 仅在 intensity > 0.1 时绘制

---

## 四、交互反馈特效

### 4.1 玩家踩上球 — 表面波纹

角色从杆走到球面时（`warpTo` 到 M1_BALL/M2_BALL），触发短时脉冲。

```
触发时机: warpTo(ballSeg, t)  // Player.cpp:484
效果: 球体外边缘短暂扩张 2~4px, 0.2s 内回弹
```

```cpp
struct BallFeedback {
    double landPulse = 0.0;    // 踩上脉冲 (递减到0)
    double hookFlash  = 0.0;   // 钩中闪光
    double jumpRipple = 0.0;   // 跳离涟漪
};

void updateFeedback(double dt) {
    landPulse  *= std::exp(-dt * 12.0);  // ~0.2s 衰减
    hookFlash  *= std::exp(-dt * 18.0);  // ~0.15s 衰减
    jumpRipple *= std::exp(-dt * 8.0);   // ~0.3s 衰减
}
```

绘制时叠加：

```cpp
int drawRadius = params_.R1;
if (feedback.landPulse > 0.01)
    drawRadius += (int)(feedback.landPulse * 4.0);
```

### 4.2 钩中闪光

钩锁命中球时（`hook()` → `bestSeg == M1_BALL / M2_BALL`），触发白色闪光：

```cpp
// 在 hook() 成功命中球体后:
p.triggerFlash(bestSeg);  // 触发闪光

// Pendulum 中:
void triggerFlash(SurfaceSeg seg) {
    if (seg == SurfaceSeg::M1_BALL) feedback_.hookFlash = 1.0;
    else                            feedback2_.hookFlash = 1.0;
}
```

渲染时闪光表现为 **高光区域瞬间扩大**（specular power 从 32 降到 4，范围变大变亮）。

### 4.3 跳离爆发

角色从球面跳跃（`jump()` → `seg_ == M1_BALL/M2_BALL`），触发粒子爆发 + 球体短暂缩小（挤压 → 回弹）：

```cpp
// 球体挤压参数
double squash = 1.0 - feedback_.jumpRipple * 0.08;  // 最多缩小 8%
// 渲染时:
int rx = (int)(radius * (1.0 / sqrt(squash)));  // 保持面积: 横向拉伸
int ry = (int)(radius * squash);                 // 纵向压缩
```

### 4.4 球体振动 (快速旋转时)

当 |ω| > 6 rad/s 时，球体边缘微小振动（模拟高速旋转的不稳定感）：

```cpp
if (fabs(omega) > 6.0) {
    double jitter = (fabs(omega) - 6.0) * 0.3;  // 最多 ±0.6px
    cx += (int)(sin(frame * 0.7) * jitter);
    cy += (int)(cos(frame * 0.9) * jitter);
}
```

---

## 五、运动残影

### 原理

与玩家运动残影（`Player.cpp:154-161` 彗星拖尾）类似，为高速运动的球体绘制残像。

### 实现

```cpp
// Pendulum.h 新增成员
struct {
    Vec2 positions[12];   // 残影位置环
    int head = 0;
    int count = 0;
} trail1_, trail2_;

// Pendulum::step() 后记录位置
void Pendulum::recordTrail(Vec2 joint, Vec2 tip) {
    static int skip = 0;
    if (++skip >= 3) {  // 每3帧记录一次
        skip = 0;
        trail1_.positions[trail1_.head] = joint;
        trail1_.head = (trail1_.head + 1) % 12;
        if (trail1_.count < 12) trail1_.count++;

        trail2_.positions[trail2_.head] = tip;
        trail2_.head = (trail2_.head + 1) % 12;
        if (trail2_.count < 12) trail2_.count++;
    }
}

// 绘制残影 (在球体之前)
void drawTrail(SDL_Renderer* r, const TrailRing& trail, int radius,
               SDL_Color base) {
    for (int i = 0; i < trail.count; i++) {
        int idx = (trail.head - 1 - i + 12) % 12;
        float t = 1.0f - (float)i / trail.count;
        int alpha = (int)(t * 60);
        int r = (int)(radius * t);  // 残影逐渐缩小
        SDL_SetRenderDrawColor(r, base.r, base.g, base.b, alpha);
        drawFillCircle(r, (int)trail.positions[idx].x,
                         (int)trail.positions[idx].y, r, {base.r, base.g, base.b, (Uint8)alpha});
    }
}
```

### 阈值控制

仅在球体线速度超过阈值时显示残影：

```cpp
double surfaceSpeed = fabs(omega) * radius;
if (surfaceSpeed > 150.0)  // 表面速度 > 150 px/s 才显示
    recordTrail(joint, tip);
```

---

## 六、表面纹理与细节

### 6.1 经纬线网格

适合科技/工业风格，给球面画经纬线。

```
     ╭─────────╮
     │  ─────── │ ← 赤道线
     │ /   │   \│ ← 经线
     ││    │    ││
     │ \   │   /│
     │  ─────── │
     ╰─────────╯
```

### 实现：球面投影经纬线

```cpp
void drawWireframeSphere(SDL_Renderer* r, int cx, int cy, int radius,
                          SDL_Color color, double rotAngle) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);

    // 纬线 (水平环) — 3条
    for (int lat = 0; lat < 3; lat++) {
        double phi = M_PI * (0.2 + lat * 0.3);  // 北极角
        double r = radius * sin(phi);
        int yOff = (int)(radius * cos(phi));
        // 画椭圆 (投影的圆)
        for (double a = 0; a < 2 * M_PI; a += 0.02) {
            int x1 = cx + (int)(r * cos(a));
            int y1 = cy - yOff;
            int x2 = cx + (int)(r * cos(a + 0.02));
            SDL_RenderDrawLine(r, x1, y1, x2, y1);
        }
    }

    // 经线 (垂直弧) — 4条, 随旋转角度偏移
    for (int lon = 0; lon < 4; lon++) {
        double theta = rotAngle + lon * M_PI / 2;
        for (double phi = 0; phi < M_PI; phi += 0.02) {
            Vec2 p1 = Vec2::fromPolar(theta, radius * sin(phi));
            Vec2 p2 = Vec2::fromPolar(theta, radius * sin(phi + 0.02));
            SDL_RenderDrawLine(r,
                cx + (int)p1.x, cy - (int)(radius * cos(phi)),
                cx + (int)p2.x, cy - (int)(radius * cos(phi + 0.02)));
        }
    }
}
```

**旋转角度来源**：经线随球体累计旋转角度偏移（`rotAngle = ∫ω dt`），让纹理看起来在随球转动。

### 6.2 自转标记

在球面上画一个点或短线作为旋转标记，让玩家直观看到球的旋转方向和速度。

```cpp
// 在球面上画一个旋转标记 (球面极坐标)
double markerAngle = fmod(state_.alpha1 + rotOffset_, 2 * M_PI);
Vec2 marker = joint + Vec2::fromPolar(markerAngle, params_.R1 * 0.7);
SDL_SetRenderDrawColor(r, 255, 255, 255, 200);
SDL_Rect dot = {(int)marker.x - 2, (int)marker.y - 2, 4, 4};
SDL_RenderFillRect(r, &dot);
```

### 6.3 噪点纹理

给球面添加细微噪点，模拟磨砂/金属质感。

**离线生成**：用 8×8 或 16×16 的随机 alpha 纹理平铺在球面上，或：

**运行时**：在逐像素绘制时叠加伪随机噪声（性能开销大，不推荐运行时逐像素噪声）。

**推荐**：预生成一张小噪声纹理（64×64 RGBA），用 `SDL_RenderCopy` 叠加到球上，alpha 混合。

---

## 七、贴图方案

### 7.1 静态贴图

最简单：加载一张 PNG 作为球的纹理。

```cpp
// 初始化时加载
SDL_Surface* surf = SDL_LoadBMP("assets/ball_m1.bmp");  // 或使用 SDL_image 加载 PNG
SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
SDL_FreeSurface(surf);

// 每帧绘制
SDL_Rect dst = {cx - R, cy - R, R*2, R*2};
SDL_RenderCopy(renderer, tex, nullptr, &dst);
```

### 7.2 动态贴图（实时调色）

加载灰度纹理，运行时用 `SDL_SetTextureColorMod` 着色：

```cpp
// 加载灰度纹理 (白底 + 光照信息)
SDL_Texture* baseTex = loadTexture("assets/ball_base.png");

// 每帧着色 (配合速度响应变色)
SDL_Color c = computeSpeedColor(omega, 8.0, coldColor, hotColor);
SDL_SetTextureColorMod(baseTex, c.r, c.g, c.b);
SDL_RenderCopy(r, baseTex, nullptr, &dst);
```

### 7.3 程序化贴图生成

不依赖外部文件，启动时用代码生成纹理（与方案一.B 的预计算纹理一致）：

```cpp
// 生成 RGBA 纹理缓冲区
vector<Uint32> pixels(size * size);

for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
        double dx = (x - radius) / (double)radius;
        double dy = (y - radius) / (double)radius;
        double distSq = dx*dx + dy*dy;
        if (distSq > 1.0) {
            pixels[y * size + x] = 0x00000000;  // 透明
            continue;
        }
        // ... 光照计算 (同方案一) ...
        pixels[y * size + x] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

SDL_Texture* tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888,
    SDL_TEXTUREACCESS_STATIC, size, size);
SDL_UpdateTexture(tex, nullptr, pixels.data(), size * sizeof(Uint32));
```

### 7.4 多层贴图叠加

分层渲染，类似 Photoshop 图层：

```
层4: 镜面高光 (小亮斑, alpha 80%)    ← 叠加模式
层3: 速度辉光 (暖色光晕, alpha 40%)  ← 叠加, 仅高速时出现
层2: 表面纹理 (经纬线/噪点, alpha 30%)
层1: 基底 (3D 球光照, alpha 255%)    ← 底层
```

每层独立纹理或实时绘制，叠加顺序从底到顶。

---

## 八、动效方案

### 8.1 挤压与拉伸 (Squash & Stretch)

球体在以下时机变形：

| 触发 | 效果 | 时长 |
|------|------|------|
| 被钩锁命中 | 命中点凹陷 → 回弹 | 0.15s |
| 玩家踩上 | 轻微扁平 (Y压缩 5%) | 0.2s |
| 玩家跳离 | 拉伸 (Y拉长 10%) → 回弹 | 0.3s |
| 高速旋转 | 离心扁平 (两极压缩) | 持续 |

```cpp
struct BallDeform {
    double squashY  = 0.0;  // Y轴挤压量 (负=扁平, 正=拉伸)
    double stretchX = 0.0;  // X轴拉伸 (保持面积)
};

// 应用变形后绘制:
int rx = (int)(radius * (1.0 + deform.stretchX));
int ry = (int)(radius * (1.0 + deform.squashY));
// 用 SDL_RenderCopyEx 或手动椭圆绘制
```

对于离心扁平（持续效果，基于 ω）：

```cpp
double centriFlat = -fabs(omega) * 0.005;  // ω=8 → flatten 4%
deform.squashY = centriFlat * 0.5;         // 平滑衰减
deform.stretchX = -centriFlat * 0.5;
```

### 8.2 脉冲心跳

球体以微小幅度持续缩放，模拟"活着的"感觉：

```cpp
double heartbeat = 1.0 + sin(frame * 0.05) * 0.015;  // ±1.5%, 慢速
int animatedRadius = (int)(radius * heartbeat);
```

### 8.3 碰撞波纹

当角色钩锁命中球体时，从命中点扩散出一个圆形波纹：

```cpp
struct Ripple {
    Vec2 origin;      // 命中点 (球面局部坐标)
    double radius;     // 当前波纹半径
    double maxRadius;  // 最大扩散半径
    double life;       // 剩余生命
    double speed;      // 扩散速度 px/s
};

void updateRipple(Ripple& r, double dt) {
    r.radius += r.speed * dt;
    r.life -= dt;
}

void drawRipple(SDL_Renderer* r, Vec2 ballCenter, const Ripple& rp) {
    if (rp.life <= 0) return;
    double alpha = rp.life / 0.3;  // 0.3s 生命周期
    int r = (int)rp.radius;
    // 画圆环
    SDL_SetRenderDrawColor(r, 255, 255, 255, (int)(alpha * 180));
    for (int y = -r; y <= r; y++) {
        int w = (int)sqrt(r*r - y*y);
        SDL_RenderDrawPoint(r, ballCenter.x + rp.origin.x - w, ballCenter.y + rp.origin.y + y);
        SDL_RenderDrawPoint(r, ballCenter.x + rp.origin.x + w, ballCenter.y + rp.origin.y + y);
    }
}
```

### 8.4 轨道粒子

球体表面持续掉落微小粒子（火花/碎屑），模拟高速运动或"不稳定能量"：

```cpp
// 每 N 帧从球表面随机位置生成一个火花粒子
if (fabs(omega) > 3.0 && rand() % 3 == 0) {
    Particle p;
    double ang = rand() % 360 * M_PI / 180.0;
    p.pos = ballCenter + Vec2::fromPolar(ang, radius);
    p.vel = Vec2::fromPolar(ang, 30 + rand() % 60)
          + Vec2(omega * radius * 0.1, 0);  // 旋转方向初始速度
    p.life = 0.3f + (rand() % 100) / 500.0f;
    particles_.push_back(p);
}
```

---

## 九、性能评估总览

### 每帧开销估算 (60fps 目标: 16.67ms/帧)

| 方案 | 每球开销 | 两球合计 | 占比 | 备注 |
|------|---------|---------|------|------|
| **当前纯色圆** | ~0.02ms | ~0.04ms | 0.2% | 基准线 |
| 一.A 逐像素渐变 | ~0.15ms | ~0.30ms | 1.8% | 无优化 |
| 一.B 预计算纹理 | ~0.01ms | ~0.02ms | 0.1% | 仅 `RenderCopy` |
| 一.C 分环近似 | ~0.05ms | ~0.10ms | 0.6% | 6层同心圆 |
| 二 速度变色 | ~0.001ms | ~0.002ms | ~0% | 纯算术 |
| 三 外发光 | ~0.8ms | ~1.6ms | 9.6% | 4层光晕, 需优化 |
| 三 外发光(优化) | ~0.3ms | ~0.6ms | 3.6% | 3层, 最大2R |
| 四 交互反馈 | ~0.05ms | ~0.10ms | 0.6% | 参数计算 + 小幅重绘 |
| 五 运动残影 | ~0.1ms | ~0.2ms | 1.2% | 仅高速时触发 |
| 六 经纬线 | ~0.3ms | ~0.6ms | 3.6% | 大量短线段 |
| 七 程序化贴图 | ~0.01ms | ~0.02ms | 0.1% | 同预计算纹理 |
| 八 动效综合 | ~0.2ms | ~0.4ms | 2.4% | 含粒子/波纹 |

### 组合方案预算

| 组合 | 预估总开销 | 占比 | 可行性 |
|------|-----------|------|--------|
| 一.B + 二 + 四 | ~0.15ms | 0.9% | ✅ 极轻 |
| 一.B + 二 + 三(优化) + 四 | ~0.8ms | 4.8% | ✅ 安全 |
| 一.B + 二 + 三 + 四 + 五 + 六 | ~2.2ms | 13.2% | ⚠️ 需验证 |
| 全部方案 | ~3.5ms | 21% | ⚠️ 可能掉帧 |

---

## 十、推荐实施路线

### Phase 1：核心升级（1-2 小时）

```
一.C 分环近似 → 二 速度变色
```

最低成本拿到 3D 球体感 + 动态速度反馈。不依赖纹理，全部实时代码。

**改动文件**：
- `Pendulum.h` — 新增 `drawSphereRings()`, 速度颜色计算
- `Pendulum.cpp` — `draw()` 中替换 `drawFillCircle` 调用

### Phase 2：性能优化（30 分钟）

```
一.C → 一.B (预计算纹理)
```

将分环改为启动时预计算纹理，`SDL_RenderCopy` 替代多层圆填充。

**改动文件**：
- `Pendulum.h` — 新增 `SDL_Texture* ballTex1_, ballTex2_`
- `Pendulum.cpp` — `Pendulum()` 生成纹理, `draw()` 用 `RenderCopy`

### Phase 3：交互反馈（1 小时）

```
四 交互反馈
```

玩家踩球、钩中、跳离时的视觉反馈。最直接影响"手感"。

**改动文件**：
- `Pendulum.h` — 新增 `BallFeedback` 结构, `triggerFlash()` 等
- `Pendulum.cpp` — `draw()` 叠加反馈效果
- `Player.cpp` — `jump()`, `hook()`, `warpTo()` 中触发反馈

### Phase 4：外发光（30 分钟）

```
三 外发光 (优化版)
```

仅在高动能时触发，3 层、最大 2R 半径。

### Phase 5：高级效果（按需）

```
五 运动残影 → 六 表面纹理 → 八 动效
```

根据实际视觉需求选择性实施。

---

## 附录 A：完整接口设计

```cpp
// Pendulum.h 扩展后的接口

class Pendulum {
public:
    // ... 现有接口不变 ...

    // ── 渲染升级 ──
    void draw(SDL_Renderer* r, Vec2 pivot, uint64_t frame);  // 增强版

    // 交互触发
    void triggerLand(SurfaceSeg seg);    // 玩家踩上
    void triggerHookHit(SurfaceSeg seg, Vec2 hitPoint);  // 钩锁命中
    void triggerJumpOff(SurfaceSeg seg); // 玩家跳离

    // 获取球体动能 (用于外部 Bloom 等)
    double ballKineticEnergy(SurfaceSeg seg) const;

private:
    // 预计算纹理
    SDL_Texture* ballTex_[2] = {nullptr, nullptr};  // [M1, M2]
    void generateBallTextures(SDL_Renderer* r);
    SDL_Color computeBallColor(int ballIdx, double omega) const;

    // 反馈状态
    BallFeedback feedback_[2];   // [M1, M2]
    TrailRing trail_[2];         // 运动残影
    std::vector<Ripple> ripples_;// 碰撞波纹

    // 粒子
    std::vector<Particle> sparkParticles_;

    // 绘制子函数
    void drawSphere3D(SDL_Renderer* r, int cx, int cy, int radius,
                      SDL_Color base, double omega, BallFeedback& fb);
    void drawBloom(SDL_Renderer* r, int cx, int cy, int radius,
                   SDL_Color color, double intensity);
    void drawTrail(SDL_Renderer* r, const TrailRing& trail, int radius,
                   SDL_Color base);
    void drawRipples(SDL_Renderer* r, Vec2 ballCenter);
    void drawWireframe(SDL_Renderer* r, int cx, int cy, int radius,
                       double rotAngle, SDL_Color color);
};
```

## 附录 B：配置参数集中管理

```cpp
// Physics.h 或新文件 BallRenderConfig.h
namespace BallRenderConfig {

// ── 光照 ──
constexpr double LIGHT_ANGLE = -M_PI / 4.0;  // 光源方向
constexpr double AMBIENT = 0.15;              // 环境光
constexpr double SPECULAR_POWER = 32.0;       // 高光聚光度
constexpr double SPECULAR_INTENSITY = 0.6;    // 高光强度

// ── 速度颜色 ──
constexpr double MAX_OMEGA_VISUAL = 8.0;     // 颜色变化上限 (rad/s)
constexpr SDL_Color M1_COLD = {80, 30, 30, 255};
constexpr SDL_Color M1_HOT  = {255, 60, 60, 255};
constexpr SDL_Color M2_COLD = {30, 40, 80, 255};
constexpr SDL_Color M2_HOT  = {60, 120, 255, 255};

// ── Bloom ──
constexpr int   BLOOM_LAYERS = 3;
constexpr double BLOOM_MAX_RADIUS_MULT = 2.0;
constexpr double BLOOM_KINETIC_THRESHOLD = 5000.0;

// ── 反馈 ──
constexpr double LAND_PULSE_DECAY = 12.0;
constexpr double HOOK_FLASH_DECAY = 18.0;
constexpr double JUMP_RIPPLE_DECAY = 8.0;

// ── 残影 ──
constexpr int    TRAIL_MAX = 12;
constexpr double TRAIL_SPEED_THRESHOLD = 150.0;  // px/s

// ── 粒子 ──
constexpr int SPARKS_PER_SEC = 10;
constexpr double SPARK_MIN_OMEGA = 3.0;

}
```

---

## 十一、整体美术风格路线

在动手做任何单个资源之前，先选定一个统一的美术方向。以下是 6 条完整路线，每条都有明确的配色、质感、参考和生图关键词。

### 路线对比总览

| # | 风格 | 氛围 | 开发难度 | 适合的玩法感受 |
|---|------|------|---------|--------------|
| A | **暗黑哥特** | 神秘、沉重 | 中 | 硬核物理、步步为营 |
| B | **赛博霓虹** | 高速、电子 | 低 | 爽快操作、视觉刺激 |
| C | **炼金手稿** | 古典、学术 | 低 | 实验感、探索物理 |
| D | **虚空极简** | 禅意、孤独 | 最低 | 专注操作、心流 |
| E | **生物机械** | 陌生、有机 | 高 | 异世界感、沉浸叙事 |
| F | **日式绘卷** | 诗意、飘逸 | 中 | 轻盈跳跃、韵律感 |

### A. 暗黑哥特 Dark Gothic

```
色调: 深紫黑底 + 暗金点缀 + 血红高光
质感: 锻铁、锈铜、宝石光泽、烛光
背景: 石砌地牢 / 哥特教堂拱顶 / 铁链悬挂
角色: 斗篷剪影 / 锁链缠绕的身形
```

**参考关键词**: dark gothic, wrought iron, cathedral, candlelit, rusted bronze, blood gem, dungeon

**Midjourney prompt 模板**:
```
dark gothic stone dungeon background, wrought iron chains hanging from vaulted ceiling,
candlelight flickering, deep shadows, purple-black tones, atmospheric fog,
gaming background, 2D platformer, 1000x750 --ar 4:3 --style raw
```

### B. 赛博霓虹 Cyber Neon

```
色调: 深蓝黑底 + 品红/青色霓虹 + 亮白描边
质感: 发光线条、玻璃反射、全息投影、网格地面
背景: 数据空间 / 霓虹网格 / 虚拟训练场
角色: 发光轮廓 / 数据传输形态
```

**参考关键词**: cyberpunk, neon glow, holographic, grid, synthwave, Tron, wireframe, datastream

**Midjourney prompt 模板**:
```
cyberpunk virtual training arena, neon grid floor, pink and cyan glowing lines,
holographic particles, dark void background, retrowave aesthetic,
gaming background 2D, 1000x750 --ar 4:3 --style raw
```

### C. 炼金手稿 Alchemical Manuscript

```
色调: 羊皮纸底色 + 深棕墨水 + 金箔 + 深蓝边框
质感: 旧纸纹理、铜版画线条、蜡封、星图
背景: 巨型手稿页面 / 天文图 / 机械图纸
角色: 墨迹人形 / 几何符号
```

**参考关键词**: alchemical manuscript, parchment, copper engraving, da Vinci sketch,
celestial map, gold leaf, aged paper, scientific illustration

**Midjourney prompt 模板**:
```
ancient alchemical manuscript background, celestial diagrams, copper plate engravings,
aged parchment texture, gold leaf details, mechanical sketches, renaissance scientific art,
2D game background, 1000x750 --ar 4:3 --style raw
```

### D. 虚空极简 Void Minimal

```
色调: 纯黑底 + 单色灰白线 + 微弱的单色渐变
质感: 无纹理、纯粹几何、微妙投影
背景: 虚空 / 淡色网格线 / 极浅渐变
角色: 简单几何形 / 白点 + 尾迹
```

**参考关键词**: minimalist, void, subtle gradient, geometric, zen, negative space,
monochrome, ambient occlusion only

**Midjourney prompt 模板**:
```
minimalist void background, subtle dark gradient from deep gray to pure black,
barely visible geometric grid lines, zen atmosphere, negative space,
2D game background clean, 1000x750 --ar 4:3 --style raw
```

### E. 生物机械 Biomechanical

```
色调: 暗肉色底 + 骨质白 + 蓝色静脉 + 金属灰
质感: 几丁质外壳、肌腱纤维、金属骨骼混合
背景: 巨大生物体腔 / 骨骼框架 / 有机管壁
角色: 寄生体 / 半生物半机械形态
```

**参考关键词**: biomechanical, H.R. Giger, chitin, sinew, organic metal, bone architecture,
visceral, alien interior, living machine

**Midjourney prompt 模板**:
```
biomechanical interior of a living machine, chitin walls with metallic veins,
bone-like structural arches, organic tubing, dark amber and blue-gray tones,
H.R. Giger inspired, 2D game background, 1000x750 --ar 4:3 --style raw
```

### F. 日式绘卷 Japanese Emakimono

```
色调: 暖黄麻纸底 + 墨色线条 + 朱红点缀 + 金泥云
质感: 和纸纤维、水墨晕染、金粉
背景: 卷轴画 / 浮世绘风景 / 云雾山峦
角色: 墨笔小人 / 和服剪影
```

**参考关键词**: japanese emakimono scroll, sumi-e ink wash, ukiyo-e,
gold clouds, washi paper texture, vermillion accents, edo period art

**Midjourney prompt 模板**:
```
japanese emakimono scroll background, sumi-e ink wash mountains and mist,
gold leaf clouds, washi paper texture, vermillion sun, edo period aesthetic,
2D game background, 1000x750 --ar 4:3 --style raw
```

### 风格选择建议

```
操作硬核、重视物理 → A (暗黑哥特) 或 B (赛博霓虹)
轻松上手、跑酷爽快 → B (赛博霓虹) 或 D (虚空极简)
学术/实验气质     → C (炼金手稿) 或 D (虚空极简)
叙事/沉浸优先     → E (生物机械) 或 F (日式绘卷)
开发资源最少      → D (虚空极简) → 甚至可以纯代码实现
```

---

## 十二、背景图方案

### 12.1 静态背景

最简单的方案：一张 PNG 作为底板，在所有游戏对象之前绘制。

**技术规格**：

| 参数 | 值 |
|------|-----|
| 分辨率 | 1000×750 (窗口尺寸) |
| 格式 | PNG (支持透明通道可选) |
| 文件大小 | < 500KB |
| 绘制方式 | `SDL_RenderCopy` 全屏拉伸 |

**代码集成**：

```cpp
// Game.h 新增
SDL_Texture* bgTexture_ = nullptr;

// Game::init()
SDL_Surface* bgSurf = SDL_LoadBMP("assets/bg.bmp");  // 或 SDL_image: IMG_Load
bgTexture_ = SDL_CreateTextureFromSurface(renderer_, bgSurf);
SDL_FreeSurface(bgSurf);

// Game::render() — 在所有内容之前
SDL_RenderCopy(renderer_, bgTexture_, nullptr, nullptr);  // 全屏
```

### 12.2 视差滚动背景

多层背景以不同速率移动，产生深度感。钟摆的摆动带动背景微微移动。

```
层3 (最远): 星空/远景  — 移动速率 2%
层2 (中间): 建筑/云雾  — 移动速率 8%
层1 (最近): 前景装饰  — 移动速率 20%
层0: 游戏对象 (不参与)
```

**移动逻辑**：背景偏移量绑定到关节球位置。

```cpp
// 钟摆支点居中, 关节球偏移量驱动视差
Vec2 bgOffset = (jointPos(pivot) - pivot) * 0.02;  // 2% 跟随

SDL_Rect bgDst = {
    (int)(bgOffset.x),
    (int)(bgOffset.y),
    1000, 750
};
SDL_RenderCopy(r, bgFar_, nullptr, &bgDst);   // 远景: 2%
// 中层: 5%, 近景: 12% ...
```

### 12.3 程序化背景

不依赖外部图片，纯代码生成。适合虚空极简或赛博霓虹路线。

**网格地面（赛博风）**：

```cpp
void drawGridBackground(SDL_Renderer* r, double time) {
    SDL_SetRenderDrawColor(r, 20, 20, 40, 255);  // 底色
    SDL_RenderClear(r);

    // 透视网格
    SDL_SetRenderDrawColor(r, 40, 40, 80, 150);
    int horizon = 200;
    int step = 60;
    for (int i = 0; i < 20; i++) {
        int y = horizon + i * step;
        int alpha = (int)(150.0 * (1.0 - (double)i / 20.0));
        SDL_SetRenderDrawColor(r, 40, 40, 80, alpha);
        SDL_RenderDrawLine(r, 0, y, 1000, y);  // 水平线

        if (i % 3 == 0) {
            int y2 = horizon + (i + 3) * step;
            // 垂直线 (透视缩短)
            for (int v = 0; v < 12; v++) {
                int x = 500 + (v - 6) * (80 + i * 15);
                SDL_RenderDrawLine(r, x, y, x + (v-6)*20, y2 > 750 ? 750 : y2);
            }
        }
    }
}
```

**粒子星空（虚空风）**：

```cpp
struct Star { double x, y, bright, speed; };
std::vector<Star> stars_;

// 初始化 200 颗星
for (int i = 0; i < 200; i++) {
    stars_.push_back({
        rand() % 1000 / 1.0, rand() % 750 / 1.0,
        0.3 + (rand() % 70) / 100.0,
        0.1 + (rand() % 30) / 100.0
    });
}

// 每帧
SDL_SetRenderDrawColor(r, 8, 8, 18, 255); SDL_RenderClear(r);
for (auto& s : stars_) {
    s.bright += (rand() % 3 - 1) * 0.01;  // 闪烁
    if (s.bright < 0.2) s.bright = 0.2;
    if (s.bright > 1.0) s.bright = 1.0;
    int a = (int)(s.bright * 200);
    SDL_SetRenderDrawColor(r, 180, 200, 255, a);
    SDL_RenderDrawPoint(r, (int)s.x, (int)s.y);
}
```

### 12.4 六种风格的生图 Prompt

以下是可直接用于 Midjourney / DALL-E / Stable Diffusion 的生图 prompt。

#### A 暗黑哥特 — 3 张变体

```
# A1 地牢
dark gothic dungeon chamber, stone archways, iron chains hanging from darkness,
single beam of moonlight from above, deep purple-black shadows, dust particles in light,
2D game background, dark fantasy, 1000x750px, --ar 4:3 --style raw --v 6.1

# A2 铁匠铺
abandoned gothic forge, giant rusted pendulum mechanism, glowing embers,
wrought iron details, dark bronze and crimson tones, atmospheric dust,
2D platformer background, 1000x750px, --ar 4:3 --style raw --v 6.1

# A3 钟楼内部
inside of a gothic clocktower, massive gears and pendulums, moonlight through rose window,
cast iron and dark wood textures, deep shadows, vertical composition,
game background, 1000x750px, --ar 4:3 --style raw --v 6.1
```

#### B 赛博霓虹 — 3 张变体

```
# B1 训练网格
cyberpunk virtual training grid, neon cyan and magenta lines on black floor,
perspective grid extending to infinity, wireframe walls, glowing data streams,
Tron-like aesthetic, 2D game background, 1000x750px, --ar 4:3 --style raw --v 6.1

# B2 数据深渊
abstract digital void with flowing data rivers, pink and blue particle streams,
geometric holographic structures, dark navy background, retrowave glow,
game background synthwave, 1000x750px, --ar 4:3 --style raw --v 6.1

# B3 霓虹竞技场
neon-lit circular arena, cyan and magenta light rings, dark metallic floor panels,
holographic displays floating in background, sci-fi clean aesthetic,
2D game arena background, 1000x750px, --ar 4:3 --style raw --v 6.1
```

#### C 炼金手稿 — 3 张变体

```
# C1 星图手稿
ancient alchemical star chart, golden celestial circles on aged parchment,
copper plate engraving style, mechanical compass roses, renaissance scientific diagrams,
warm sepia tones, 2D game background, 1000x750px, --ar 4:3 --style raw --v 6.1

# C2 机械图纸
da Vinci style mechanical drawings, pendulum and gear studies on yellowed paper,
ink wash sketches, handwritten formulas in margins, golden ratio spirals,
renaissance notebook background, 1000x750px, --ar 4:3 --style raw --v 6.1

# C3 炼金实验室
alchemical laboratory table, glass vessels and copper instruments on dark wood,
mysterious glowing liquids, scattered manuscripts, candlelit scene,
warm amber and dark brown tones, game background, 1000x750px, --ar 4:3 --style raw --v 6.1
```

#### D 虚空极简 — 3 张变体

```
# D1 纯虚空
absolute void, pure black background with barely visible dark gray radial gradient from center,
single subtle vignette, no texture, minimalist zen, negative space,
game background clean, 1000x750px, --ar 4:3 --style raw --v 6.1

# D2 微网格
minimalist dark background, extremely subtle square grid lines in dark gray,
soft center glow, clean geometric, almost invisible pattern,
minimal 2D game background, 1000x750px, --ar 4:3 --style raw --v 6.1

# D3 微尘
near-black void with sparse tiny particles like distant stars,
very subtle warm gray dust motes, deep ambient darkness, atmospheric,
minimalist game background, 1000x750px, --ar 4:3 --style raw --v 6.1
```

#### E 生物机械 — 3 张变体

```
# E1 体腔
interior of a living biomechanical creature, rib-like bone arches overhead,
organic chitin walls with metallic veins, dark amber fluid pools,
H.R. Giger inspired atmosphere, 2D game background, 1000x750px, --ar 4:3 --style raw --v 6.1

# E2 肌腱森林
forest of organic sinew cables and tendon fibers, bioluminescent nodes,
metal and flesh fused architecture, dark teal and rust tones,
alien interior game background, 1000x750px, --ar 4:3 --style raw --v 6.1

# E3 骨骼巢穴
giant skeletal structure interior, vertebrae-like columns, membrane windows,
organic metal growth, dark purple and bone white palette,
biomechanical cavern, 2D game background, 1000x750px, --ar 4:3 --style raw --v 6.1
```

#### F 日式绘卷 — 3 张变体

```
# F1 云雾山峦
japanese emakimono landscape, layered misty mountains in sumi-e ink wash,
gold leaf clouds drifting, washi paper texture, subtle vermillion sun,
edo period scroll painting, 2D game background, 1000x750px, --ar 4:3 --style raw --v 6.1

# F2 月下松林
moonlit pine forest in japanese ink painting style, silver mist between trees,
gold powder stars, aged silk scroll texture, night atmosphere,
ukiyo-e game background, 1000x750px, --ar 4:3 --style raw --v 6.1

# F3 流水落花
flowing water and falling cherry blossoms, sumi-e style river,
pink petals on wind, gold-flecked paper texture, spring atmosphere,
japanese scroll painting background, 1000x750px, --ar 4:3 --style raw --v 6.1
```

---

## 十三、角色精灵方案

### 13.1 当前状态 vs 目标

```
当前: 12×12 纯色方块 (绿/黄/蓝/红, 按状态切换)
目标: 有辨识度的角色精灵, 支持方向/状态动画
```

### 13.2 技术规格

| 参数 | 值 | 说明 |
|------|-----|------|
| 精灵尺寸 | 64×64 或 96×96 | 为高清留余量, 渲染时缩放到 24~32px |
| 帧数 | 4-8 帧/动作 | 走动循环 4 帧, 跳跃 2 帧, 钩锁 2 帧 |
| 朝向 | 可水平翻转 | 用 `SDL_RenderCopyEx` 的 flip 参数 |
| 格式 | PNG RGBA | 透明背景 |
| 锚点 | 脚底中心 | 方便定位到 `pos_` |

### 13.3 精灵表布局

```
┌────────────────────────────────────────┐
│ idle0  idle1  idle2  idle3  (待机呼吸) │  ← 行1: 待机 4帧
│ walk0  walk1  walk2  walk3  (行走循环) │  ← 行2: 行走 4帧
│ jump0  jump1  .       .      (跳跃)    │  ← 行3: 跳跃 2帧
│ hook0  hook1  .       .      (钩锁)    │  ← 行4: 钩锁 2帧
│ death0 death1 death2 death3 (死亡)     │  ← 行5: 死亡 4帧
└────────────────────────────────────────┘
每帧: 64×64
总图: 256×320  (4×5 格)
```

### 13.4 代码集成

```cpp
// Player.h 新增
struct SpriteAnim {
    int frame = 0;        // 当前帧
    int frameCount = 4;   // 总帧数
    double timer = 0.0;   // 帧计时器
    double fps = 8.0;     // 帧率
};

// 更新动画帧
void updateAnim(SpriteAnim& anim, double dt, int count, double fps) {
    anim.frameCount = count;
    anim.fps = fps;
    anim.timer += dt;
    if (anim.timer > 1.0 / anim.fps) {
        anim.timer = 0.0;
        anim.frame = (anim.frame + 1) % anim.frameCount;
    }
}

// 绘制精灵
void drawSprite(SDL_Renderer* r, SDL_Texture* sheet,
                int frame, int row, int fw, int fh,
                int dx, int dy, int dw, int dh, SDL_RendererFlip flip) {
    SDL_Rect src = { frame * fw, row * fh, fw, fh };
    SDL_Rect dst = { dx - dw/2, dy - dh, dw, dh };
    SDL_RenderCopyEx(r, sheet, &src, &dst, 0.0, nullptr, flip);
}
```

### 13.5 各风格角色生图 Prompt

#### A 暗黑哥特 — 角色

```
# 角色设计
dark fantasy character sprite, hooded figure wrapped in chain links,
tattered cloak trailing, small glowing eyes under hood, lean agile body,
dark iron gray and deep crimson accents, 2D game character,
pixel art friendly design, sprite sheet, side view, on black background,
64x64 per frame, 4-frame walk cycle --ar 4:5 --style raw --v 6.1

# 变体: 更抽象
small shadow creature, humanoid silhouette made of dark smoke and embers,
trailing wispy particles, two bright white eyes, no facial features,
2D platformer character, dark fantasy, sprite design --ar 4:5 --v 6.1
```

#### B 赛博霓虹 — 角色

```
# 角色设计
cyberpunk runner character, luminous outline suit with cyan and magenta neon trim,
visor helmet showing data patterns, sleek athletic build, trailing light streak,
Tron-inspired, 2D game sprite, side view, dark background,
64x64 pixel art character sheet --ar 4:5 --style raw --v 6.1

# 变体: 纯光形态
abstract light being, humanoid form made of pure cyan neon lines and particles,
no solid body, geometric wireframe anatomy, glowing core at chest,
2D game character, synthwave aesthetic, sprite design --ar 4:5 --v 6.1
```

#### C 炼金手稿 — 角色

```
# 角色设计
ink-drawn homunculus character, copperplate engraving style, geometric symbol on chest,
rough sketch lines, parchment-colored body with dark ink outlines,
renaissance alchemical art, 2D game sprite, side view, aged paper background,
64x64 sprite sheet frame --ar 4:5 --style raw --v 6.1

# 变体: 几何符号
living alchemical symbol, circle-triangle-square construct with orbiting gold rings,
compass rose limbs, eye symbol at center, monochrome ink on parchment,
2D game character, mystical geometry, sprite design --ar 4:5 --v 6.1
```

#### D 虚空极简 — 角色

```
# 角色设计
minimalist geometric character, single white circle with trailing line,
small bright core dot, no details, pure silhouette against void,
zen aesthetic, 2D game sprite, extreme minimal, side view,
64x64 sprite sheet --ar 4:5 --style raw --v 6.1

# 变体: 光点+轨道
tiny bright white dot with orbiting smaller dots, comet-like micro trail,
pure geometric, minimalist game character, 2D platformer,
abstract sprite design --ar 4:5 --v 6.1
```

#### E 生物机械 — 角色

```
# 角色设计
biomechanical parasite character, chitin armor plates with pulsing veins,
multiple thin limbs, organic metal fusion, dark amber and blue-gray,
H.R. Giger inspired, 2D game sprite, side view, dark biome background,
64x64 creature sprite sheet --ar 4:5 --style raw --v 6.1

# 变体: 骨片生物
bone and sinew creature, exposed vertebrae segments, translucent wing membranes,
phosphorescent nodes along spine, alien insectoid, sprite design --ar 4:5 --v 6.1
```

#### F 日式绘卷 — 角色

```
# 角色设计
tiny ink-brush traveler, straw hat and flowing robes, sumi-e painting style,
trailing ink splash footsteps, minimal strokes, warm paper-toned body,
edo period wanderer, 2D game sprite, side view, washi background,
64x64 sprite sheet --ar 4:5 --style raw --v 6.1

# 变体: 樱花魂
floating spirit of cherry petals, humanoid form made of swirling pink petals
and gold dust, translucent and ethereal, japanese yokai inspired,
2D game character, sprite design --ar 4:5 --v 6.1
```

---

## 十四、其他美术资源生图描述

### 14.1 尖刺障碍 Sprite

当前是红色矩形 + 锯齿线，可替换为风格化精灵。

**技术规格**：宽度可变 (30-80px)，高度 12-28px，PNG，可九宫格拉伸。

```
# 通用生图 prompt (替换 {STYLE} 为具体风格)
{STYLE} spike trap, sharp dangerous thorns, game obstacle,
horizontal strip, 2D platformer hazard, transparent background,
game asset sprite, 200x40px --ar 5:1 --style raw --v 6.1
```

**六风格变体**：

| 风格 | 替换关键词 |
|------|-----------|
| A 暗黑哥特 | `wrought iron spikes, rusted metal thorns, gothic` |
| B 赛博霓虹 | `neon red laser spikes, holographic warning stripes` |
| C 炼金手稿 | `ink-drawn thorny vine, copper etching, botanical` |
| D 虚空极简 | `sharp white geometric triangles, minimal hazard` |
| E 生物机械 | `bone spikes, chitin thorns, organic teeth, visceral` |
| F 日式绘卷 | `sumi-e ink slash marks, sharp brush strokes` |

### 14.2 安全平台 Sprite

当前是青色矩形 + 白色顶边，2D 平台游戏的落脚点。

```
{STYLE} floating platform, solid surface to stand on,
2D game platform asset, top surface clearly defined,
transparent background, game sprite, 80x20px --ar 4:1 --style raw --v 6.1
```

### 14.3 钩锁绳索特效

当前是黄色虚线 (`SDL_RenderDrawLine` 循环)。可以替换为能量绳/锁链粒子。

```
# 能量绳 (赛博)
energy tether beam, cyan to white gradient, pulsing particles along length,
glowing connection line, sci-fi grappling hook, game effect asset,
transparent background, 300x8px --ar 30:1 --v 6.1

# 锁链 (哥特)
dark iron chain links, rusted metal, gothic dungeon chain,
2D game grappling hook chain asset, transparent background --v 6.1

# 墨线 (绘卷/手稿)
single sumi-e ink brush stroke, tapered ends, slight ink bleed,
traditional japanese calligraphy line, game effect --v 6.1
```

### 14.4 摆锤支点 Sprite

当前是 4px 小黄圆。可做成装饰性锚点。

```
{STYLE} mechanical anchor point, ceiling mount for pendulum,
ornate metal fixture, circular base plate, 2D game prop,
transparent background, 48x48px --ar 1:1 --v 6.1
```

### 14.5 速度线 / 风效

角色高速运动时出现的速度线（类似漫画效果），增强速度感。

```
# 横版速度线
manga speed lines, horizontal motion streaks, transparent center,
anime action effect, 2D game overlay, 200x100px --ar 2:1 --v 6.1

# 也可以用代码生成:
for (int i = 0; i < 8; i++) {
    int x = rand() % 200 - 100;
    int y = pos_.y - 20 + rand() % 40;
    int len = 10 + rand() % 30;
    SDL_SetRenderDrawColor(r, 255, 255, 255, 80);
    SDL_RenderDrawLine(r, x, y, x + len, y);
}
```

### 14.6 粒子 Sprite 集

统一的粒子贴图（圆点、星形、碎片），替代当前的 `SDL_RenderDrawPoint`。

```
# 粒子合集 sheet
game particle sprite sheet, soft glow dots, small sparks, tiny debris chunks,
all on one 256x256 transparent sheet, organized grid,
2D game vfx, --ar 1:1 --v 6.1
```

```
粒子sheet布局:
┌──┬──┬──┬──┐
│● │✦ │▲ │■ │  圆点/星光/三角碎片/方块碎片
├──┼──┼──┼──┤
│◆ │◈ │◇ │· │  菱形/空心菱/细菱/微点
└──┴──┴──┴──┘
每格 32×32, 总图 128×64
```

### 14.7 UI / HUD 装饰

```
# 钩锁充能槽装饰
ornate frame for UI element, small rectangular slot,
{STYLE} decorative border, game HUD element,
transparent background, 32x40px --ar 4:5 --v 6.1

# 分数背景板
small decorative panel for game score display,
{STYLE} ornamental plate, number display background,
game UI element, 80x40px --ar 2:1 --v 6.1
```

### 14.8 死亡特效

角色死亡时的爆发特效（替代当前 12 个像素粒子）。

```
# 爆发闪光
energy burst explosion, radial shockwave, white hot center fading to transparent,
2D game death effect, impact animation sprite sheet,
256x256px sprite sheet, 8 frames --ar 1:1 --v 6.1

# 六风格变体关键词:
A: dark soul escaping, crimson smoke burst, gothic death
B: digital disintegration, neon pixel shatter, glitch death
C: ink splash explosion, black ink droplets, manuscript
D: geometric dissolve, circle expanding into dots, minimal
E: organic spore burst, visceral explosion, biomechanical
F: cherry petal scatter, ink wash bloom, graceful death
```

### 14.9 球体本身 — 从代码到贴图

如果不用程序化渲染（方案一~八），而是直接用 AI 生成球体贴图：

```
# M1 关节球 (红色系)
dark red metallic orb, polished sphere with surface imperfections,
chain-link attachment point at top and bottom, heavy iron ball,
game asset, 128x128px, transparent background --ar 1:1 --v 6.1

# M2 末端球 (蓝色系)
dark blue metallic orb, polished steel sphere,
slightly smaller than M1, anchor ring on top,
game asset, 118x118px, transparent background --ar 1:1 --v 6.1

# 六风格变体核心关键词:
A gothic: wrought iron ball, rusted, blood gem
B neon:  glowing plasma orb, cyan core, glass shell
C alchemy: philosopher stone, gold-veined marble sphere
D void:   pure white circle gradient, subtle rim light
E bio:    organic egg sac, chitin sphere, pulsing veins
F japanese: lacquered wood ball, gold leaf maki-e pattern
```

---

## 十五、资产目录结构

统一管理所有美术资源：

```
Game/
├── assets/
│   ├── background/
│   │   ├── bg_far.png          # 远景 (视差层3)
│   │   ├── bg_mid.png          # 中景 (视差层2)
│   │   └── bg_near.png         # 近景 (视差层1)
│   ├── sprites/
│   │   ├── player_sheet.png    # 角色精灵表 (256×320)
│   │   ├── ball_m1.png         # M1球贴图 (128×128)
│   │   ├── ball_m2.png         # M2球贴图 (118×118)
│   │   └── pivot_anchor.png    # 支点装饰 (48×48)
│   ├── obstacles/
│   │   ├── spike.png           # 尖刺 (200×40)
│   │   └── platform.png        # 安全平台 (80×20)
│   ├── effects/
│   │   ├── rope_chain.png      # 锁链/能量绳
│   │   ├── particle_sheet.png  # 粒子合集 (128×64)
│   │   ├── death_burst.png     # 死亡爆发 (256×256)
│   │   └── ripple.png          # 波纹圆环
│   ├── ui/
│   │   ├── hook_slot.png       # 钩锁充能槽 (32×40)
│   │   └── score_panel.png     # 分数板 (80×40)
│   └── prompts/                # 生图 prompt 备份
│       ├── background.txt
│       ├── character.txt
│       └── effects.txt
```

---

## 十六、批量生图脚本

自动化生成所有变体的 prompt 列表，方便批量提交给 AI 生图工具：

```bash
#!/bin/bash
# generate_prompts.sh — 输出所有风格的所有 prompt

STYLES=("gothic" "cyber-neon" "alchemy" "void" "biomech" "emakimono")
ASSETS=("background" "character" "spike" "platform" "ball_m1" "ball_m2" "rope" "death")

echo "=== 生图 Prompt 总表 ==="
for style in "${STYLES[@]}"; do
    for asset in "${ASSETS[@]}"; do
        echo "[$style / $asset]"
        # 从对应的 prompt 文件中提取
        grep -A2 "STYLE: $style" "assets/prompts/${asset}.txt"
        echo "---"
    done
done
```

### 生图注意事项

1. **首先生成背景** — 背景确定整体色调，其他资产要配色协调
2. **同一风格所有资产用同一个 seed** — 保持画风一致性
3. **生成后手动挑选** — 每个 prompt 生成 4 张，挑最好的
4. **抠图** — 角色/尖刺/平台需要透明背景，用 `rembg` 或 PS 去除
5. **缩放到目标尺寸** — 生图分辨率通常偏大，缩到实际使用尺寸
6. **精灵表拼接** — 多帧角色需要手动或用工具拼接成 sheet

---

## 十七、更新后的实施路线

### 总览

```
Phase 0: 选定美术风格        (30 min)   → 从路线A-F中选一条
Phase 1: 球体3D渲染 + 变色   (1-2 h)    → 立即可做, 不依赖外部资源
Phase 2: 背景图              (1 h)      → 生图 + 集成
Phase 3: 球体交互反馈        (1 h)      → 手感提升
Phase 4: 角色精灵             (2 h)     → 生图 + 精灵表 + 动画
Phase 5: 场景装饰             (1 h)     → 尖刺、平台替换
Phase 6: 特效升级             (1.5 h)   → 钩锁绳、粒子、死亡
Phase 7: 高级球体效果         (按需)    → 外发光、残影、动效
```

### 各 Phase 依赖关系

```
Phase 0 (选风格)
  ├─→ Phase 1 (球体3D) — 无依赖, 随时可做
  ├─→ Phase 2 (背景)   — 依赖 Phase 0
  ├─→ Phase 4 (角色)   — 依赖 Phase 0
  │     └─→ Phase 6 (特效升级)
  ├─→ Phase 5 (场景)   — 依赖 Phase 0
  └─→ Phase 7 (高级球体) — 依赖 Phase 1
```

### 快速验证路线（最小成本看到效果）

```
选 D 虚空极简 → 代码画网格/星空背景 → 球体分环渐变 → 简单几何角色
                 (0 min, 纯代码)        (1 h)            (纯代码, 30 min)
```

这条路线不依赖任何外部图片资源，全部用 C++ 代码实现。

### 完整美术路线（最佳视觉效果）

```
选 B 赛博霓虹 → 生图背景 → 球体3D+发光 → 角色精灵 → 特效全部
                 (生图30min)  (代码2h)     (生图+集成2h)
```

---

## 附录 C：快速决策矩阵

选择困难时，回答以下问题可自动缩小范围：

| 问题 | → A哥特 | → B霓虹 | → C手稿 | → D虚空 | → E生物 | → F绘卷 |
|------|---------|---------|---------|---------|---------|---------|
| 想突出"物理模拟"的感觉？ | ✅ | — | ✅ | ✅ | — | — |
| 想突出"高速跑酷"的爽感？ | — | ✅ | — | — | — | ✅ |
| 开发时间极度有限？ | — | — | — | ✅ | — | — |
| 想展现独特艺术气质？ | ✅ | — | ✅ | — | ✅ | ✅ |
| 想要最少的生图量？ | — | ✅ | — | ✅ | — | — |
| 想要黑暗/沉重的氛围？ | ✅ | — | — | ✅ | ✅ | — |
| 想要明亮/有希望的感觉？ | — | ✅ | ✅ | — | — | ✅ |

---

## 附录 D：SDL_image 集成

如果要加载 PNG（带透明通道的精灵），需要 SDL_image 库：

```cpp
// 初始化 (Game::init)
#include <SDL_image.h>
IMG_Init(IMG_INIT_PNG);

// 加载 PNG 纹理
SDL_Texture* loadTexture(SDL_Renderer* r, const char* path) {
    SDL_Surface* s = IMG_Load(path);
    if (!s) { printf("IMG_Load %s: %s\n", path, IMG_GetError()); return nullptr; }
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_FreeSurface(s);
    return t;
}

// 关闭 (Game::shutdown)
IMG_Quit();
```

**编译**：添加 `-lSDL2_image`

---

*文档版本: 3.0 | 最后更新: 2026-07-17*
*新增: Phase 1~4 全部实施完成*

---

## 十八、实施记录 (Phase 1~4 已完成)

> 以下为实际实施的视觉效果，已全部集成到代码中。

### Phase 1: 球体3D立体渲染 + 速度变色

**已实现**:
- 预计算纹理 (`generateBallTexture`): 启动时生成灰度光照纹理 (Blinn-Phong 漫反射 + 镜面高光)
- 实时调色 (`SDL_SetTextureColorMod`): 零开销动态变色
- 速度响应变色 (`computeBallColor`): 两段插值 冷→热→白热
  - M1: 暗红(80,30,30) → 正红(255,60,60) → 白热(255,220,180)
  - M2: 暗蓝(30,40,80) → 亮蓝(140,200,255) → 白热(220,240,255)
- 心跳脉动: ±1.5% 半径缩放, sin(frame*0.05)
- 降级模式: 纹理未初始化时自动回退到分环近似

**改动文件**: `Pendulum.h`, `Pendulum.cpp`, `Game.cpp`

### Phase 2: 交互反馈特效 (核心)

**已实现 4 种反馈**:

| 反馈 | 触发时机 | 视觉表现 | 衰减 |
|------|---------|---------|------|
| `landPulse` | warpTo() 切到球面 | 球半径扩张+4px | 0.2s (rate=12) |
| `hookFlash` | hook() 命中球面 | 高光扩大+亮度+80, 白色波纹环 | 0.15s (rate=18) |
| `jumpRipple` | jump() 从球面跳离 | Y压缩8% + X拉伸保持面积 | 0.3s (rate=8) |
| `ejectGlow` | 速度甩出/角度脱落 | 球边缘白色光晕扩散 | 0.2s (rate=14) |

**触发接口**:
```cpp
pendulum.triggerLand(0/1);    // 玩家踩上球面
pendulum.triggerHookHit(0/1); // 钩锁命中球面
pendulum.triggerJumpOff(0/1); // 玩家从球面跳离
pendulum.triggerEject(0/1);   // 角色被球甩出
```

**Player.cpp 调用点**:
- `warpTo()` → triggerLand
- `hook()` → triggerHookHit
- `jump()` → triggerJumpOff
- `updateOnRod()` 速度甩出/角度脱落 → triggerEject

**改动文件**: `Pendulum.h`, `Pendulum.cpp`, `Player.h`, `Player.cpp`

### Phase 3: Bloom 外发光

**已实现**:
- 3层半透明光晕: R×1.4 (alpha=60), R×2.0 (alpha=35), R×2.5 (alpha=18)
- 动能驱动强度: `intensity = 0.1 + 0.9 * min(KE/50000, 1.0)`
- 阈值控制: intensity < 0.15 时不绘制 (省性能)
- 光晕颜色跟随速度变色结果

**改动文件**: `Pendulum.cpp`

### Phase 4: 运动残影 + 轨道火花

**已实现**:
- 球体残影: 12帧环形缓冲, 每3帧记录, 仅表面速度>150px/s时显示
- 轨道火花: |ω|>3 rad/s 时从球表面随机生成, 切向+向外初速度, 0.3~0.5s生命
- 火花颜色跟随球体当前速度色
- 碰撞波纹: 钩中球时从球心扩散白色圆环, 0.3s生命

**改动文件**: `Pendulum.h`, `Pendulum.cpp`

### 性能预算

| 效果 | 预估开销 | 备注 |
|------|---------|------|
| 预计算纹理 RenderCopy | ~0.01ms/球 | 几乎零开销 |
| 速度变色 ColorMod | ~0.001ms | 纯算术 |
| Bloom 3层 | ~0.3ms/球 | 仅高动能时 |
| 反馈更新 | ~0.01ms | 指数衰减 |
| 残影 | ~0.05ms | 仅高速时 |
| 火花粒子 | ~0.05ms | 动态数量 |
| **合计** | **~0.8ms** | **<5% 帧时间** |

### 新增数据结构

```cpp
struct BallFeedback { landPulse, hookFlash, jumpRipple, ejectGlow };
struct BallTrail { Vec2 positions[12]; int head, count; };
struct SparkParticle { Vec2 pos, vel; float life, maxLife; SDL_Color color; };
struct Ripple { Vec2 origin; double radius, life, maxLife; int ballIdx; };
```
