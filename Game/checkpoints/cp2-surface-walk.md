# Checkpoint 2 — Player 步行面行走

> 前置依赖：Checkpoint 1（双摆物理 + 绘制）必须通过验证。

---

## 一、目标

创建 `Player` 类，角色出现在双摆的步行面上（杆 + 球表面），按 A/D 左右行走，位置随 pendelum 摆动实时更新。

---

## 二、新增 / 修改文件

```
src/
├── Vec2.h        (不变)
├── Physics.h     (不变)
├── Physics.cpp   (增加 closestPointOnSurface() 供后续使用)
├── Pendulum.h    (不变)
├── Pendulum.cpp  (不变)
├── Player.h      ★ 新建
├── Player.cpp    ★ 新建
├── Input.h       (不变，暂不使用)
├── Game.h        ★ 新建
├── Game.cpp      ★ 新建
└── main.cpp      (改写: new Game().run())
```

---

## 三、SurfaceSeg — 步行面段枚举

```cpp
// Player.h 中定义
enum class SurfaceSeg {
    L1_ROD,    // L1 杆, t∈[0,1], 0=支点端, 1=关节端
    M1_BALL,   // m1 关节球面, t∈[0,1], 0=接L1侧(顶), 1=接L2侧(顶)
    L2_ROD,    // L2 杆, t∈[0,1], 0=关节端, 1=末端端
    M2_BALL,   // m2 末端球面, t∈[0,1], 0=接L2侧(顶), 1=末端正上方
};
```

### 步行面示意图

```
  支点 ●
       │  L1 杆 (角色在线段上方, OFFSET 偏移)
  ┌─   │   ─┐
  │    │    │  ← t 增大方向
  │ ╭──●──╮ │  ← m1 球挡住杆端, t=1 卡在球边缘
  │ ╰─────╯ │
  └─   │   ─┘  ← L2 杆, t=0 从球边缘开始
       │
  ┌─   │   ─┐
  │ ╭──●──╮ │  ← m2 球挡住杆端
  │ ╰─────╯ │
  └─────────┘

四段互相隔离 —— 角色不能从杆走到球, 不能从球走到杆。
切换表面 = 钩锁(CHP5) 或 FLY 中 snap 回面(CP4)。
```

### 杆的物理模型

杆不是零厚度的数学线段。它是一个实体，角色站在杆**上方侧**（偏移 `ROD_OFFSET`）：

```
      角色 (pos_)
       │
       │ ROD_OFFSET = 5px (角色脚底到杆中心线的垂直距离)
       │
   ════●════  杆中心线
       │
       │ 下方侧 (不可站立)
```

- **上方侧**：杆的中心线 + 垂直偏移（沿 `fromPolar(θ + π/2, ROD_OFFSET)`）
- **下滑方向**：沿中心线朝向低处，不是沿偏移方向
- **球挡**：杆端 t=0 或 t=1 处，球的实体挡住，t 不能越界

```cpp
constexpr double ROD_OFFSET = 5.0;  // 脚底到杆中心线距离 (px)
```

### 四段独立行走

```
L1_ROD:  沿 L1 杆上方侧移动, t∈[0,1], 角色不能走到 m1 球上
M1_BALL: 沿 m1 球顶部弧移动, t∈[0,1], 角色不能走到杆上
L2_ROD:  沿 L2 杆上方侧移动, t∈[0,1], 角色不能走到球上
M2_BALL: 沿 m2 球顶部弧移动, t∈[0,1], 角色不能走到其他段

切换段 = 钩锁命中目标段 (CP5) 或 FLY 中 snap 到最近段 (CP4)

---

## 四、Player 类设计

### 4.1 类声明

```cpp
class Player {
public:
    Player();

    // 每帧更新
    void update(double dt, const Pendulum& p, Vec2 pivot);

    // 控制接口
    void moveLeft();
    void moveRight();

    // 状态查询
    Vec2  position() const;  // 脚底世界坐标

    // 渲染
    void draw(SDL_Renderer* r) const;

private:
    // --- 步行面定位 ---
    SurfaceSeg seg_ = SurfaceSeg::L1_ROD;
    double t_ = 0.5;       // 段内位置 [0, 1]（带阻尼的平滑值）
    double target_t_ = 0.5; // 期望位置（无阻尼）

    // --- 世界坐标 ---
    Vec2 pos_;             // 当前世界坐标（由 footPos 计算后缓存）

