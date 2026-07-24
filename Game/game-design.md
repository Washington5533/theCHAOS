# 🐆 混沌摆钩爪游戏 — 完整开发计划

> C++ + SDL2 | 单人 | 单场景跑酷 | 混沌双摆 + 钩锁动作

---

## 一、游戏概念

你在一个混沌双摆的杆上自由行走，利用双摆的物理动量把自己甩向目标，配合钩锁在杆和摆锤之间来回穿梭。场景中散布尖刺障碍，碰到即死。

**核心玩法循环：**

```
结构面上行走观察摆向 → 选时机脱手 → 被高速甩出 →
空中钩杆/球调整方向 → 落回结构面继续走 → ...
```

---

## 二、物理底层：混沌双摆

### 状态定义

```
State:
  θ₁, θ₂    — 角度 (rad)
  ω₁, ω₂    — 角速度 (rad/s)

Params:
  L₁  = 150   — 第一杆长 (px)
  L₂  = 120   — 第二杆长 (px)
  m₁  = 2.0   — 质量1
  m₂  = 1.5   — 质量2
  g   = 600   — 重力 (px/s²)
  R₁  = 15    — 关节球半径 (px)
  R₂  = 12    — 末端球半径 (px)
```

### 积分器

Symplectic Euler + 8 子步/帧。

**加速度方程（拉格朗日推导）：**

```
Δ = θ₁ - θ₂
D = 2m₁ + m₂ - m₂·cos(2Δ)

a₁ = [ -g·(2m₁+m₂)·sinθ₁ - m₂·g·sin(θ₁-2θ₂)
       - 2·sinΔ·m₂·(ω₂²·L₂ + ω₁²·L₁·cosΔ) ] / (L₁·D)

a₂ = [ 2·sinΔ·( ω₁²·L₁·(m₁+m₂) + g·(m₁+m₂)·cosθ₁
                 + ω₂²·L₂·m₂·cosΔ ) ] / (L₂·D)
```

### 世界坐标计算

```
支点:    pivot  = (400, 120)     // 固定屏幕上方居中
杆L1矢量: v1     = (L₁·sinθ₁, L₁·cosθ₁)
关节球心: joint  = pivot + v1
杆L2矢量: v2     = (L₂·sinθ₂, L₂·cosθ₂)
末端球心: tip    = joint + v2

球面连接处:
  L1 → m1球: 连接点为球心 joint，球以 joint 为中心半径 R₁
  L2 → m2球: 连接点为球心 tip，球以 tip 为中心半径 R₂
```

**关键变更**：杆端连接到球的**中心**，球有物理半径 R₁/R₂，角色在**表面**行走。

---

## 三、角色系统

### 3.1 行走路径概念

pendelum 的结构体是一个连续的**步行面**，由三段拼接而成：

```
支点
  │
  │  L1 杆 (线路径)
  │
  ╭───╮  ← m1 关节球 (弧路径: 从上方绕过球面)
  ╰───╯
  │
  │  L2 杆 (线路径)
  │
  ╭───╮  ← m2 末端球 (弧路径)
  ╰───╯
```

角色可以在整个表面上连续行走，走过的路径依次为：

```
L1杆(线) → m1球面(弧) → L2杆(线) → m2球面(弧)

←→ 反向走回来
```

### 3.2 路径参数化

角色位置用 **段枚举 + 段内参数 t**：

```cpp
enum SurfaceSeg {
    L1_ROD,       // L1 杆, t∈[0,1], 0=枢轴端, 1=关节端
    M1_BALL,      // m1 球面, t∈[0,1], 0=接L1侧顶, 1=接L2侧顶
    L2_ROD,       // L2 杆, t∈[0,1], 0=关节端, 1=末端
    M2_BALL,      // m2 球面, t∈[0,1], 0=接L2侧顶, 1=末端正上方
};
```

**角色脚底的世界坐标计算：**

