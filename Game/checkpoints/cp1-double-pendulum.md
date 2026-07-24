# Checkpoint 1 — 双摆物理

> 这份文档是 `game-design.md` 中 Checkpoint 1 的**完全展开**。代码实现时以此为唯一参考，不需要翻其他文件。

---

## 一、目标

在 SDL2 窗口中看到双摆在屏幕上摆动，验证：
1. 物理公式正确（能量守恒 < 0.1%）
2. 绘制正确（杆 + 球）
3. 交互可用（R 重置，Space 暂停，1~5 切换预设）

---

## 二、文件结构

```
C:\Users\wst\Desktop\Game\src\
├── Vec2.h        # 2D 向量
├── Physics.h     # 双摆数学（纯 static）
├── Physics.cpp   # 实现
├── Pendulum.h    # 组合 State+Params+SDL 绘制
├── Pendulum.cpp  # 实现
└── main.cpp      # SDL 入口 + 事件循环
```

---

## 三、Vec2 — 2D 向量

### 接口

```cpp
struct Vec2 {
    double x = 0, y = 0;

    Vec2() = default;
    Vec2(double x, double y);

    Vec2  operator+(Vec2 o) const;
    Vec2  operator-(Vec2 o) const;
    Vec2  operator*(double s) const;
    Vec2  operator/(double s) const;
    Vec2& operator+=(Vec2 o);

    double len() const;   // 长度 √(x²+y²)
    double len2() const;  // 平方长度（避免开方）
    Vec2 norm() const;    // 单位向量（零向量返回自身）

    // 极坐标构造: angle=0 → (0,len) 竖直向下
    //            angle=π/2 → (len,0) 水平向右
    static Vec2 fromPolar(double angle, double len);
};
```

### 关键约定

- 屏幕坐标系：**y 轴向下为正**
- `fromPolar(angle, len)` 的 angle 是**与竖直向下方向的夹角**，顺时针为正
- 这个约定与双摆 θ 的定义一致：θ=0 时摆锤在正下方，θ=π/2 时在水平右侧

### 使用示例

```cpp
Vec2 v1(3, 4);
double d = v1.len();       // 5.0

Vec2 v2 = Vec2::fromPolar(M_PI / 2, 100);
// v2 = (100, 0) → 水平向右 100px
```

---

## 四、Physics — 纯数学，所有方法 static

### 4.1 State — 物理状态

```cpp
struct State {
    double th1 = 0, th2 = 0;   // 角度 (rad)
    double w1  = 0, w2  = 0;   // 角速度 (rad/s)

    // 关节球心 = pivot + 沿 th1 方向走 L1
    Vec2 tip1(Vec2 pivot, double L1) const;

    // 末端球心 = tip1 + 沿 th2 方向走 L2
    Vec2 tip2(Vec2 pivot, double L1, double L2) const;
};
```

### 4.2 Params — 物理参数

```cpp
struct Params {
    double L1 = 150;    // 第一杆长 (px)
    double L2 = 120;    // 第二杆长 (px)
    double m1 = 2.0;    // 关节球质量
    double m2 = 1.5;    // 末端球质量
    double g  = 600;    // 重力加速度 (px/s²)
    double R1 = 15;     // 关节球绘制半径 (px)
    double R2 = 12;     // 末端球绘制半径 (px)
};
```

**关于 R₁ R₂：** 本 checkpoint 角色还没出现，球半径仅用于绘制。把半径放 Params 是因为后续 checkpoint 的碰撞检测和角色行走直接用同一个值，避免重复定义。

### 4.3 acceleration — 加速度公式

```cpp
// 给定状态和参数，计算角加速度 [a₁, a₂]
// 结果通过引用参数传出
static void acceleration(const State& s, const Params& p,
                         double& a1, double& a2);
```

#### 完整公式

```
Δ = θ₁ - θ₂
D = 2·m₁ + m₂ - m₂·cos(2·Δ)

分子₁ = -g·(2·m₁+m₂)·sin(θ₁)  -  m₂·g·sin(θ₁-2·θ₂)
       - 2·sin(Δ)·m₂·(ω₂²·L₂ + ω₁²·L₁·cos(Δ))

分子₂ = 2·sin(Δ)·( ω₁²·L₁·(m₁+m₂)
                 + g·(m₁+m₂)·cos(θ₁)
                 + ω₂²·L₂·m₂·cos(Δ) )

a₁ = 分子₁ / (L₁ · D)
a₂ = 分子₂ / (L₂ · D)
```

#### C++ 实现