    // --- 内部方法 ---
    Vec2 footPos(const Pendulum& p, Vec2 pivot) const;
    void warpTo(SurfaceSeg seg, double t);  // 强制切段 (CP4/CP5 使用)

    constexpr static double WALK_SPEED = 2.0;  // 每帧 t 增量
    constexpr static double DAMPING    = 0.85; // 平滑系数
    constexpr static double ROD_OFFSET = 5.0;  // 杆面偏移 (px)
};
```

### 4.2 footPos — 脚底坐标计算（核心方法）

```cpp
Vec2 Player::footPos(const Pendulum& p, Vec2 pivot) const {
    Vec2 joint = p.jointPos(pivot);
    Vec2 tip   = p.tipPos(pivot);

    switch (seg_) {

    case SurfaceSeg::L1_ROD: {
        // 杆中心线上的点
        Vec2 center = pivot + Vec2::fromPolar(p.state().th1,
                                               p.params().L1 * t_);
        // 杆上方侧偏移 — 垂直于杆, 指向摆的外侧
        Vec2 offset = Vec2::fromPolar(p.state().th1 + M_PI / 2, ROD_OFFSET);
        return center + offset;
    }

    case SurfaceSeg::M1_BALL: {
        // m1 球面上方弧: 从接L1方向转到接L2方向
        double a1 = atan2(-sin(p.state().th1), -cos(p.state().th1));
        double a2 = atan2(-sin(p.state().th2), -cos(p.state().th2));
        double a = a1 + (a2 - a1) * t_;
        return joint + Vec2::fromPolar(a, p.params().R1);
    }

    case SurfaceSeg::L2_ROD: {
        // 杆中心线上的点
        Vec2 center = joint + Vec2::fromPolar(p.state().th2,
                                               p.params().L2 * t_);
        // 杆上方侧偏移
        Vec2 offset = Vec2::fromPolar(p.state().th2 + M_PI / 2, ROD_OFFSET);
        return center + offset;
    }

    case SurfaceSeg::M2_BALL: {
        // m2 球面: 从接L2方向转到球正上方
        double a = -p.state().th2 + (M_PI - p.state().th2 - (-p.state().th2)) * t_;
        return tip + Vec2::fromPolar(a, p.params().R2);
    }

    }
}
```

**杆面位置说明：**

```
L1_ROD.t=0  → 支点处, 杆上方侧偏移 5px
L1_ROD.t=1  → 关节球边缘, 碰球停住
M1_BALL.t=0 → 关节球顶部 (接L1侧)
M1_BALL.t=1 → 关节球顶部 (接L2侧)
L2_ROD.t=0  → 关节球边缘, 碰球停住
L2_ROD.t=1  → 末端球边缘, 碰球停住
M2_BALL.t=0 → 末端球顶部 (接L2侧)
M2_BALL.t=1 → 末端球正上方顶点
```

**为什么 M1_BALL 用 `atan2(-sin(θ), -cos(θ))` 而不是 `-θ`？**

```
atan2(-sin(θ), -cos(θ)) = θ + π（折叠到 [-π, π]）

它的含义是「杆的反方向」——从球心指向杆与球连接处的方向。
在 fromPolar 约定中，这个方向指向球的上半球（步行面所在侧）。

例如 θ=0（杆竖直下垂）：
  -θ = 0  → fromPolar(0, R) = (0, R) 指向正下方 ❌ 角色在球底部
  atan2(0,-1) = π → fromPolar(π, R) = (0, -R) 指向正上方 ✅ 角色在球顶部
```

**为什么 M2_BALL 用 `M_PI - θ₂` 而不是 `-M_PI`？**

```
-M_PI 和 M_PI 在角度空间中是同一点（正上方），但 lerp 路径不同。

θ₂=0.5 时:
  旧: lerp(-0.5, -π=-3.14, t) → 从-0.5逆时针到-3.14（走下半球 ❌）
  新: lerp(-0.5, π-0.5=2.64, t) → 从-0.5顺时针到2.64（走上半球 ✅）