```cpp
Vec2 footPos(const Pendulum& p, SurfaceSeg seg, double t) {
    switch (seg) {
    case L1_ROD:
        // 杆上：沿 L1 中心线走
        return pivot + Vec2::fromPolar(θ₁, t * L₁);

    case M1_BALL:
        // m1 球面：从接L1侧到接L2侧，沿球顶部弧走
        // 角度 a ∈ [90°+θ₁, 90°+θ₂] 左右
        // 简化：a 从 -θ₁ 渐变到 -θ₂（在球心坐标系中）
        // 实际上弧从杆 L1 方向转到杆 L2 方向
        {
            double a1 = atan2(-sin(θ₁), -cos(θ₁)); // L1 朝向（向上方向）
            double a2 = atan2(-sin(θ₂), -cos(θ₂)); // L2 朝向
            double a  = lerp(a1, a2, t);           // 球面上方弧
            return joint + Vec2::fromPolar(a, R₁);
        }

    case L2_ROD:
        // 杆上：沿 L2 中心线走
        return joint + Vec2::fromPolar(θ₂, t * L₂);

    case M2_BALL:
        // m2 球面：球正上方
        {
            double a = lerp(-θ₂, M_PI - θ₂, t);
            return tip + Vec2::fromPolar(a, R₂);
        }
    }
}
```

> **视觉理解**：当角色站在球上时，不要将其理解为「在球上滑」，而是理解成角色**站在球的最上沿**——球随着杆旋转，角色脚下跟着转，始终保持「站最上面」。球只是增大了角色的可站区域，让杆端有了一个稳定的过渡步行面。

### 3.4 步行与速度继承

```cpp
// 行走（当前段内移动 t）
左移 A:  当前 segment 的 t 递减
右移 D:  当前 segment 的 t 递增

// 段间过渡（t 走到边界时自动切段）
L1_ROD.t=1   → M1_BALL.t=0
M1_BALL.t=1  → L2_ROD.t=0
L2_ROD.t=1   → M2_BALL.t=0
// 反向同理

// 平滑阻尼
current_t += (target_t - current_t) * 0.85;
```

**脱手 W：角色获得脚下点的瞬时世界速度**

```
// 速度继承分两段：
// L1杆上点: v = (L₁ × t₁ × ω₁ 的切向)  +  (L₁杆整体速度)
// L2杆上点: v = (L₂ × t₂ × ω₂ 的切向)  +  (关节速度)
// 球面上点: v = 球心速度 + ω₁/ω₂ × R × 旋转分量

// 统一在 footPosVel() 中计算，不区分段
// W 键额外加一个向上的垂直初速度
v_char = footWorldVel(p, seg, t) + Vec2(0, -JUMP_BOOST);
```

### 3.5 脱落与下滑系统

角色在步行面上的稳定性受两个因素影响：**速度甩出**（球面）和**角度脱落/下滑**（球面脱落 + 杆面下滑）。两者共存，**速度甩出优先检测**。

#### 3.5.1 速度甩出（球面，优先级最高）

角色在球面上时，球自身的自转速度会产生离心力。当线速度超过阈值时，角色被自动甩出。此检测优先于角度检测。

```cpp
// 每帧检测 — 优先级最高
if (seg_ == M1_BALL || seg_ == M2_BALL) {
    double omega = (seg_ == M1_BALL) ? ω₁ : ω₂;
    double R     = (seg_ == M1_BALL) ? R₁ : R₂;
    double surfaceSpeed = fabs(omega * R);  // 球表面线速度

    if (surfaceSpeed > SPIN_EJECT_THRESHOLD) {
        // 自动甩出：状态切到 FLY
        Vec2 tangent = footWorldVel(p, seg_, t_);  // 切向
        Vec2 outward = (pos_ - ballCenter).norm() * surfaceSpeed * 0.5;  // 外向
        vel_ = tangent + outward;
        pos_ = footWorldPos(p, seg_, t_);
        state_ = FLY;
        return;  // 跳过角度检测
    }
}
```

