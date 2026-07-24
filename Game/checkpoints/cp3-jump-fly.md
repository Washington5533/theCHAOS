# Checkpoint 3 — 跳跃、飞行与杆面下滑

> 前置依赖：Checkpoint 2（Player 步行面行走）必须通过验证。

---

## 一、目标

1. 按 W 键脱手，角色进入 FLY 自由落体状态，继承步行面脚下点的瞬时速度
2. 杆面下滑：当角色站在杆上且杆面倾斜 >30° 时，角色沿杆被动下滑

这是核心玩法「被抛出去」和「位置漂移」的物理基础。

---

## 二、新增 / 修改

```
src/
├── Physics.h      (新增加: rodSlideSpeed, ballSurfaceSpeed, ballAngleFromTop)
├── Physics.cpp    (实现 3 个物理计算)
├── Player.h      (追加: jump(), updateFly(), footVel())
├── Player.cpp    (实现 FLY 状态物理 + 杆面下滑)
├── Game.h        (不变)
├── Game.cpp      (追加: W 键处理)
└── 其他文件      (不变)
```

---

## 三、速度继承物理

### 3.1 原理

角色站在步行面上时，脚下点不是静止的——pendelum 的摆动让步行面上每一点都有**瞬时速度**。脱手时，角色获得这个速度，然后做自由落体。

```
pendelum 在摆动中，你站在 m2 末端上：

  摆到最低点时 (θ₂≈0, ω₂ 最大):
    v_脚下 ≈ ω₂ × L₂ ≈ 6 × 120 ≈ 720 px/s
    → 脱手后角色像弹射一样飞出去

  摆到最高点时 (θ₂≈π, ω₂≈0):
    v_脚下 ≈ 0
    → 脱手后角色原地落下
```

### 3.2 footVel — 脚下点速度计算

```cpp
// Player.h — 新增私有方法
Vec2 footVel(const Pendulum& p, Vec2 pivot) const;
```

**各段速度公式：**

```
L1_ROD:
  点位置 = pivot + fromPolar(θ₁, t·L₁)
  该点速度 = ω₁ × t·L₁ 的切向分量
  vx = -ω₁ · t·L₁ · sin(θ₁)               ← 对时间求导
  vy =  ω₁ · t·L₁ · cos(θ₁)

M1_BALL:
  点位置 = joint + fromPolar(α, R₁)，其中 α 是球面弧上的角度
  球心 joint 本身有速度 (同 L1 末端)
  球面上点的速度 = joint速度 + ω₁ × R₁ × 球面切向
  // 简化：近似取 joint 速度 + ω₁·R₁ 的切向分量

L2_ROD:
  点位置 = joint + fromPolar(θ₂, t·L₂)
  关节 joint 有速度
  该点速度 = joint速度 + ω₂ × t·L₂ 的切向

M2_BALL:
  同 M1_BALL 逻辑，用 ω₂ 和 R₂
```

**C++ 实现：**

