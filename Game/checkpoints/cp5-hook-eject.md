# Checkpoint 5 — 钩锁 + 球面脱落 ✅ 已完成

> 前置依赖：CP4 | 完成日期：2026-07-15

---

## 一、实际改动

```
Game/                          # 项目根
├── Player.h      +22行        hook/releaseHook/resolveBallCollision/surfacePosAt
│                              +hookSeg_/hookT_/hookMaxLen_/hookWorldPos_
│                              +5常量 (HOOK_MAX_RANGE/HOOK_K/HOOK_REST/...)
├── Player.cpp    +180行       匿名ns: rayHitSegment/rayHitCircle
│                              hook(): 射线检测4段→取最近命中→state=HOOKED
│                              updateHooked(): 弹性绳+绳长约束+重力+snap
│                              jump(): HOOKED分支→releaseHook()
│                              draw(): HOOKED时画黄色虚线钩绳
│                              surfacePosAt(): 参数化版footPos
│                              resolveBallCollision(): 球体碰撞(下半区弹/上半区推)
├── Game.h        +1行         Vec2 mouseWorld_
└── Game.cpp      +7行         SDL_MOUSEMOTION/SDL_MOUSEBUTTONDOWN
```

### 未创建的文件（与原始 spec 的差异）

| spec 要求 | 实际做法 | 原因 |
|----------|---------|------|
| `Input.h/cpp` | 不建 | 鼠标3行内联到Game.cpp, 键盘已在内 |
| `Physics.h/cpp` 追加射线 | 放Player.cpp匿名ns | 仅Player使用, 不污染公共接口 |
| `HookTarget` 结构体 | 不需要 | 存(seg,t)即可, 无尖刺无需区分类型 |
| `hitAABB` | 延至CP7 | 尖刺未创建 |

---

## 二、钩锁系统

### 2.1 数据流

```
鼠标左键 → Game::handleInput() → player_.hook(mouseWorld, p, pivot)
  → 归一化方向
  → 依次检测 L1杆 / L2杆 / M1球 / M2球 (取最近命中)
  → 命中: hookSeg_/hookT_/hookMaxLen_(=距×1.2) 记录, state_=HOOKED
  → 未命中: 空挥, 无效果 (CP6 加入充能消耗)

每帧 updateHooked():
  1. hookWorldPos_ = surfacePosAt(hookSeg_, hookT_, p, pivot)  // 钩点随摆锤运动
  2. 弹性拉力: F = HOOK_K × (dist - HOOK_REST) × dir (仅拉伸)
  3. 绳长约束: dist > hookMaxLen_ → 拉回 + 阻尼径向速度
  4. 重力 + 半隐式欧拉
  5. 球体碰撞检测
  6. 出屏 → DEAD
  7. dist < HOOK_SNAP_DIST → snap回ON_ROD

W键:
  ON_ROD → 原跳跃逻辑
  HOOKED → releaseHook() → FLY + vel_.y -= JUMP_BOOST
```

### 2.2 射线检测（Player.cpp 匿名 namespace）

```cpp
// 射线-线段: O + t*D = A + s*(B-A), t>=0, s∈[0,1]
bool rayHitSegment(Vec2 O, Vec2 D, Vec2 A, Vec2 B, Vec2 &hit, double &dist);

// 射线-圆: |O + t*D - C|² = R², 取最近正根
bool rayHitCircle(Vec2 O, Vec2 D, Vec2 C, double R, Vec2 &hit, double &dist);
```

### 2.3 钩点坐标

```cpp
// 存 (seg, t) 而非世界坐标 — 每帧 surfacePosAt() 重算, 钩点随摆锤自然运动
Vec2 Player::surfacePosAt(SurfaceSeg seg, double t, const Pendulum &p, Vec2 pivot) const;
// 与 footPos() 同逻辑但参数化, 4段全覆盖
```

### 2.4 杆上 t 参数计算（重要）

```cpp
// L1: t = |hp - pivot| / (L1 - R1)     — 沿杆距离 / 可行走长度
// L2: t = (|hp - joint| - R1) / (L2 - R1 - R2)  — 扣除M1球区域
// ⚠ 必须用命中点到杆起点的投影距离, 不能用射线距离 d
```

### 2.5 弹性绳绘制

HOOKED 状态下从 `pos_` 到 `hookWorldPos_` 画黄色虚线（`draw()` 内联，无独立方法）。

---

## 三、球面脱落系统

### 3.1 当前参数

| 参数 | 值 | 位置 |
|------|-----|------|
| BALL_THROW_SPEED | 200 px/s | Player.h:108 |
| 角度脱落 | t < 0.01 或 t > 0.99 | Player.cpp:246-247 |

与原 spec (R=15/12, 甩出80, 角度60°) 不同的原因：球半径改为50/40，阈值相应调大。

### 3.2 检测流程

```
updateOnRod() 第4步 — 仅球面段:
  ① 速度甩出: fabs(ω) × R > BALL_THROW_SPEED → FLY
  ② 角度脱落: t_ < 0.01 || t_ > 0.99 → FLY
  ③ 否则正常站立
```

---

## 四、球体碰撞（★ 新增，超出原 spec）

飞行中穿透球体时的响应：

```cpp
void Player::resolveBallCollision(Vec2 ballCenter, double radius) {
    if 未穿透 → return;
    推出球体表面;
    if 下半区 (dy > 0):       // 非步行面侧
        速度反射 + ×1.5 弹力;  // 硬弹开
    else:                      // 上半区 (步行面内侧)
        仅消去向内速度分量;     // 软推出, snap-back接管
}
```

在 `updateFly()` 和 `updateHooked()` 位置更新后调用，对 M1/M2 两球均检测。

---

## 五、Player.h 常量速查

```cpp
// CP5 钩锁
HOOK_MAX_RANGE  = 300.0   // 最大射程 (px)
HOOK_K          = 8.0     // 弹性系数
HOOK_REST       = 8.0     // 自然绳长 (px)
HOOK_MAX_STRETCH= 1.2     // 最大拉伸倍率
HOOK_SNAP_DIST  = 5.0     // 自动抓回距离 (px)

// CP4 脱落
BALL_THROW_SPEED= 200.0   // 速度甩出阈值 (px/s)
FLY_GRACE_FRAMES= 10      // 跳跃保护帧数

// CP3 通用
JUMP_BOOST      = 200.0   // W键上跳 (px/s)
GRAVITY         = 600.0   // (Physics.h)
```

---

## 六、验证清单

| 操作 | 预期 |
|------|------|
| FLY 状态鼠标左键点杆/球 | 钩绳出现, 角色被拉向钩点 |
| 钩中后摆锤运动 | 钩点跟随, 绳自然摆动 |
| 拉近到 <5px | snap 回 ON_ROD |
| HOOKED 中按 W | 松钩 + 上跳回 FLY |
| 空挥 (300px内无目标) | 无反应 |
| 站球面, 摆过最低点 | 速度甩出 (ω×R > 200) |
| 站球面极边缘 (t<0.01或>0.99) | 角度脱落 |
| 飞行中撞球底部 | 弹开, 速度×1.5 |
| 飞行中误入球顶内侧 | 软推出, snap-back接管 |

---

*跑通后进入 [cp6-hud-cooldown.md](cp6-hud-cooldown.md)*