#### 3.5.2 角度脱落（球面，速度未触发时）

速度甩出未触发时，检测球面切面角度。角色所站位置的**切面与水平面夹角 > 60°** 时，角色无法保持抓握，自动脱落。

```
切面角度定义：
  角色站在球面上某点，该点处的切面方向垂直于「球心→角色」连线
  切面与水平面夹角 = |90° - (球心→角色连线的极角)|
  
  等价判断：球心→角色连线与竖直方向夹角 < 30° 时（连线接近竖直），
  切面就接近垂直（>60°），角色脱落。
```

```cpp
// 速度未触发，检测角度脱落
if (seg_ == M1_BALL || seg_ == M2_BALL) {
    Vec2 ballCenter = (seg_ == M1_BALL) ? joint : tip;
    Vec2 toChar = pos_ - ballCenter;
    double angleFromVertical = fabs(atan2(toChar.x, toChar.y));  // 与竖直方向夹角
    
    const double SLIDE_ANGLE_THRESHOLD = M_PI / 3;  // 60° = π/3
    if (angleFromVertical > SLIDE_ANGLE_THRESHOLD) {
        // 角度过大，脱落！
        vel_ = footVel(p, pivot);  // 继承脚下速度（不含外向分量）
        state_ = FLY;
    }
}
```

> **直观理解**：站在球的正上方（顶点）最安全。当 pendelum 摆动使球倾斜，角色滑到球的侧面、切面变陡时，就会脱落。切面 > 60° 意味着你站在球的「肩膀」以下位置。

#### 3.5.3 杆面下滑（杆，不脱落）

杆面**不触发角度脱落**。当杆面倾斜过大时，角色不会飞出，而是**沿杆加速下滑**——这更符合「在杆上抓握滑动」的物理直觉。

```cpp
// 杆面下滑：在 ON_ROD 状态下自然发生
if (seg_ == L1_ROD || seg_ == L2_ROD) {
    // 杆与水平面夹角 = |π/2 - θ|（θ 是摆角，从竖直向下起算）
    // 当杆接近竖直时（θ≈0或θ≈π），杆面倾斜大 → 沿杆下滑
    
    double rodAngle = (seg_ == L1_ROD) ? s.th1 : s.th2;
    double tiltFromHorizontal = fabs(M_PI / 2 - rodAngle);  // 杆与水平面夹角
    
    if (tiltFromHorizontal > M_PI / 6) {  // 杆倾角 > 30° 即开始滑动
        // 滑动方向：沿杆朝向更低的一侧
        // θ < π/2 时杆向右下方倾斜 → t 增加（远离支点/关节）
        // θ > π/2 时杆向左上方倾斜 → t 减少（靠近支点/关节）
        double slideDir = (rodAngle < M_PI / 2) ? 1.0 : -1.0;
        double slideSpeed = GRAVITY * sin(tiltFromHorizontal) * SLIDE_FACTOR;
        target_t_ += slideDir * slideSpeed * dt;
    }
}
```

**下滑参数：**

| 参数 | 值 | 说明 |
|------|-----|------|
| SLIDE_FACTOR | 0.3 | 下滑速度系数（防止瞬移） |
| 触发倾角 | >30° 开始滑动 | 杆面接近水平时无滑动 |
| 下滑方向 | 向低处 | 由重力分量决定 |

> **设计意图**：杆面不甩出但会下滑，给玩家一个不同的风险——如果 pendelum 大幅摆动，你会被滑到杆的一端（掉到球上或滑到支点端）。这创造了一个自然的「位置漂移」机制，迫使玩家主动调整。

#### 3.5.4 参数总览