```cpp
Vec2 Player::footVel(const Pendulum& p, Vec2 pivot) const {
    const auto& s = p.state();
    const auto& pr = p.params();

    switch (seg_) {

    case SurfaceSeg::L1_ROD: {
        // L1 杆中心线上点速度 = ω₁ × (t·L₁) 切向
        double r = t_ * pr.L1;
        Vec2 centerVel(-s.w1 * r * sin(s.th1),
                        s.w1 * r * cos(s.th1));
        // 杆上方偏移点(ROD_OFFSET)的速度 = 中心速度 + ω₁ × OFFSET 切向
        Vec2 offsetVel(-s.w1 * ROD_OFFSET * sin(s.th1 + M_PI/2),
                        s.w1 * ROD_OFFSET * cos(s.th1 + M_PI/2));
        return centerVel + offsetVel;
    }

    case SurfaceSeg::M1_BALL: {
        // m1 球心速度 = L1 末端速度
        Vec2 jointVel(-s.w1 * pr.L1 * sin(s.th1),
                       s.w1 * pr.L1 * cos(s.th1));
        // 球面弧上的角度 α（与 footPos 保持一致）
        double a1 = atan2(-sin(s.th1), -cos(s.th1));
        double a2 = atan2(-sin(s.th2), -cos(s.th2));
        double alpha = a1 + (a2 - a1) * t_;
        // 表面切向速度（近似：ω₁ × R₁ 在切向）
        Vec2 surfaceVel(-s.w1 * pr.R1 * sin(alpha),
                         s.w1 * pr.R1 * cos(alpha));
        return jointVel + surfaceVel;
    }

    case SurfaceSeg::L2_ROD: {
        // 关节速度 (L1末端)
        Vec2 jointVel(-s.w1 * pr.L1 * sin(s.th1),
                       s.w1 * pr.L1 * cos(s.th1));
        // L2 杆中心线上点速度
        double r = t_ * pr.L2;
        Vec2 centerVel(-s.w2 * r * sin(s.th2),
                        s.w2 * r * cos(s.th2));
        // 杆上方偏移点(ROD_OFFSET)的速度
        Vec2 offsetVel(-s.w2 * ROD_OFFSET * sin(s.th2 + M_PI/2),
                        s.w2 * ROD_OFFSET * cos(s.th2 + M_PI/2));
        return jointVel + centerVel + offsetVel;
    }

    case SurfaceSeg::M2_BALL: {
        // 关节速度
        Vec2 jointVel(-s.w1 * pr.L1 * sin(s.th1),
                       s.w1 * pr.L1 * cos(s.th1));
        // L2 末端（= m2 球心）速度
        Vec2 tipVel = jointVel + Vec2(-s.w2 * pr.L2 * sin(s.th2),
                                       s.w2 * pr.L2 * cos(s.th2));
        // 球面弧上的角度（与 footPos 保持一致）
        double alpha = -s.th2 + (M_PI - s.th2 - (-s.th2)) * t_;
        Vec2 surfaceVel(-s.w2 * pr.R2 * sin(alpha),
                         s.w2 * pr.R2 * cos(alpha));
        return tipVel + surfaceVel;
    }

    }
}
```

### 3.3 Physics 新增：杆面下滑 + 球面物理

杆面下滑和球面脱落的物理计算交给 Physics（纯数学，不依赖 Player 状态）。Player 只负责调结果、判阈值。

**为什么放 Physics？** Player 不需要知道"下滑速度 = g·sin(tilt)·factor"或"球面线速度 = ω×R"——这些是物理层的实现细节。Physics 封装后，Player 只传参数、收结果。

```cpp
// Physics.h — 新增 3 个 static 方法

// 杆面下滑速度 (t单位/s)。正=向t增大方向, 负=向t减小方向, 0=不下滑
static double rodSlideSpeed(double theta, double tiltThreshold,
                            double slideFactor, double g);

// 球表面线速度 (px/s) = |ω| × R。CP5 速度甩出使用
static double ballSurfaceSpeed(double omega, double radius);

// 角色在球面上偏离正上方的角度 (rad)。0=正上方安全, π/2=侧面危险。CP5 角度脱落使用
static double ballAngleFromTop(Vec2 charPos, Vec2 ballCenter);
```

**Physics.cpp 实现：**

```cpp
double Physics::rodSlideSpeed(double theta, double tiltThreshold,
                               double slideFactor, double g) {
    double tilt = fabs(M_PI / 2 - theta);           // 杆与水平面夹角
    if (tilt <= tiltThreshold) return 0.0;           // 倾角不足, 不下滑
    double dir = (theta < M_PI / 2) ? 1.0 : -1.0;   // 向低处滑
    return dir * g * sin(tilt) * slideFactor;
}

double Physics::ballSurfaceSpeed(double omega, double radius) {
    return fabs(omega * radius);
}

double Physics::ballAngleFromTop(Vec2 charPos, Vec2 ballCenter) {
    Vec2 toChar = charPos - ballCenter;
    return fabs(atan2(toChar.x, toChar.y));
}
```

