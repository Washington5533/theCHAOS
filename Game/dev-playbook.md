# 🔧 开发手册 — 快速验证与调试

> 每个 checkpoint 的编译 → 运行 → 验证全流程。目标是：**5 分钟内确认这一步正确，不把 bug 带到下一步。**

---

## 目录

- [环境准备](#环境准备)
- [Checkpoint 0 — SDL2 窗口](#checkpoint-0--sdl2-窗口)
- [Checkpoint 1 — 双摆物理](#checkpoint-1--双摆物理)
- [Checkpoint 2 — Player 步行面行走](#checkpoint-2--player-步行面行走)
- [Checkpoint 3 — Player 跳跃与飞行](#checkpoint-3--player-跳跃与飞行)
- [Checkpoint 4 — 落回步行面检测](#checkpoint-4--落回步行面检测)
- [Checkpoint 5 — 钩锁系统](#checkpoint-5--钩锁系统)
- [Checkpoint 6 — 钩锁充能与 HUD](#checkpoint-6--钩锁充能与-hud)
- [Checkpoint 7 — 尖刺与死亡](#checkpoint-7--尖刺与死亡)
- [调试技巧合集](#调试技巧合集)
- [快速排错表](#快速排错表)

---

## 环境准备

### 安装 SDL2（MinGW / MSYS2）

```bash
# 如果你的环境还没有：
pacman -S mingw-w64-x86_64-SDL2

# 验证头文件存在：
ls /mingw64/include/SDL2/SDL.h

# 验证库文件存在：
ls /mingw64/lib/libSDL2.dll.a
```

### 编译一条命令（贯穿全程用）

```bash
# 双文件（前两个 checkpoint）
g++ -std=c++17 main.cpp Pendulum.cpp -I/mingw64/include -L/mingw64/lib -lSDL2 -o game.exe

# 多文件（后续 checkpoint 逐渐追加）
g++ -std=c++17 main.cpp Game.cpp Pendulum.cpp Physics.cpp Tracker.cpp Player.cpp Stage.cpp Input.cpp -I/mingw64/include -L/mingw64/lib -lSDL2 -o game.exe
```

**建议写个 `build.bat` 放项目根目录，以后只打 `build` 就编译：**

```bat
@echo off
g++ -std=c++17 src/*.cpp -I/mingw64/include -L/mingw64/lib -lSDL2 -o game.exe
echo Build %DATE% %TIME% >> build.log
```

---

## Checkpoint 0 — SDL2 窗口

### 做什么

创建一个窗口，确认 SDL2 能正常工作。

### 代码（单文件 checkpoint_0.cpp）

```cpp
#include <SDL2/SDL.h>

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* w = SDL_CreateWindow("混沌摆", SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED, 800, 600, 0);
    SDL_Renderer* r = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        SDL_SetRenderDrawColor(r, 15, 15, 25, 255);
        SDL_RenderClear(r);
        SDL_RenderPresent(r);
    }

    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
```

### 编译 & 运行

```bash
g++ checkpoint_0.cpp -I/mingw64/include -L/mingw64/lib -lSDL2 -o c0.exe
./c0.exe
```

### ✅ 验证清单

| 验证项 | 期望结果 |
|--------|---------|
| 窗口弹出 | 800×600 深灰色窗口 |
| 按 Esc 或点 X | 窗口关闭，程序退出 |
| 窗口无闪烁 | 背景稳定深色 |

### ❌ 常见失败

| 现象 | 原因 | 修复 |
|------|------|------|
| `fatal error: SDL2/SDL.h: No such file` | SDL2 没装或少 include 路径 | `pacman -S mingw-w64-x86_64-SDL2` 或加 `-I` 参数 |
| `undefined reference to 'SDL_Init'` | 没链接 SDL2 库 | 加 `-lSDL2` |
| `cannot find -lSDL2` | 库不在标准路径 | 加 `-L/mingw64/lib` |
| 窗口打开但一片黑/花屏 | 渲染器初始化失败 | 确保 `SDL_RENDERER_ACCELERATED` 有备选，或检查显卡驱动 |

---

## Checkpoint 1 — 双摆物理

### 做什么

从单文件扩出 Vec2.h + Physics.h + Pendulum.h，看到双摆在屏幕上摆动。

### 关键文件

```
src/
├── Vec2.h        # 新建
├── Physics.h     # 新建
├── Physics.cpp   # 新建（加速度公式 + step + substep）
├── Pendulum.h    # 新建
├── Pendulum.cpp  # 新建（draw 含杆 + 摆锤）
└── main.cpp      # 修改（事件循环 + 物理步进 + 绘制）
```

### 编译

```bash
g++ -std=c++17 main.cpp Pendulum.cpp Physics.cpp -I/mingw64/include -L/mingw64/lib -lSDL2 -o game.exe
```

### ✅ 快速验证（每项 10 秒）

1. **能跑吗？**
   - 启动后看到两根杆 + 两个圆点在摆动
   - 关闭点 X 或 Esc

2. **是双摆吗？**
   - 两个摆锤各自独立运动，不是刚性的整体旋转
   - 盯 10 秒，轨迹**不重复** → 混沌成立
   - 如果轨迹完全可预测重复 → 检查是否为单摆公式还遗留

3. **能量守恒？**
   - 在 Physics::step() 里加一行日志（只调 100 步后打印一次，避免刷屏）：
     ```cpp
     static int dbg = 0;
     if (++dbg == 100) {
         double ke = kinetic(s, p), pe = potential(s, p);
         printf("E=%.2f (KE=%.2f PE=%.2f)\n", ke+pe, ke, pe);
         dbg = 0;
     }
     ```
   - 三次连续输出，总能量波动 < 0.1% → 物理正确
   - 总能量持续上涨 → substep 不够
   - 总能量持续下跌 → symplectic euler 实现反了（先更新位置再更新速度）

4. **子步参数合适？**
   - 把 substep 从 8 改为 1，看能量漂移是否显著变大（从 <0.1% 跳到 ~5%）
   - 如果 1 步就稳定 → 你的 dt 可能太小，或者公式有误

### 🔍 视觉调试技巧

```cpp
// 在 Pendulum::draw() 里——画出支点到摆锤的连线方向
SDL_SetRenderDrawColor(r, 50, 50, 80, 255);
SDL_RenderDrawLine(r, pivot.x, pivot.y, pivot.x+sin(th1)*50, pivot.y+cos(th1)*50);

// 画一个小十字标明关节位置
SDL_SetRenderDrawColor(r, 200, 200, 200, 100);
SDL_RenderDrawLine(r, joint.x-5, joint.y, joint.x+5, joint.y);
SDL_RenderDrawLine(r, joint.x, joint.y-5, joint.x, joint.y+5);
```

### ❌ 常见失败

| 现象 | 原因 | 修复 |
|------|------|------|
| 两个摆锤叠在一起不动 | sin/cos 搞反了 | 检查 `fromPolar`：x=sin, y=cos |
| 疯狂旋转 | substep 太少 | 从 1 调到 8 |
| 杆画到了屏幕上方（y 负值） | 角度不在 [-π,π] | 加 fold() |
| 摆得特别慢 | g 太小 | g = 600 起步，调大加速 |
| 摆得特别快、乱颤 | dt 太大 | 检查 `dt = min((now-last)/1000, 0.05)` |

---

## Checkpoint 2 — Player 步行面行走

### 做什么

创建 Player 类，角色出现在步行面上（杆+球表面），A/D 左右走，位置实时正确。

### 球半径参数

```cpp
// Physics.h — Params 新增
struct Params {
    double L1 = 150;
    double L2 = 120;
    double m1 = 2.0;
    double m2 = 1.5;
    double g  = 600;
    double R1 = 15;   // 关节球半径 (px)
    double R2 = 12;   // 末端球半径 (px)
};
```

### 步行面定义

角色在四段上连续行走：

```
段枚举:
  L1_ROD:   L1 杆线, t=[0,1], 0=支点, 1=关节
  M1_BALL:  m1 球面弧, t=[0,1], 0=接L1侧, 1=接L2侧
  L2_ROD:   L2 杆线, t=[0,1], 0=关节, 1=末端
  M2_BALL:  m2 球面弧, t=[0,1], 0=接L2侧, 1=末端正上方

段间过渡:
  L1_ROD.t=1 → M1_BALL.t=0
  M1_BALL.t=1 → L2_ROD.t=0
  L2_ROD.t=1 → M2_BALL.t=0
  反向同理
```

球面弧的位置计算（m1 为例）：
```
// 从 L1 方向转到 L2 方向的弧，沿球顶（反重力侧）
// 角度在球心坐标系中 lerp
```

### 关键变化

```
新增: src/Player.h  src/Player.cpp
修改: src/main.cpp (创建 Player, 传递输入, 每帧更新绘制)
```

### 编译

```bash
g++ -std=c++17 main.cpp Game.cpp Pendulum.cpp Physics.cpp Player.cpp -I/mingw64/include -L/mingw64/lib -lSDL2 -o game.exe
```

### 核心验证方法

#### 验证 1：角色跟着步行面走

在 Player 的 `footPos()` 计算结果点画一个标记：

```cpp
// 在 Game::render() 中加临时测试
Vec2 fp = player_.footPos();
SDL_Rect tr = {(int)fp.x - 2, (int)fp.y - 2, 4, 4};
SDL_SetRenderDrawColor(r, 0, 255, 100, 255);
SDL_RenderFillRect(r, &tr);
```

- 绿色方块应跟随步行面（杆面 + 球面）移动
- 走到关节附近时，方块平滑地从杆面过渡到球面
- 如果散开或乱飘 → footPos() 计算或段切换逻辑有误

#### 验证 2：角色位置正确

```
观察四段关键点:
  L1_ROD.t=0     → 角色在支点
  L1_ROD.t=1     → 角色在关节球上沿（杆与球交汇处）
  M1_BALL.t=0.5  → 角色在关节球正上方
  M1_BALL.t=1    → 角色在球另一侧，接 L2 杆
  L2_ROD.t=1     → 角色在末端球上沿
  M2_BALL.t=0.5  → 角色在末端球正上方
```

#### 验证 3：AD 移动响应

```cpp
// 在 update 里打印 (调试用)
printf("seg=%d t=%.2f\n", (int)seg_, t_);

// 快速测试序列:
// 1. 启动后 → seg=L1_ROD, t=0.5
// 2. 按 A 2 秒 → t 降到接近 0
// 3. 按 D 2 秒 → t 回到 0.5
// 4. 按住 D → t 到 1.0, 切到 M1_BALL, t 从 0 开始
// 5. 继续 D → 走完球面到 L2_ROD → 最后到 M2_BALL
```

### 🔍 特殊调试：画当前段高亮

```cpp
// 在 Pendulum::draw() 中加这段——高亮角色所在段
// 读取 Player 的 seg_ 和 t_
Vec2 fp = player_.footPos();
SDL_SetRenderDrawColor(r, 0, 255, 0, 50);
switch (seg_) {
case L1_ROD:  // 高亮 L1 杆
    SDL_RenderDrawLine(r, pivot.x, pivot.y, joint.x, joint.y); break;
case M1_BALL: // 高亮 m1 球
    drawCircle(r, joint.x, joint.y, R1, {0,255,0,50}, false); break;
case L2_ROD:  // 高亮 L2 杆
    SDL_RenderDrawLine(r, joint.x, joint.y, tip.x, tip.y); break;
case M2_BALL: // 高亮 m2 球
    drawCircle(r, tip.x, tip.y, R2, {0,255,0,50}, false); break;
}
```

---

## Checkpoint 3 — Player 跳跃与飞行

### 做什么

按 W 脱手，角色进入自由落体并继承步行面脚下点的速度。

### Player 新增

```
Player::jump() → state_ = FLY, 继承 surfaceFootVel()
Player::updateFly() → v += g*dt, pos += v*dt
Player::updateOnRod() → 角色坐标跟着步行面走

// 速度继承——对所有 4 段统一处理
Vec2 Player::surfaceFootVel(const Pendulum& p) {
    switch (seg_) {
    case L1_ROD:
        // 杆上点速度 = θ₁ 角速度 × 到支点的距离
        return ...;
    case M1_BALL: {
        // 球面上点速度 = 球心速度 + 球旋转带来的分量
        Vec2 centerVel = ...;  // 关节速度
        double angVel = ω₁;     // 球自转角速度
        double r = R₁;          // 球半径
        return centerVel + /* 切向速度分量 */;
    }
    case L2_ROD:
        return /* L2 杆上 */;
    case M2_BALL:
        return /* m2 球上 */;
    }
}
```

### ✅ 快速验证

#### 验证 1：速度继承明显可感知

```cpp
// 在 Pendulum::step() 中加入——在最低点时打印 ω₂
// 最低点 = th₂ 接近 0
if (fabs(th2) < 0.05) printf("ω₂=%.2f\n", w2);
```

测试序列：
```
1. 等 pendelum 摆到最低点附近
2. 按 W 跳跃
3. 观察角色是否被高速甩出去
4. 在最高点（th₂≈π）时再跳，应该飞得很近
```

#### 验证 2：自由落体正确

```cpp
// 跳出去后应在 console 看到
printf("v=(%.1f, %.1f) pos=(%.1f, %.1f)\n", vel_.x, vel_.y, pos_.x, pos_.y);

// 期望：
// - vel_.y 每帧增大（向下加速）
// - vel_.x 保持不变（空气阻力=0）
// - pos_ 画出一条抛物线
```

#### 验证 3：出屏幕测试

- 在最低点全力跳出 → 角色飞出画面
- 应没有任何 crash，角色位置继续更新（游戏不会崩）
- 后续（Checkpoint 7）才会处理"出屏=死亡"

### 🔍 调试辅助：画速度向量

```cpp
// 在 Player::draw() 里
if (state_ == FLY) {
    // 从角色位置画一条线表示当前速度方向和大小
    double len = vel_.len() * 0.3;  // 缩放
    SDL_SetRenderDrawColor(r, 255, 255, 0, 200);
    SDL_RenderDrawLine(r, pos_.x, pos_.y,
                         pos_.x + vel_.x * 0.3, pos_.y + vel_.y * 0.3);
}
```

---

## Checkpoint 4 — 落回步行面检测

### 做什么

角色飞出去后，当接近步行面任意一段（杆或球表面）时自动 snap 回 ON_ROD。

### 新增检测目标

球表面也纳入捕获范围：

```
对 4 段分别检测:
  L1_ROD:   点到线段(pivot→joint)距离
  M1_BALL:  点到球心距离 - R₁
  L2_ROD:   点到线段(joint→tip)距离
  M2_BALL:  点到球心距离 - R₂

取最近段，若距离 < 捕获半径(15px) 则 snap
```

### 新增函数

```cpp
bool Physics::closestRodPoint(Vec2 charPos, Vec2 pivot,
                              double L1, double L2,
                              double th1, double th2,
                              double& out_t1, double& out_t2);
// 找出杆上距离角色最近的点，返回 t1, t2 和是否在捕获范围内
```

### 实现要点

```cpp
// 采样法——对 4 段分别采样
// 先找距离杆/球表面最近的捕获点
// 球面的距离 = |p - ballCenter| - R

// 如果最近距离 < 捕获半径 (15px) → snap，并设置 seg_ 和 t_
```

### ✅ 快速验证

1. **不操作，观察 pendelum 摆动** → 角色在步行面上不动
2. **按 W 跳出** → 角色飞出去
3. **pendelum 摆回来，角色接近杆** → 角色 snap 回杆上
4. **pendelum 摆回来，角色接近球表面** → 角色 snap 到球面上（新！）
5. **重复几次** → 每次都能自动回到步行面

### ❌ 常见失败

| 现象 | 原因 |
|------|------|
| 角色穿过步行面不捕捉 | 捕获半径太小 |
| 角色 snap 到球上但被吸到球心 | 球面的 snap 没算表面偏移 |
| 角色 snap 后位置跳变 | 捕获半径内直接瞬移，正常（MVP）|

**推荐**：捕获半径 15px，检测频率每帧一次。snap 时直接瞬移。

---

## Checkpoint 5 — 钩锁系统 + 球面离心甩出

### 做什么

空中按鼠标左键发射钩爪，命中步行面（杆或球表面）后建立弹性绳连接，角色被拉回。

同时新增：站在球面上时如果球转太快，角色会被自动甩出。

### 离心甩出参数

```cpp
// Player::updateOnRod() 中在球面段追加检测
const double SPIN_EJECT_THRESHOLD = 80.0;  // px/s

if (seg_ == M1_BALL || seg_ == M2_BALL) {
    double omega = (seg_ == M1_BALL) ? ω₁ : ω₂;
    double R     = (seg_ == M1_BALL) ? R₁ : R₂;
    if (fabs(omega * R) > SPIN_EJECT_THRESHOLD) {
        // 自动跳转 FLY，保留速度
        state_ = FLY;
        vel_ = surfaceFootVel(p) + /* 切向外推 */;
    }
}
```

### ✅ 快速验证（离心甩出）

1. **站在 m2 球面上**，等 pendelum 摆到最低点附近（ω₂ 最大）→ 角色自动被甩飞
2. **站在 m1 球面上** → 同样会被甩，但阈值需更大角度（ω₁ 通常小于 ω₂）
3. **站在杆上** → 不受离心影响，只在球面上触发
4. **甩出去后继承当前速度** → 飞出方向和速度应与手动跳 W 一致或更快
5. **甩出后回到 FLY 状态** → 可以正常钩锁回来

### 手感调优参考

| 阈值 (px/s) | 等效 ω (R=12) | 玩家感觉 |
|-------------|--------------|--------|
| 60 | 5.0 rad/s | 容易甩出，频繁触发 |
| **80** | **6.7 rad/s** | **适中，只有加速时甩出** |
| 100 | 8.3 rad/s | 难触发，球转很快才能甩出 |

推荐初始值 80，玩几把后根据手感调。

### 新增

```
Player::hook(Vec2 mouseWorld) → 发射射线检测
Player::updateHooked() → 弹性绳物理
Player::draw() → 画绳子
```

### 射线检测逻辑

```cpp
// 从角色位置沿 aimDir 发射射线
// 检测目标：4 段步行面

// 线段检测（L1 杆, L2 杆）：
//   L1: pivot → joint
//   L2: joint → tip

// 球面检测（m1, m2）：
//   射线到球心距离 ≤ R

bool hitSegment(Vec2 rayOrigin, Vec2 rayDir,
                Vec2 segA, Vec2 segB,
                Vec2& hitPoint, double& hitDist) {
    // 射线-线段相交检测
}

bool hitCircle(Vec2 rayOrigin, Vec2 rayDir,
               Vec2 center, double radius,
               Vec2& hitPoint, double& hitDist) {
    // 射线-圆相交检测
    // 解 (O + t·D - C)² = R² 的二次方程
}
```

取所有命中中离发射点最近的那个作为钩锁目标。

### 弹性绳参数调优

```cpp
// 初始值（手感偏硬）
const double K_ELASTIC = 8.0;       // 弹性系数
const double REST_LENGTH = 8.0;     // 自然绳长 (px)
const double MAX_STRETCH = 1.2;     // 最大拉伸倍率

// 调试时在控制台打印
if (state_ == HOOKED) {
    double dist = (pos_ - hookPoint_).len();
    printf("hook dist=%.0f force=%.1f\n", dist, K_ELASTIC*(dist-REST_LENGTH));
}
```

### ✅ 快速验证

1. **空中按鼠标左键** → 发射一条射线（方向 = 鼠标位置 - 角色位置）
2. **射线命中杆** → 角色被弹性绳拉住
3. **绳子可见** → 从角色到钩点有一条虚线
4. **被拉回** → 角色自动向钩点靠近
5. **到达钩点** → snap 回杆上（ON_ROD）
6. **空挥** → 钩锁发射但未命中，消耗一发但没反应

### 🔍 调试辅助：画出射线

```cpp
// 在 Player::draw() 里
if (state_ == FLY && hookPending_) {
    // 高亮瞄准方向
    SDL_SetRenderDrawColor(r, 200, 200, 50, 100);
    SDL_RenderDrawLine(r, pos_.x, pos_.y,
                         pos_.x + aimDir_.x * 300, pos_.y + aimDir_.y * 300);
}
```

---

## Checkpoint 6 — 钩锁充能与 HUD

### 做什么

钩锁 2 发充能，独立 0.3s 冷却，屏幕角落显示能量格。

### 新增

```
Player::updateHookCooldowns()  → 每帧更新计时器
Player::drawHUD()              → 画能量格
```

### HUD 布局

```
┌─────────────────────────────────────────┐
│  [■ ■]    ← 屏幕左上角                 │
│  钩锁能量                               │
│                                          │
│            ◆ 支点                        │
│             ● 关节                       │
│              ● 角色                      │
│                                          │
└─────────────────────────────────────────┘
```

### 能量格绘制

```cpp
void drawHookHUD(SDL_Renderer* r) {
    int x = 20, y = 20, w = 20, h = 24, gap = 6;
    for (int i = 0; i < 2; i++) {
        bool ready = hookSlots_[i].ready;
        SDL_Color c = ready ? (SDL_Color){0, 255, 100, 255}
                            : (SDL_Color){60, 60, 60, 255};
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        SDL_Rect rect = {x + i * (w + gap), y, w, h};
        SDL_RenderFillRect(r, &rect);
        if (!ready) {
            // 冷却进度条：从底部向上填充
            float pct = hookSlots_[i].timer / 0.3f;
            SDL_SetRenderDrawColor(r, 40, 40, 60, 255);
            SDL_Rect empty = {x + i*(w+gap), y, w, (int)(h * (1-pct))};
            SDL_RenderFillRect(r, &empty);
        }
    }
}
```

### ✅ 快速验证

```cpp
// 测试序列
// 1. 启动 → 两个能量格都是亮绿色
// 2. 钩一次 → 一格变灰，开始冷却填充
// 3. 0.3s 后 → 变回亮绿色
// 4. 快速连钩两次 → 两格都灰
// 5. 第三次 → 禁止发射（钩锁无反应）
// 6. 0.3s 后 → 第一格恢复 → 可以再钩一次
```

---

## Checkpoint 7 — 尖刺与死亡

### 做什么

放置尖刺 + 屏幕边界 = 死亡 + 重置角色。

### 新增

```
Stage.h / Stage.cpp  → 尖刺容器 + 绘制
Game::checkDeaths()  → 碰撞检测
Player::die()        → 进入 DEAD 状态
```

### ✅ 快速验证

1. **碰到尖刺** → 角色闪烁 0.5s → 重置到杆上
2. **跳出屏幕** → 角色闪烁 0.5s → 重置到杆上
3. **多次死亡** → 每次都正常重置，不会卡死
4. **瞬移重置位置** → 默认 (t₁=0.5, t₂=0.5)

### 死亡闪烁实现

```cpp
void Player::drawDeath(SDL_Renderer* r, Uint64 now) {
    // 0.5s 死亡动画
    Uint64 elapsed = now - deathTimer_;
    if (elapsed > 500) {
        // 结束死亡，重置位置
        state_ = ON_ROD;
        seg_ = L1_ROD; t_ = 0.5;
        deathTimer_ = 0;
        return;
    }
    // 闪烁：每 80ms 交替显隐
    if ((elapsed / 80) % 2 == 0) {
        SDL_SetRenderDrawColor(r, 200, 50, 50, 255);
        SDL_Rect rect = {(int)pos_.x - 4, (int)pos_.y - 6, 8, 12};
        SDL_RenderFillRect(r, &rect);
    }
}
```

### 🔍 调试辅助：尖刺碰撞盒显示

```cpp
// 尖刺碰撞盒全透明全时段显示
for (auto& s : stage_.spikes()) {
    SDL_SetRenderDrawColor(r, 255, 0, 0, 80);  // 半透明红
    SDL_RenderDrawRect(r, &s.rect);             // 只画框不填充
}
```

---

## 调试技巧合集

### 1. 通杀：开一个调试标记变量

在 Game 类里：

```cpp
bool debugMode_ = true;        // 按 F3 切换

void drawDebug() {
    if (!debugMode_) return;
    // 画速度向量
    // 画碰撞盒
    // 画坐标数字
    // 打印 FPS
    // ...
}
```

### 2. 控制台日志分级

```cpp
#define DBG(fmt, ...) if (debugMode_) printf("[DBG] " fmt "\n", ##__VA_ARGS__)

// 用法
DBG("角色速度: %.1f, %.1f", vel_.x, vel_.y);
```

### 3. Pendelum 复位键

不管做到第几步，保留 `R` 键重置 pendelum 到初始状态 + 角色回到 (L1_ROD, t=0.5)。**这条贯穿全程。**

### 4. 球面调试辅助

```cpp
// 画出球的轮廓和球心，用于验证角色位置
// m1: center=joint, radius=R1
// m2: center=tip, radius=R2
// snap到球面时，角色脚底应在球表面，不是球心
```

### 5. 暂停键

保留 `Space` 暂停。暂停时绘制照常，物理不动——方便观察某一帧的精确状态。

### 5. 步进调试（单帧推进）

```cpp
if (e.key.keysym.sym == SDLK_F5) {
    singleStep_ = true;  // 下一帧物理只走一次就暂停
}
```

### 6. 视觉断言

```cpp
// 如果角色位置为 NaN，立刻在标题栏显示
if (std::isnan(pos_.x) || std::isnan(pos_.y)) {
    SDL_SetWindowTitle(window_, "!!! NaN DETECTED !!!");
}
```

### 7. 能量显示器

```cpp
// 在 HUD 区域画一个迷你能量条
double totalE = Physics::kinetic(state, params) + Physics::potential(state, params);
static double initialE = totalE;
double drift = (totalE - initialE) / initialE * 100;

// 画彩色条：
// 绿色 (<0.1%)    → 物理正确
// 黄色 (0.1~1%)   → 勉强可接受
// 红色 (>1%)      → 物理崩了
```

---

## 快速排错表

### 角色相关

| 现象 | 原因 | 诊断方法 | 修复 |
|------|------|---------|------|
| 角色不在杆上 | t₁/t₂ 或 fromPolar 错误 | 在 Pendulum::draw 里画测试点 | 对照公式检查 |
| 跳跃飞不出去 | 速度继承为 0 | 打印 vel_ 查看 | 确认 `jump()` 里赋了 vel_ |
| 速度方向不对 | 切向速度正负反了 | 画速度向量观察 | ω 符号反了，加负号 |
| 卡在 FLY 状态落不回 | 杆上检测没触发 | 打印 `closestRodPoint` 返回值 | 加大捕获半径或修正采样 |

### 钩锁相关

| 现象 | 原因 | 诊断方法 | 修复 |
|------|------|---------|------|
| 钩爪按了没反应 | 射线检测未命中 | 画射线可视化 | 检查鼠标坐标是否转换正确 |
| 钩中后角色乱飞 | 弹性系数太大 | 打印 force 值 | 减小 K_ELASTIC |
| 钩中后拉得太慢 | 弹性系数太小 | 打印 dist 值 | 增大 K_ELASTIC 或减 restLength |
| 钩中后 snap 到杆上但位置异常 | 落面检测和钩爪冲突 | 打印 snap 时的 seg/t | 检查过渡逻辑 |
| 钩锁瞄准球但命中不了 | 射线-圆相交没实现 | 画射线确认方向 | 加 hitCircle() 检测 |
| 角色 snap 到球上时位置在球心 | snap 算的是到球心距离而非表面 | 打印 snap 前后的 seg/t | snap 后计算 footPos 时应使用球表面 |
| 站在球上就立刻被甩飞 | 阈值太低 | 打印 surfaceSpeed | SPIN_EJECT_THRESHOLD 改到 80 |
| 球转很快但角色稳稳站着 | 阈值太高或没触发检测 | 确认在 updateOnRod 中有离心检测 | 加到 Player::updateOnRod() 里的球面分支 |
| 被甩出去后速度太小 | 甩出速度没加向外分量 | 打印 vel_ | 追加 outward 分量

### 球面相关

| 现象 | 原因 | 诊断方法 | 修复 |
|------|------|---------|------|
| 角色在球上位置跳动 | 弧参数计算不对 | 打印 arc angle 值 | 核实 lerp(startAngle, endAngle, t) |
| 从杆走到球上时角色位置突变 | 段间过渡没做平滑 | 打印 seg 和 t 变化 | 边界处用阻尼过渡 |
| 球画得比预期大/小 | R₁/R₂ 值不对 | 打印 R₁ R₂ | 确认 Physics::Params 设了值 |
| 钩锁射中球但挂点在球心 | hitCircle 返回的是最近点 | 核实 hitPoint 计算 | hitPoint = O + t·D 而不是球心 |

### 物理相关

| 现象 | 原因 | 诊断方法 | 修复 |
|------|------|---------|------|
| 能量持续上涨 | 积分方法不是 symplectic | 换 Verlet 或调小子步 dt | 确认 step 顺序：先 w 再 th |
| 能量持续下跌 | 子步数不够 | 把 substep 从 8 调到 16 | 改 Physics::substep 参数 |
| 摆速突然爆炸 | dt 过大 | 打印 dt 值 | 加 `min(dt, 0.05)` 限制 |

---

## 附录：checkpoint 0~7 快速启动模板

如果你直接从 checkpoint 7 开始写，这是最终的文件模板骨架：

```
src/
├── Vec2.h          # 从 checkpoint 1
├── Physics.h/cpp   # 从 checkpoint 1
├── Tracker.h/cpp   # 从 checkpoint 1（可选，尾迹）
├── Pendulum.h/cpp  # 从 checkpoint 1
├── Player.h/cpp    # 从 checkpoint 2→3→4→5→6 增量
├── Stage.h/cpp     # checkpoint 7 新增
├── Input.h/cpp     # checkpoint 4 新增
├── Game.h/cpp      # checkpoint 2 新增
└── main.cpp        # 始终不变：new Game().run()
```

每个文件的最终行数预估：

| 文件 | 最终行数 | 涉及 checkpoint |
|------|---------|----------------|
| Vec2.h | ~20 | 1 |
| Physics.h | ~50 | 1（含 R₁ R₂ 定义）|
| Physics.cpp | ~70 | 1（R₁ R₂ 用于绘制和碰撞）|
| Tracker.h | ~30 | 1 |
| Tracker.cpp | ~50 | 1 |
| Pendulum.h | ~40 | 1（含球半径绘制接口）|
| Pendulum.cpp | ~90 | 1（球以实际半径画圆）|
| Player.h | ~80 | 2~6（含 SurfaceSeg 枚举）|
| Player.cpp | ~230 | 2~6（含 4 段 footPos/footVel）|
| Stage.h | ~20 | 7 |
| Stage.cpp | ~30 | 7 |
| Input.h | ~40 | 4 |
| Input.cpp | ~60 | 4 |
| Game.h | ~40 | 2 |
| Game.cpp | ~120 | 2~7 |
| main.cpp | ~10 | 0 |
| **合计** | **~910** | |

---

*写完一个 checkpoint 就编译测试。有 bug 就在这一步修，别带到下一步。*