```cpp
void Physics::acceleration(const State& s, const Params& p,
                           double& a1, double& a2) {
    double delta = s.th1 - s.th2;
    double denom = 2 * p.m1 + p.m2 - p.m2 * cos(2 * delta);

    double num1 = -p.g * (2 * p.m1 + p.m2) * sin(s.th1)
                  - p.m2 * p.g * sin(s.th1 - 2 * s.th2)
                  - 2 * sin(delta) * p.m2
                    * (s.w2 * s.w2 * p.L2
                       + s.w1 * s.w1 * p.L1 * cos(delta));

    double num2 = 2 * sin(delta)
                  * (s.w1 * s.w1 * p.L1 * (p.m1 + p.m2)
                     + p.g * (p.m1 + p.m2) * cos(s.th1)
                     + s.w2 * s.w2 * p.L2 * p.m2 * cos(delta));

    a1 = num1 / (p.L1 * denom);
    a2 = num2 / (p.L2 * denom);
}
```

#### 分母 D 的边界分析

```
D = 2m₁ + m₂ - m₂·cos(2Δ)

cos(2Δ) ∈ [-1, 1]
  cos(2Δ)=1 时 D = 2m₁            ← 最小
  cos(2Δ)=-1 时 D = 2m₁ + 2m₂     ← 最大

m₁=2, m₂=1.5 → D ∈ [4.0, 7.0]
```

**结论：分母永远 > 0，不需要除零保护。**

### 4.4 step — Symplectic Euler 一步

```cpp
// 用当前加速度更新速度，再用新速度更新位置
static void step(State& s, const Params& p, double dt);

void Physics::step(State& s, const Params& p, double dt) {
    double a1, a2;
    acceleration(s, p, a1, a2);

    // 关键：先速度，后位置（Symplectic Euler 的定义）
    s.w1  += a1 * dt;
    s.w2  += a2 * dt;
    s.th1 += s.w1 * dt;
    s.th2 += s.w2 * dt;

    fold(s.th1);
    fold(s.th2);
}
```

#### 为什么先速度后位置？

```
显式欧拉（错）：
  th += w * dt     ← 用旧速度更新位置
  w  += a * dt     ← 再更新速度
  → 能量持续上涨，数值发散

Symplectic Euler（对）：
  w  += a * dt     ← 先更新速度
  th += w * dt     ← 用新速度更新位置
  → 能量守恒 < 0.1%
```

### 4.5 substep — 多子步封装

```cpp
// 把 dt 拆成 n 个子步，每个子步用 step() 推进
// 推荐 n=8 在 60fps 下使用
static void substep(State& s, const Params& p,
                    double dt, int n = 8);

void Physics::substep(State& s, const Params& p,
                      double dt, int n) {
    double h = dt / n;
    for (int i = 0; i < n; i++)
        step(s, p, h);
}
```

#### 子步数对照表

| n | 子步 dt | 60fps 时总精度 | 能量漂移 | 推荐 |
|---|---------|----------------|---------|------|
| 1 | 16.7ms  | 粗 | ~5%     | ❌ |
| 4 | 4.2ms   | 中 | ~0.5%   | 勉强可用 |
| **8** | **2.1ms** | **好** | **<0.1%** | **✅ 推荐** |
| 16 | 1.0ms  | 极好 | <0.01%  | 高精度 |

### 4.6 fold — 角度标准化

```cpp
// 把角度折叠到 [-π, π] 范围内
static void fold(double& a);

void Physics::fold(double& a) {
    if (a >  M_PI) a -= 2 * M_PI;
    if (a < -M_PI) a += 2 * M_PI;
}
```

**为什么不取模？** 因为 fmod 对负数的行为在 C++ 中可能是实现定义的。而且 fold 一次调整不可能超过 2π（因为我们每步只加了一个很小的 ω·dt），所以 if 就够了，不需要 while。

### 4.7 能量计算（调试验证用）

```cpp
static double kinetic(const State& s, const Params& p);
static double potential(const State& s, const Params& p);
```

**动能：**
```
v₁² = (L₁·ω₁)²
v₂² = v₁² + (L₂·ω₂)² + 2·L₁·L₂·ω₁·ω₂·cos(θ₁-θ₂)
KE = ½·m₁·v₁² + ½·m₂·v₂²
```

**势能（y=0 在支点，y 向下为正）：**
```
y₁ = L₁·cos(θ₁)
y₂ = y₁ + L₂·cos(θ₂)
PE = m₁·g·y₁ + m₂·g·y₂
```

总能量 E = KE + PE，验证步进 1000 步后波动应 < 0.1%。

---