> `ballSurfaceSpeed` 和 `ballAngleFromTop` 在 CP3 定义，CP5 使用——球面脱落检测依赖它们。

### 3.4 杆面下滑 — Player 调用

Player 不再内联物理计算，只调 Physics 方法 + 判阈值：

```cpp
// Player.h — 阈值常量 (游戏设计参数, 不是物理公式)
constexpr double ROD_TILT_THRESHOLD = M_PI / 6;   // 30°
constexpr double SLIDE_FACTOR        = 0.3;        // 下滑速度系数
constexpr double GRAVITY             = 600.0;      // px/s²
```

```cpp
// Player::updateOnRod() 中，行走逻辑之后追加
void Player::updateOnRod(double dt, const Pendulum& p, Vec2 pivot) {
    // ... 原有行走逻辑（阻尼 + clamp）...

    // --- 杆面下滑 ---
    if (seg_ == SurfaceSeg::L1_ROD || seg_ == SurfaceSeg::L2_ROD) {
        const auto& s = p.state();
        double rodAngle = (seg_ == SurfaceSeg::L1_ROD) ? s.th1 : s.th2;

        double slide = Physics::rodSlideSpeed(rodAngle, ROD_TILT_THRESHOLD,
                                               SLIDE_FACTOR, GRAVITY);
        target_t_ += slide * dt;
        // slide = 0 时倾角不足不下滑
        // slide > 0 向 t 增大方向滑; slide < 0 向 t 减小方向滑
        // t 到 0 或 1 后被 clamp 挡住(球体卡住), 不跨段
    }
}
```

**下滑参数速查：**

| 参数 | 值 | 说明 |
|------|-----|------|
| ROD_TILT_THRESHOLD | 30° (π/6) | 杆与水平面夹角超过此值开始下滑 |
| SLIDE_FACTOR | 0.3 | 下滑速度系数（防止瞬移到底） |
| 触发倾角 | >30° 开始滑动 | 杆面接近水平时无滑动 |
| 下滑方向 | 向低处 | 由重力分量决定 |

> **设计意图**：杆面不甩出但会下滑，给玩家一个不同的风险——如果 pendelum 大幅摆动，你会被滑到杆的一端（掉到球上或滑到支点端）。玩家需要主动按 A/D 对抗下滑。

---

## 四、Player 修改

### 4.1 新增：状态机 + FLY 状态

```cpp
// Player.h — ★ 新增 State 枚举（CP3 引入，后续 checkpoint 扩展）
enum class State { ON_ROD, FLY };  // CP5 加 HOOKED, CP7 加 DEAD

// Player.h — ★ 新增成员
State state_ = State::ON_ROD;  // ★ 新增：状态机
Vec2 vel_;                      // ★ 新增：当前速度 (px/s)，FLY 时使用

// ★ 新增方法
Vec2 footVel(const Pendulum& p, Vec2 pivot) const;
void updateFly(double dt, const Pendulum& p, Vec2 pivot);
void updateOnRod(double dt, const Pendulum& p, Vec2 pivot);  // 从 update 拆分
```

### 4.2 jump — 脱手

```cpp
void Player::jump(const Pendulum& p, Vec2 pivot) {
    if (state_ != State::ON_ROD) return;

    // 继承脚下速度
    vel_ = footVel(p, pivot);

    // 加一个向上的初速度（W 键的垂直跳）
    vel_.y -= JUMP_BOOST;   // 向上 200 px/s

    // 切换到 FLY 状态
    state_ = State::FLY;

    // 记录当前位置作为 FLY 的起点
    pos_ = footPos(p, pivot);
}
```

### 4.3 updateFly — 自由落体物理

```cpp
void Player::updateFly(double dt, const Pendulum& p, Vec2 pivot) {
    // 重力加速度
    vel_.y += 600 * dt;   // g = 600 px/s²

    // 更新位置
    pos_ += vel_ * dt;
}
```

### 4.4 update — 分派