| 参数 | 值 | 说明 |
|------|-----|------|
| SPIN_EJECT_THRESHOLD | ~80 px/s | 球表面线速度超过此值即甩出（优先检测） |
| SLIDE_ANGLE_THRESHOLD | 60° (π/3) | 球面切面与水平夹角超过此值脱落 |
| SLIDE_FACTOR | 0.3 | 杆面下滑速度系数 |
| ROD_TILT_THRESHOLD | 30° (π/6) | 杆与水平面夹角超过此值开始下滑 |

#### 3.5.5 触发条件对照

| 表面 | 速度甩出 | 角度脱落 | 杆面下滑 |
|------|---------|---------|---------|
| L1_ROD | ❌ 不适用 | ❌ 不脱落 | ✅ 倾角>30°下滑 |
| M1_BALL | ✅ ω₁×R₁ > 80 | ✅ 切面>60° | ❌ 不适用 |
| L2_ROD | ❌ 不适用 | ❌ 不脱落 | ✅ 倾角>30°下滑 |
| M2_BALL | ✅ ω₂×R₂ > 80 | ✅ 切面>60° | ❌ 不适用 |

> 这个双机制让 pendelum 在不同阶段有不同的风险：摆过最低点附近（高速）→ 速度甩出；摆到竖直（低速但角度大）→ 角度脱落或杆面下滑。玩家需要综合判断「速度 + 角度」来选择跳跃时机。

### 3.6 状态机

```
                    ┌─────────────────────────────────────────┐
                    │  (杆面: 倾斜下滑 target_t_ 漂移)          │
                    │  (球面: 角度>60°脱落 / 速度>阈值甩出)      │
                    ▼                                         │
┌─────────┐  W键    ┌───────┐  钩锁键(命中任意表面) ┌──────────┐
│ ON_ROD  │ ──────→ │ FLY   │ ──────────────────→ │  HOOKED  │
│ (步行面) │ ←────── │ (自由) │ ←────────────────── │ (弹性绳) │
└──────────┘ 落回面  └───────┘   W键跳跃打断        └──────────┘
     │                   │         (附加向上速度)         │
     │  出屏/碰刺         │  出屏/碰刺                   │  出屏/碰刺
     │  速度甩出(球)      │  角度脱落(球)                │  拉到尖刺→死亡
     │                   │                             │
     └────────┬──────────┴─────────────┬───────────────┘
              │                        │
              ▼                        ▼
        ┌──────────────────────────────────┐
        │              DEAD                │
        │      (闪烁 → 重置回 ON_ROD)       │
        └──────────────────────────────────┘
```

### 3.7 状态明细

| 状态 | 行为 | 物理 | 触发 |
|------|------|------|------|
| **ON_ROD** | A/D 左右走，W 脱手；杆面倾角>30°时被动下滑 | 坐标跟随步行面；杆面下滑时 target_t_ 漂移 | 常态 / snap回面 |
| **FLY** | 自由落体，可按钩锁键（命中任意表面） | `v += g·dt`, `pos += v·dt` | W / 速度甩出 / 角度脱落 |
| **HOOKED** | 弹性绳连接钩点（任意表面），**W 跳跃打断**（附加向上速度） | 弹性拉力 + 重力 + 钩点运动 | 钩锁命中任意表面 |
| **DEAD** | 短暂闪烁效果后重置 | 无物理 | 碰刺 / 出屏 / 钩锁拉到尖刺 |

---

## 四、钩锁系统

### 4.1 瞄准机制

玩家鼠标指向决定钩锁发射方向，从角色位置发出射线检测。钩爪可命中**任意表面**：

```
① 鼠标在屏幕上的位置 → 方向向量
② 从角色位置沿方向发射检测线（最大射程 300px）
③ 按优先级检测命中：
   a. 步行面元素: L1杆 / L2杆 / m1球面 / m2球面
   b. 尖刺 (AABB 碰撞盒)
   c. 场景边界 / 地面 / 平台（如有）
④ 命中成功 → 记录挂点，进入 HOOKED
⑤ 未命中 → 消耗一发但无效果（空挥惩罚）

⚠ 钩中尖刺的风险：
  - 钩锁拉到尖刺 → 角色碰到尖刺碰撞盒 → 触发死亡
  - 玩家需要判断钩爪目标是否安全
```