M_PI - θ₂ 随 θ₂ 动态调整，确保弧总是沿着球的上方走。
```

### 4.2b 段间隔离说明

四段互不连通。角色只能在同一段内移动 t ∈ [0,1]，到达边界后被球实体挡住：

```
L1_ROD: t=0 → 支点(墙)   | t=1 → m1球挡住
M1_BALL: t=0 → 顶部(接L1) | t=1 → 顶部(接L2)
L2_ROD: t=0 → m1球挡住   | t=1 → m2球挡住
M2_BALL: t=0 → 顶部(接L2) | t=1 → 端点正上方
```

| 段 | t=0 位置 | t=1 位置 |
|----|---------|---------|
| L1_ROD | 支点处, 杆上方偏移 5px | 关节球边缘 (外侧), 球体卡住 |
| M1_BALL | 球顶部接L1侧 | 球顶部接L2侧 |
| L2_ROD | 关节球边缘 (外侧), 球体卡住 | 末端球边缘, 球体卡住 |
| M2_BALL | 球顶部接L2侧 | 球正上方顶点 |

### 4.3 update — 每帧更新

```cpp
void Player::update(double dt, const Pendulum& p, Vec2 pivot) {
    // 1. 阻尼平滑位移
    t_ += (target_t_ - t_) * DAMPING;

    // 2. clamp 到 [0, 1]，杆端被球实体挡住不移段
    //    注意：只 clamp t_，不重置 target_t_
    //    玩家可以按住 D "推" 在边界上，松手后 t_ 停在 1.0
    if (t_ < 0.0) t_ = 0.0;
    if (t_ > 1.0) t_ = 1.0;

    // 3. 缓存位置
    pos_ = footPos(p, pivot);
}
```

### 4.4 移动控制

```cpp
void Player::moveLeft()  { target_t_ -= WALK_SPEED * 1/60.0; }
void Player::moveRight() { target_t_ += WALK_SPEED * 1/60.0; }

// 段切换——仅在跳跃回面(CP4)或钩锁(CP5)中触发
void Player::warpTo(SurfaceSeg seg, double t) {
    seg_ = seg;
    t_ = target_t_ = t;
}
```

### 4.6 绘制

```cpp
void Player::draw(SDL_Renderer* r) const {
    // 8×12 像素块，脚底在 pos_ 位置
    SDL_Rect rect = {(int)pos_.x - 4, (int)pos_.y - 12, 8, 12};
    SDL_SetRenderDrawColor(r, 0, 255, 100, 255);  // 亮绿
    SDL_RenderFillRect(r, &rect);
}
```

**坐标说明：**

```
  pos_.x ─┼──┼──
          │██│   ← 8px 宽
 pos_.y ──┼██│   ← 脚底在此
          │██│
          │██│   ← 12px 高
          └──┘

像素块锚点：顶部居中在 (pos_.x, pos_.y-6)
```

---

## 五、Game 类

### 5.1 类声明

```cpp
class Game {
public:
    Game();
    ~Game();

    bool init();     // SDL_Init + 创建窗口/渲染器
    void run();      // 主循环
    void shutdown();

private:
    SDL_Window*   window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Pendulum      pendulum_;
    Player        player_;
    Vec2          pivot_ = {400, 120};
    bool          paused_ = false;
    bool          running_ = false;
    Uint64        lastTick_ = 0;

    void handleInput();
    void update(double dt);
    void render(uint64_t now);
};
```

### 5.2 init / shutdown

```cpp
bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow("混沌摆 - Checkpoint 2",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               800, 600, 0);
    if (!window_) {
        printf("Window failed: %s\n", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1,
                                   SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
        printf("Renderer failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void Game::shutdown() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_)   SDL_DestroyWindow(window_);
    SDL_Quit();
}
```

### 5.3 run — 主循环

```cpp
void Game::run() {
    running_ = true;
    lastTick_ = SDL_GetTicks64();

    while (running_) {
        // 1. 事件
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running_ = false;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running_ = false; break;
                    case SDLK_r:      pendulum_.reset(); break;
                    case SDLK_SPACE:  paused_ = !paused_; break;
                    case SDLK_a:      player_.moveLeft();  break;
                    case SDLK_d:      player_.moveRight(); break;
                    case SDLK_1: case SDLK_2: case SDLK_3:
                    case SDLK_4: case SDLK_5:
                        pendulum_.setPreset(e.key.keysym.sym - SDLK_1);
                        break;
                }
            }
        }

        // 2. 时间
        Uint64 now = SDL_GetTicks64();
        double dt = (now - lastTick_) / 1000.0;
        lastTick_ = now;
        if (dt > 0.05) dt = 0.05;

        // 3. 更新
        if (!paused_) {
            pendulum_.step(dt, 8);
            player_.update(dt, pendulum_, pivot_);
        }

        // 4. 渲染
        SDL_SetRenderDrawColor(renderer_, 15, 15, 25, 255);
        SDL_RenderClear(renderer_);

        pendulum_.draw(renderer_, pivot_, now);
        player_.draw(renderer_);

        SDL_RenderPresent(renderer_);
        SDL_Delay(1);
    }
}
```

---

## 六、main.cpp

```cpp
#include "Game.h"