```cpp
void Player::update(double dt, const Pendulum& p, Vec2 pivot) {
    switch (state_) {
    case State::ON_ROD:
        updateOnRod(dt, p, pivot);   // 从 CP2 拆出来的行走 + 杆面下滑
        break;
    case State::FLY:
        updateFly(dt, p, pivot);
        break;
    }
}
```

### 4.5 绘制 — 支持 FLY 状态

```cpp
void Player::draw(SDL_Renderer* r) const {
    // 像素块
    SDL_Rect rect = {(int)pos_.x - 4, (int)pos_.y - 12, 8, 12};

    if (state_ == State::ON_ROD) {
        SDL_SetRenderDrawColor(r, 0, 255, 100, 255);  // 绿色：在地上
    } else {
        SDL_SetRenderDrawColor(r, 255, 200, 0, 255);  // 黄色：在空中
    }

    SDL_RenderFillRect(r, &rect);
}
```

---

## 五、Game 修改

### 5.1 W 键处理

```cpp
// Game.cpp — handleInput() 中追加
case SDLK_w:
    player_.jump(pendulum_, pivot_);
    break;
```

### 5.2 更新顺序

```cpp
if (!paused_) {
    pendulum_.step(dt, 8);               // 先更新 pendelum
    player_.update(dt, pendulum_, pivot_); // 再更新玩家
}
```

**为什么先 pendelum 后 player？** 因为跳跃时 player 需要读取 pendelum 的当前状态（速度继承），如果顺序反了就是读的上一帧旧数据。

---

## 六、调试辅助

### 6.1 画速度向量

```cpp
// 在 Player::draw() 中加——调试用
if (state_ == State::FLY && debugMode_) {
    double len = vel_.len();
    if (len > 10) {
        Vec2 dir = vel_ / len;
        Vec2 end = pos_ + dir * std::min(len * 0.5, 150.0);
        SDL_SetRenderDrawColor(r, 255, 255, 0, 150);
        SDL_RenderDrawLine(r, pos_.x, pos_.y, end.x, end.y);
        // 箭头
        SDL_RenderDrawLine(r, end.x, end.y,
                           end.x - dir.x*8 + dir.y*5, end.y - dir.y*8 - dir.x*5);
        SDL_RenderDrawLine(r, end.x, end.y,
                           end.x - dir.x*8 - dir.y*5, end.y - dir.y*8 + dir.x*5);
    }
}
```

### 6.2 显示速度数值

```cpp
// 临时打印
printf("FLY: pos=(%.0f,%.0f) vel=(%.0f,%.0f) |vel|=%.0f\n",
       pos_.x, pos_.y, vel_.x, vel_.y, vel_.len());
```

---

## 七、验证清单

### 7.1 功能验证

| 操作 | 预期 |
|------|------|
| 启动，按 W | 角色从步行面跳起，沿竖直方向向上（加了一点继承速度） |
| pendelum 摆到最低点时按 W | 角色被弹射出去，速度很快 |
| pendelum 摆到最高点时按 W | 角色只跳了一小段距离 |
| 在空中持续观察 | 角色做抛物线运动（水平速度不变，垂直加速下降） |
| 飞出屏幕 | 角色继续运动，不崩溃（后续 CP7 才会死亡） |
| 杆上走着时按 W 和 A/D | 继承杆上速度 + 向上跳 |

### 7.1b 杆面下滑验证

| 操作 | 预期 |
|------|------|
| 站在 L1 杆上，等 pendelum 大幅摆动 | 杆倾斜 >30° 时角色自动沿杆下滑 |
| 杆向右下方倾斜 (θ<π/2) | 角色滑向关节端（t 增加） |
| 杆向左上方倾斜 (θ>π/2) | 角色滑向支点端（t 减少） |
| 滑到杆端 (t=0 或 t=1) | 卡住，被球/支点挡住，**不跨到球上** |
| 站在 L2 杆上同理 | 沿 L2 下滑，杆端被球挡住 |
| 杆接近水平时 (θ≈π/2) | 不下滑，角色静止 |
| 按住 A/D 对抗下滑 | 可以逆着下滑方向走，但需要持续按键 |
| 站在球面上 | 不下滑（球面有独立的脱落机制，CP5 实现） |