### 4.1.1 射线检测扩展

```cpp
// 射线-AABB 检测（新增，用于尖刺等矩形表面）
bool hitAABB(Vec2 origin, Vec2 dir,
             SDL_Rect rect,
             Vec2& hitPoint, double& hitDist);

// 检测顺序取所有命中中距离最近者：
// 1. L1杆 (hitSegment)
// 2. m1球面 (hitCircle)
// 3. L2杆 (hitSegment)
// 4. m2球面 (hitCircle)
// 5. 尖刺AABB (hitAABB) — 新增
// 6. 屏幕边界 / 其他表面
```

### 4.2 充能与冷却

```
充能上限: 2 发
冷却时间: 每发独立计时 0.3s
UI 显示: 屏幕角落 2 个能量格

使用 → 消耗一发 → 0.3s 后恢复 → 最多存 2 发
连钩节奏: 钩→钩→0.6s等待→钩→钩→...
```

### 4.3 弹性绳物理

进入 HOOKED 后，角色和钩点之间建立弹性连接：

```
每帧:
  rope = hookPoint - charPos              // 当前绳向量
  dist = rope.len()                        // 当前绳长
  if dist > maxRopeLength:                 // 超长约束
    charPos = hookPoint - rope.norm() * maxRopeLength
  
  force = k * (dist - restLength)          // 弹性拉力
  direction = rope.norm()
  a = gravity + direction * force          // 加速度合成
  v += a * dt
  pos += v * dt                            // 但受 maxRopeLength 约束
```

**参数推荐：**

| 参数 | 值 | 效果 |
|------|-----|------|
| k (弹性系数) | 8.0 | 较硬，不拖泥带水 |
| restLength | 8px | 自然绳长远小于初始绳长，持续拉近 |
| maxRopeLength | hookPoint 到角色初始距离 × 1.2 | 限制最大拉伸 |

**钩点在运动：** hookPoint 是杆上某点，pendelum 每帧在摆，hookPoint 位置随物理更新而变化。

### 4.4 松钩 / 跳跃打断

| 操作 | 行为 |
|------|------|
| 钩爪自动拉近 | 当角色离钩点足够近（< 5px），snap 到对应表面 |
| **按 W（跳跃打断）** | 松钩，回到 FLY 状态，保留当前速度 + **附加向上初速度** `JUMP_BOOST` (200 px/s) |

> **设计意图**：W 键在 HOOKED 状态下也是「跳跃」——不仅是松钩，还附加弹跳力。这让玩家可以在钩锁拉力中借力跳起，做出「钩→拉→跳→再钩」的连招。

---

## 五、死亡与重置

### 5.1 死亡条件

```
① 角色世界坐标超出画面边界（800×600）
   └─ 宽限: 额外 50px 缓冲，超出即死
② 角色 AABB 与尖刺碰撞
   └─ 尖刺: 固定的像素块/三角区域
```

### 5.2 死亡表现

```
触发 DEAD → 角色像素块闪烁 0.5s →
角色瞬移到步行面默认位置 (seg=L1_ROD, t=0.5) →
回到 ON_ROD 状态
```

无生命数、无惩罚、无记录——纯粹 reset。

---

## 六、关卡障碍设计

### 6.1 场景布局

```
┌─────────────────────── 800 ──────────────────────┐
│                                                    │
│  ◆ 支点 (400, 120)                                │
│   \                                                │
│    \                                               │
│     ● 关节                                         │
│    / \                                             │
│   /   \                                            │
│  ◎     ◎ 角色在此                                  │
│                                                    │
│      ▲        ▲          ▲                         │
│     ████    ████       ████  ← 尖刺               │
│                                                    │
│      ── 平台(垫脚) ──                              │
│                                                    │
└─────────────────────── 600 ────────────────────────┘
```