int main() {
    Game game;
    if (!game.init()) return 1;
    game.run();
    game.shutdown();
    return 0;
}
```

---

## 七、编译

```bash
g++ -std=c++17 src/main.cpp src/Game.cpp src/Pendulum.cpp \
    src/Physics.cpp src/Player.cpp \
    -I/mingw64/include -L/mingw64/lib -lSDL2 -o game.exe
```

---

## 八、验证清单

### 8.1 视觉验证

| 操作 | 预期 |
|------|------|
| 启动 | 双摆摆动，亮绿色像素块站在 L1 杆上方（偏离中心线 5px） |
| 按 D | 角色向右沿 L1 杆移动，到 t=1 时卡住（被 m1 球挡住） |
| 继续按 D | 角色停在 t=1.0，**不会**走到 m1 球上 |
| 按 A | 反向走回，到 t=0 时卡在支点端 |
| 观察 | 角色始终跟随后摆结构摆动，在杆上方偏移位置 |
| 按 R | 角色回到默认位置 (L1_ROD, t=0.5) |
| 按 Space | 全画面冻结，包括角色 |

### 8.2 四段隔离验证

```
手动测试序列:
  1. 启动 → seg=L1_ROD, t≈0.5
  2. 按住 D 不放 → t 增加到 1.0 → 卡住（球挡） ✅ 不切段
  3. 按住 A 不放 → t 减少到 0.0 → 卡住（支点/球挡） ✅ 不切段
  4. (CP5 接入后) 用钩锁钩 m1 球 → seg 切到 M1_BALL ✅ 钩锁切段
  5. 在 M1_BALL 上按 D → t 在 [0,1] 内移动, 到边界卡住

关键: 走路永远不跨段；钩锁是跨段的唯一途径
```

### 8.3 杆面偏移验证

```cpp
// 在 Player::update() 临时加打印
printf("seg=%d t=%.3f center=(%.0f,%.0f) pos=(%.0f,%.0f)\n",
       (int)seg_, t_,
       centerLine.x, centerLine.y,  // 杆中心线坐标
       pos_.x, pos_.y);

// 观察: pos_ 始终偏离杆中心线 ~5px, 在杆的外侧
```

---

## 九、翻车点排查

| 现象 | 原因 | 修复 |
|------|------|------|
| 角色不在步行面上，飘在空中 | footPos() 中的 pivot/tip 坐标错了 | 检查 pendulum_.jointPos() |
| 角色站杆上但在中心线(没偏移) | L1/L2 的 footPos 没加 offset | 确认 `+ offset` 部分被执行 |
| 角色走到杆端后走到球上去了 | 旧 transitionCheck 没删除 | 确认段间过渡已移除，只 clamp |
| 角色走到杆端后继续走(飞到空中) | clamp 没生效 | 加 `if (t_>1.0) t_=target_t_=1.0` |
| A/D 反应太慢 | DAMPING 太小 | 从 0.85 调到 0.9 |
| A/D 反应太快（瞬移） | target_t_ 直接赋值，没有阻尼 | 确认实现了 t_ += (target_t_ - t_) * DAMPING |
| 按 A 但角色往反方向走 | 左右逻辑反了 | moveLeft 减 target_t_, moveRight 加 |
| 角色偏移方向在杆内侧(指向摆内) | offset 角度符号错了 | 确认 `th + M_PI/2` 方向，画偏移向量可视化 |

---

## 十、文件清单

```
src/
├── Vec2.h       ~25 行  (不变)
├── Physics.h    ~45 行  (不变)
├── Physics.cpp  ~55 行  (不变)
├── Pendulum.h   ~30 行  (不变)
├── Pendulum.cpp ~75 行  (不变)
├── Player.h     ~45 行  ★ 新增
├── Player.cpp   ~105 行  ★ 新增
├── Game.h       ~25 行  ★ 新增
├── Game.cpp     ~80 行  ★ 新增
└── main.cpp     ~10 行  (重写)
总计              ~495 行
```

---

*跑通后进入 [cp3-jump-fly.md](cp3-jump-fly.md)*