### 7.2 速度方向验证

```cpp
在 L1_ROD.t=0.5 时跳跃:
  // 如果 pendelum 正在向右摆 (ω₁ > 0):
  vel_.x 应为正数（向右飞）
  vel_.y 应为负数（向上跳）

在 M2_BALL.t=0.5 且 pendelum 在最低点时跳跃:
  vel_.x 很大（弹射）
```

### 7.3 状态机验证

| 操作 | 状态变化 |
|------|---------|
| 按 W (在步行面上) | ON_ROD → FLY |
| 等待（FLY 中） | FLY 保持，物理自由落体 |
| （后续 CP4）落回面 | FLY → ON_ROD |

---

## 八、翻车点排查

| 现象 | 原因 | 修复 |
|------|------|------|
| 按 W 没反应 | jump() 里 state_ 判断不通过 | 检查 state_ == ON_ROD |
| 跳跃后速度为零 | footVel() 返回 (0,0) | 检查 footVel 里的 ω 值是否正确获取 |
| 跳跃方向不对 | 切向速度正负反了 | footVel 里 sin/cos 符号调换 |
| 跳跃后角色呈直线高速飞出（不做抛物线） | vel_.y 没受重力影响 | 检查 updateFly 里是否加了 `vel_.y += 600*dt` |
| 跳跃后角色瞬移到奇怪位置 | pos_ 没正确初始化 | jump() 中先 `pos_ = footPos(...)` 再切状态 |
| 按 W 时 pendelum 还在摆动但角色速度继承很小 | 跳跃发生在 pendelum 速度低点 | 等 pendelum 过最低点再跳试试 |
| FLY 中角色位置不动但 pendelum 继续摆 | updateFly 里 pos_ 没更新 | 确认 `pos_ += vel_ * dt` 被执行 |
| 杆面下滑方向反了 | slideDir 符号逻辑反了 | 调换 slideDir 的符号 |
| 杆接近水平时还在滑 | ROD_TILT_THRESHOLD 太小 | 确认阈值是 π/6 (30°) |
| 下滑太快（瞬移到底） | SLIDE_FACTOR 太大 | 从 0.3 调到 0.1 |
| 下滑太慢（感觉不到） | SLIDE_FACTOR 太小 | 从 0.3 调到 0.5 |
| 站在球面上也在滑 | 下滑检测没有限制在杆面段 | 加 `seg_ == L1_ROD || seg_ == L2_ROD` 判断 |

---

## 九、速度数值参考

| 场景 | 典型速度 (px/s) | 等价感觉 |
|------|----------------|---------|
| 站在 L1 杆中部，pendelum 普通摆动 | ~100~200 | 小跳 |
| 站在 m2 末端，pendelum 过最低点 | ~500~900 | 弹射 |
| 站在 m2 末端，pendelum 在最高点 | ~0~50 | 原地落下 |
| W 键附加垂直速度 | ~200 (向上) | 略高于跳上一个小平台 |

## 十、参数速查

```cpp
// Physics.h — 物理公式 (CP3 新增)
static double rodSlideSpeed(double theta, double tiltThreshold,
                            double slideFactor, double g);
static double ballSurfaceSpeed(double omega, double radius);
static double ballAngleFromTop(Vec2 charPos, Vec2 ballCenter);

// Player.h — 游戏参数 (CP3 新增)
constexpr double ROD_TILT_THRESHOLD   = M_PI / 6;  // 30°
constexpr double SLIDE_FACTOR         = 0.3;
constexpr double GRAVITY              = 600.0;     // px/s²
constexpr double JUMP_BOOST           = 200.0;     // W 键向上初速度 (px/s)

// Player.h — CP2 已有
constexpr double WALK_SPEED           = 2.0;
constexpr double DAMPING              = 0.85;
constexpr double ROD_OFFSET           = 5.0;
```

---

*跑通后进入 [cp4-snap-detection.md](cp4-snap-detection.md)*