### 6.2 球体碰撞盒披露

为照顾物理碰撞，球 m1 和 m2 除了绘图半径，还有**碰撞半径**：

| 球 | 物理半径 (px) | 碰撞半径 (px) |
|----|-------------|-------------|
| m1 (关节) | R₁ = 15 | 15 |
| m2 (末端) | R₂ = 12 | 12 |

钩锁射线检测时，球的命中判断：`点到球心距离 ≤ R + 容差(5px)`。

### 6.3 尖刺数据结构

```cpp
struct Spike {
    SDL_Rect rect;          // AABB 碰撞盒
    bool active = true;     // 可开关
};
```

尖刺放置示例（固定布局，以后可做成关卡数据加载）：

📋 **参考布局：**
- 底部地面一排（宽度 800，高度 30）
- 左侧空中一个孤立尖刺（距左边 200，高度 350）
- 右侧一个尖刺集群（距左边 500~650，高度 250~400）

### 6.3 目标点（可选）

可以在场景中放置一个闪光方块作为目标，角色碰到目标时触发胜利或记分。

```
struct Goal {
    Vec2 pos;
    SDL_Rect hitbox;
};
```

---

## 七、渲染与视觉

### 7.1 绘制顺序（从底到顶）

| 层 | 内容 | 颜色/样式 |
|----|------|----------|
| 0 | 背景 | RGB(15, 15, 25) |
| 1 | 尖刺 | RGB(255, 50, 50) 三角/锯齿 |
| 2 | pendelum 轨迹 | HSL 渐变 + 透明度衰减 |
| 3 | 杆 L1 | RGB(220, 220, 240) |
| 4 | 杆 L2 | RGB(180, 180, 220) |
| 5 | 支点/关节/摆锤 | 黄(支点) 红(m1) 蓝(m2) |
| 6 | 钩锁绳 | RGB(200, 200, 100) 虚线 |
| 7 | 角色 | ⬜ 像素块 RGB(0, 255, 100) |
| 8 | HUD: 能量格 + 死亡闪烁 | 纯白/透明 |

### 7.2 角色绘制

```cpp
// 8×12 像素块
void drawCharacter(SDL_Renderer* r, Vec2 pos, bool dead) {
    if (dead && (frame / 4) % 2 == 0) return;  // 闪烁
    SDL_Rect rect = {(int)pos.x - 4, (int)pos.y - 6, 8, 12};
    SDL_SetRenderDrawColor(r, 0, 255, 100, 255);
    SDL_RenderFillRect(r, &rect);
}
```

### 7.3 钩锁绳绘制

```cpp
void drawHookRope(SDL_Renderer* r, Vec2 from, Vec2 to) {
    // 虚线效果
    SDL_SetRenderDrawColor(r, 200, 200, 100, 180);
    for (double t = 0; t <= 1.0; t += 0.05) {
        Vec2 p = from + (to - from) * t;
        if ((int)(t * 10) % 2 == 0)  // 虚线段
            SDL_RenderDrawPoint(r, (int)p.x, (int)p.y);
    }
}
```

---

## 八、控制方案

| 操作 | 按键 | 效果 |
|------|------|------|
| 杆上左移 | A | t 递减（L1 或 L2） |
| 杆上右移 | D | t 递增（L1 或 L2） |
| 跳跃/脱手 | W | ON_ROD: 垂直向上离开杆/球，继承杆速 |
|  |  | HOOKED: **跳跃打断**，松钩 + 附加向上速度 |
| 钩锁 | 鼠标左键 | 向鼠标方向发射钩爪（最大射程 300px） |
|  |  | **可命中任意表面**：杆/球/尖刺/边界 |
| 松钩 | W (空中) | HOOKED 中按 W → 回到 FLY，附加向上速度 |
| ~~暂停~~ | Space | 可加，初期不必须 |
| ~~重置~~ | R | 重置 pendelum 到初始状态