## 五、Pendulum — 组合 State + Params + SDL 绘制

### 5.1 类声明

```cpp
class Pendulum {
public:
    Pendulum();    // 默认 θ₁=θ₂=π/2, ω₁=ω₂=0

    // 物理
    void step(double dt, int substeps = 8);

    // 参数访问
    Physics::Params& params();
    const Physics::State& state() const;

    // 重置
    void reset();

    // 预设切换
    void setPreset(int idx);

    // 坐标查询
    Vec2 jointPos(Vec2 pivot) const;  // 关节球心
    Vec2 tipPos(Vec2 pivot) const;    // 末端球心

    // 绘制
    void draw(SDL_Renderer* r, Vec2 pivot, uint64_t frame);

private:
    Physics::State  state_;
    Physics::Params params_;
};
```

### 5.2 实现要点

#### 构造函数 & 重置

```cpp
Pendulum::Pendulum() {
    reset();
}

void Pendulum::reset() {
    state_.th1 = M_PI / 2;
    state_.th2 = M_PI / 2;
    state_.w1  = 0;
    state_.w2  = 0;
}
```

#### 预设表

```cpp
void Pendulum::setPreset(int idx) {
    static const Physics::State presets[] = {
        { M_PI/2,  M_PI/2,   0, 0 },   // 0: 对称起步
        { 2.0,     0.2,      0, 0 },   // 1: 近平衡扰动
        { 3.0,    -1.5,      0, 0 },   // 2: 大幅度卷曲
        { M_PI/2, -M_PI/2,   0, 0 },   // 3: 左右对称
        { 1.2,     1.8,      0, 0 },   // 4: 不对称卷曲
    };
    state_ = presets[idx % 5];
}
```

### 5.3 坐标查询

```cpp
Vec2 Pendulum::jointPos(Vec2 pivot) const {
    return state_.tip1(pivot, params_.L1);
}

Vec2 Pendulum::tipPos(Vec2 pivot) const {
    return state_.tip2(pivot, params_.L1, params_.L2);
}
```

### 5.4 绘制（核心功能）

```cpp
void Pendulum::draw(SDL_Renderer* r, Vec2 pivot, uint64_t frame) {
    Vec2 joint = jointPos(pivot);
    Vec2 tip   = tipPos(pivot);

    // --- 层 1: 轨迹（简化版，不依赖 Tracker 类）---
    // 这里暂时不做轨迹，等 Checkpoint 3 加入

    // --- 层 2: 杆 ---
    // L1
    SDL_SetRenderDrawColor(r, 220, 220, 240, 255);
    SDL_RenderDrawLine(r, (int)pivot.x, (int)pivot.y,
                       (int)joint.x, (int)joint.y);
    // L2
    SDL_SetRenderDrawColor(r, 180, 180, 220, 255);
    SDL_RenderDrawLine(r, (int)joint.x, (int)joint.y,
                       (int)tip.x, (int)tip.y);

    // --- 层 3: 球 ---
    // 支点
    drawFillCircle(r, (int)pivot.x, (int)pivot.y, 4, {255,255,100,255});
    // m1 关节球
    drawFillCircle(r, (int)joint.x, (int)joint.y,
                   (int)params_.R1, {255,100,100,255});
    // m2 末端球
    drawFillCircle(r, (int)tip.x, (int)tip.y,
                   (int)params_.R2, {100,200,255,255});
}
```

### 5.5 drawFillCircle — 实心圆绘制

由于 SDL2 没有原生画圆函数，用**水平扫描线法**：

```cpp
void drawFillCircle(SDL_Renderer* r, int cx, int cy,
                    int radius, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int y = -radius; y <= radius; y++) {
        int w = (int)std::sqrt(radius * radius - y * y);
        SDL_RenderDrawLine(r, cx - w, cy + y,
                           cx + w, cy + y);
    }
}
```

**为什么不是中点圆算法？** 水平扫描线法每行画一条水平线，比逐点画快得多。对于小半径（<30px），性能完全够用。

---

## 六、main.cpp — 入口 + 事件循环

### 6.1 完整结构