**段间过渡：** 走到段边界 (L1_ROD.t=1 → M1_BALL.t=0 等) 自动切段，脚下位置平滑衔接。

---

## 九、代码架构

### 9.1 类一览

| 类 | 职责 | 文件 |
|------|------|------|
| `Vec2` | 2D 向量运算 | Vec2.h |
| `Physics` | 双摆拉格朗日 + Symplectic Euler | Physics.h |
| `Tracker` | 轨迹环形缓冲区 | Tracker.h |
| `Pendulum` | 组合 Phys + Tracker + 绘制 | Pendulum.h |
| `Player` | 角色 + 钩锁 + 状态机 | Player.h |
| `Stage` | 场景：尖刺 + 目标 | Stage.h |
| `Input` | 键盘 + 鼠标抽象 | Input.h |
| `Game` | 主循环 + 碰撞检测 + 组合所有子系统 | Game.h |
| `main` | 入口 | main.cpp |

### 9.2 Player 类设计

```cpp
class Player {
public:
    enum State { ON_ROD, FLY, HOOKED, DEAD };

    Player();

    // 每帧更新
    void update(double dt, const Pendulum& p, const Stage& stage);

    // 控制接口
    void moveLeft();
    void moveRight();
    void jump();
    void hook(Vec2 mouseWorld);
    void releaseHook();

    // 状态查询
    Vec2  position() const;
    State state() const;
    bool  isDead() const;

    // 渲染
    void draw(SDL_Renderer* r, uint64_t frame);

    // 钩锁能量
    int   hookCharges() const;
    float hookCooldown(int slot) const;  // 0=ready ~ 0.3=charging

private:
    State state_ = ON_ROD;
    SurfaceSeg seg_ = L1_ROD;  // 当前所在段
    double t_ = 0.5;            // 段内位置 [0,1]

    Vec2 pos_, vel_;                // FLY/HOOKED 时的位置和速度
    Vec2 hookPoint_;                // 钩锁挂点
    Vec2 aimDir_;                   // 瞄准方向

    // 钩锁冷却
    struct HookSlot {
        bool ready = true;
        float timer = 0;
    };
    HookSlot slots_[2];

    // 死亡
    Uint64 deathTimer_ = 0;

    // 内部方法
    void updateOnRod(double dt, const Pendulum& p);
    void updateFly(double dt, const Pendulum& p, const Stage& stage);
    void updateHooked(double dt, const Pendulum& p);

    Vec2 surfaceFootPos(const Pendulum& p) const;
    Vec2 surfaceFootVel(const Pendulum& p) const; // 速度继承
};
```

### 9.3 Game 类设计

```cpp
class Game {
public:
    Game();
    bool init();
    void run();
    void shutdown();

private:
    // 子系统
    Pendulum  pendulum_;
    Player    player_;
    Stage     stage_;
    Input     input_;

    // SDL
    SDL_Window*   window_;
    SDL_Renderer* renderer_;

    // 镜头/世界转换
    Vec2 pivot_ = {400, 120};  // 支点屏幕位置

    // 主循环
    Uint64 lastTick_ = 0;
    bool paused_ = false;
    bool running_ = false;

    void handleInput();
    void update(double dt);
    void render(uint64_t now);
    void checkDeaths();
};
```

---

## 十、碰撞检测

### 10.1 角色碰撞体

```cpp
// 8×12 像素块中心对齐
SDL_Rect charRect(Vec2 pos) {
    return {(int)pos.x - 4, (int)pos.y - 6, 8, 12};
}
```

### 10.2 尖刺碰撞

```cpp
bool checkSpikeCollision(Vec2 charPos, const std::vector<Spike>& spikes) {
    SDL_Rect cr = charRect(charPos);
    for (auto& s : spikes) {
        if (SDL_HasIntersection(&cr, &s.rect))
            return true;
    }
    return false;
}
```

### 10.3 落回步行面检测

角色在 FLY 状态下，如果离步行面任意一段足够近，自动 snap 回 ON_ROD：

```cpp
bool detectSnap(Vec2 charPos, const Pendulum& p, SurfaceSeg& out_seg, double& out_t) {
    // 对 4 段分别检测:
    //   L1_ROD:  点到线段(pivot→joint)距离
    //   M1_BALL: 点到球心距离 - R₁
    //   L2_ROD:  点到线段(joint→tip)距离
    //   M2_BALL: 点到球心距离 - R₂
    // 取最近段, 距离 < 15px 则返回 seg 和 t
}
```

---

## 十一、开发路线（Checkpoint）

| # | 阶段 | 新增代码 | 累计 | 可验证 |
|---|------|---------|------|--------|
| 0 | SDL2 窗口 + 清屏 | ~15 | ~15 | 看到深灰窗口 |
| 1 | 双摆物理 + 绘制 | ~120 | ~135 | 双摆混沌运动 |
| 2 | Player: 步行面行走 | ~100 | ~235 | 角色在杆+球表面走，4段过渡 |
| 3 | Player: FLY+跳跃 | ~60 | ~295 | 落体+速度继承 |
| 4 | Player: 碰撞+落回面 | ~60 | ~355 | 跳出后落回杆或球面 |
| 5 | 钩锁+离心甩出 | ~120 | ~475 | 钩杆/球+球上被甩飞 |
| 6 | 钩锁充能+冷却HUD | ~40 | ~515 | 能量格+冷却 |
| 7 | 尖刺+死亡重置 | ~50 | ~565 | 碰刺/出屏死 |

**建议：每个 checkpoint 编译测试通过后再进入下一个。**

---

## 十二、文件清单

```
C:\Users\wst\Desktop\Game\
├── src/
│   ├── Vec2.h           ~20行
│   ├── Physics.h        ~90行（含 R₁ R₂）
│   ├── Physics.cpp      ~90行（球半径绘制）
│   ├── Tracker.h        ~70行
│   ├── Tracker.cpp      ~60行
│   ├── Pendulum.h       ~70行（球半径绘制）
│   ├── Pendulum.cpp     ~110行（球以实际半径画圆）
│   ├── Player.h         ~80行（含 SurfaceSeg）
│   ├── Player.cpp       ~240行（含离心甩出）
│   ├── Stage.h          ~30行
│   ├── Stage.cpp        ~40行
│   ├── Input.h          ~60行
│   ├── Input.cpp        ~80行
│   ├── Game.h           ~30行
│   ├── Game.cpp         ~100行
│   └── main.cpp         ~10行
├── README.md
├── game-design.md       ← 这个文件
└── Makefile             (或 CMakeLists.txt)
```

**总预估：~1100 行 C++**

---

## 十三、MVP 定义

**Minimum Viable Product** 的确切边界：

| 必须做（MVP） | 可选（迭代） |
|-------------|------------|
| 双摆物理 + 绘制 | 轨迹尾迹颜色渐变 |
| 角色在步行面行走 | 像素块换成精灵图 |
| W 跳跃脱手 + 速度继承 | 音效 |
| 钩锁发射 + 弹性绳 + 离心甩出 | 镜头跟随 |
| 钩锁充能 2 发 × 0.3s 冷却 | 暂停菜单 |
| 尖刺碰撞 + 死亡重置 | 关卡列表 |
| 屏幕边界 = 死亡 | 收集物/目标点 |
| 鼠标瞄准方向 | 记分/计时 |

---

*设计完成。打开 `src/main.cpp` 写第一行。*