```cpp
#include <SDL2/SDL.h>
#include "Pendulum.h"

int main() {
    // ---- 初始化 ----
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(
        "混沌摆 - Checkpoint 1",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600, 0);

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED);

    // ---- 游戏对象 ----
    Pendulum pendulum;
    Vec2 pivot = {400, 120};   // 支点固定在屏幕上方居中
    bool running = true;
    bool paused = false;
    Uint64 lastTick = SDL_GetTicks64();

    // ---- 主循环 ----
    while (running) {
        // 1. 事件
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_r:      pendulum.reset(); break;
                    case SDLK_SPACE:  paused = !paused; break;
                    case SDLK_1:      pendulum.setPreset(0); break;
                    case SDLK_2:      pendulum.setPreset(1); break;
                    case SDLK_3:      pendulum.setPreset(2); break;
                    case SDLK_4:      pendulum.setPreset(3); break;
                    case SDLK_5:      pendulum.setPreset(4); break;
                }
            }
        }

        // 2. 时间
        Uint64 now = SDL_GetTicks64();
        double dt = (now - lastTick) / 1000.0;
        lastTick = now;
        if (dt > 0.05) dt = 0.05;  // 防螺旋

        // 3. 物理
        if (!paused)
            pendulum.step(dt, 8);

        // 4. 绘制
        SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
        SDL_RenderClear(renderer);

        pendulum.draw(renderer, pivot, now);

        SDL_RenderPresent(renderer);

        // 5. 控制帧率
        SDL_Delay(1);
    }

    // ---- 清理 ----
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

### 6.2 时间管理说明

```cpp
Uint64 now = SDL_GetTicks64();
double dt = (now - lastTick) / 1000.0;   // ms → s
lastTick = now;
if (dt > 0.05) dt = 0.05;                // cap
```

- `SDL_GetTicks64()` 返回毫秒级时间戳，精度足够
- 除以 1000 转为秒，物理学步进需要秒单位
- `cap 0.05` = 限制最大 50ms，防止窗口拖拽/断点后 dt 爆炸
- `SDL_Delay(1)` 让出 CPU 给其他进程，不加的话 CPU 会跑满 100%

---

## 七、编译

```bash
cd C:\Users\wst\Desktop\Game

g++ -std=c++17 src/main.cpp src/Pendulum.cpp src/Physics.cpp \
    -I/mingw64/include -L/mingw64/lib -lSDL2 -o game.exe

# 运行
game.exe
```

---

## 八、验证清单

### 8.1 视觉验证（每项 5 秒）

| 操作 | 预期 | 排查方向 |
|------|------|---------|
| 启动 | 红色球在关节，蓝色球在末端，两杆摆动 | step() 没调用 / dt=0 |
| 观察 10 秒 | 轨迹不重复（混沌） | 如果重复 → 检查加速度公式 |
| 按 R | 回到 90°/90° 重新开始 | reset() 没重置 w |
| 按 Space | 画面定格，物理冻结 | paused 没起效 |
| 按 1~5 | 每种初始条件产生不同的运动 | presets 表 |

### 8.2 数值验证

在 Physics::step() 中每隔 100 步打印能量：

```cpp
// 临时调试代码
static int dbg = 0;
if (++dbg >= 100) {
    dbg = 0;
    double ke = kinetic(s, p);
    double pe = potential(s, p);
    printf("E=%.2f (KE=%.2f PE=%.2f)\n", ke+pe, ke, pe);
}
```

| 输出特征 | 结论 |
|---------|------|
| E 波动 < 0.1% | ✅ 物理正确 |
| E 持续上涨 | ❌ step 顺序反了（先 th 再 w）|
| E 持续下跌 | ❌ 子步数太少或 dt 没 cap |

---

## 九、翻车点排查

| 现象 | 根因 | 修复 |
|------|------|------|
| 两个球叠在 pivot 不动 | 公式中 sin/cos 搞反 | `fromPolar(a,l)={l*sin(a), l*cos(a)}` |
| 球飞出屏幕 | L₁+L₂ 太大或 g 太大 | L=150+120=270，g=600 |
| 画圆是方框 / 条纹 | drawFillCircle 的 w 计算错误 | `w = sqrt(r² - y²)` 不能用 `y*y` 溢出 |
| 按下 R 没反应 | SDL_PollEvent 在内部循环 | 确保 e.type == SDL_KEYDOWN 判断 |
| CPU 100% | 没加 SDL_Delay | 加 `SDL_Delay(1)` |
| 暂停后恢复时 dt 很大 | 没做 cap | `if (dt > 0.05) dt = 0.05` |

---

## 十、文件清单（最终）

```
C:\Users\wst\Desktop\Game\src\
├── Vec2.h        ~25 行
├── Physics.h     ~45 行
├── Physics.cpp   ~55 行
├── Pendulum.h    ~30 行
├── Pendulum.cpp  ~75 行
└── main.cpp      ~60 行
总计              ~290 行
```

---

*这个文档就是你的实现蓝本。写代码时逐行对应，先跑通验证清单里的每一项，再进入下一个 checkpoint。*
