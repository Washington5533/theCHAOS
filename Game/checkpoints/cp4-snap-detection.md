 
 # Checkpoint 4 — 落回步行面检测

> 前置依赖：Checkpoint 3（跳跃与飞行）必须通过验证。

---

## 一、目标

角色在 FLY 状态时，如果接近步行面任意一段（杆或球表面），自动 snap 回 ON_ROD。这是钩锁之外**唯一回到步行面的途径**。

---

## 二、新增 / 修改

```
src/
├── Physics.h      (追加: closestPointOnSurface)
├── Physics.cpp    (实现: 4 段最近点采样)
├── Player.h       (不变)
├── Player.cpp     (updateFly 中加入 snap 检测)
└── 其他文件       (不变)
```

---

## 三、核心检测算法

### 3.1 思路

在步行面的 4 段上**采样 N 个点**，找到离角色最近的一个。如果距离 < 捕获半径，snap。

```
每帧检测流程:

  1. 在 4 段上各采样 ~20 个点
  2. 计算每个采样点到角色的距离
  3. 取所有段中的最近距离
  4. 如果最近距离 < CAPTURE_RADIUS (15px):
     → snap: 设置 seg_, t_, 切换到 ON_ROD
```

### 3.2 采样函数

```cpp
// Physics.h — 新增
// 返回杆/球面上距离 pos 最近的点，以及其段和 t
struct SurfacePoint {
    SurfaceSeg seg;
    Vec2 pos;
    double t;
    double dist;
};

static SurfacePoint closestPointOnSurface(
    Vec2 pos,
    Vec2 pivot,
    double th1, double th2,
    double L1, double L2,
    double R1, double R2,
    int samplesPerSegment = 20);
```

### 3.3 实现

```cpp
SurfacePoint Physics::closestPointOnSurface(
    Vec2 pos,
    Vec2 pivot,
    double th1, double th2,
    double L1, double L2,
    double R1, double R2,
    int samplesPerSegment) {

    Vec2 joint = pivot + Vec2::fromPolar(th1, L1);
    Vec2 tip   = joint + Vec2::fromPolar(th2, L2);

    SurfacePoint best;
    best.dist = 1e9;

    // --- L1_ROD: 采样杆上方侧的点 (CP2: +ROD_OFFSET = 5px) ---
    for (int i = 0; i <= samplesPerSegment; i++) {
        double t = (double)i / samplesPerSegment;
        Vec2 center = pivot + Vec2::fromPolar(th1, L1 * t);
        Vec2 offset = Vec2::fromPolar(th1 + M_PI / 2, 5.0);  // ROD_OFFSET
        Vec2 p = center + offset;
        double d = (p - pos).len();
        if (d < best.dist) {
            best = {SurfaceSeg::L1_ROD, p, t, d};
        }
    }

    // --- M1_BALL: 采样球面弧上的点 ---
    for (int i = 0; i <= samplesPerSegment; i++) {
        double t = (double)i / samplesPerSegment;
        double a1 = atan2(-sin(th1), -cos(th1));
        double a2 = atan2(-sin(th2), -cos(th2));
        double a = a1 + (a2 - a1) * t;
        Vec2 p = joint + Vec2::fromPolar(a, R1);
        double d = (p - pos).len();
        if (d < best.dist) {
            best = {SurfaceSeg::M1_BALL, p, t, d};
        }
    }

    // --- L2_ROD: 采样杆上方侧的点 (CP2: +ROD_OFFSET = 5px) ---
    for (int i = 0; i <= samplesPerSegment; i++) {
        double t = (double)i / samplesPerSegment;
        Vec2 center = joint + Vec2::fromPolar(th2, L2 * t);
        Vec2 offset = Vec2::fromPolar(th2 + M_PI / 2, 5.0);  // ROD_OFFSET
        Vec2 p = center + offset;
        double d = (p - pos).len();
        if (d < best.dist) {
            best = {SurfaceSeg::L2_ROD, p, t, d};
        }
    }

    // --- M2_BALL: 采样球面弧上的点 ---
    for (int i = 0; i <= samplesPerSegment; i++) {
        double t = (double)i / samplesPerSegment;
        double a = -th2 + (M_PI - th2 - (-th2)) * t;
        Vec2 p = tip + Vec2::fromPolar(a, R2);
        double d = (p - pos).len();
        if (d < best.dist) {
            best = {SurfaceSeg::M2_BALL, p, t, d};
        }
    }

    return best;
}
```

### 3.4 捕获半径

```cpp
constexpr double CAPTURE_RADIUS = 15.0;  // px
```

**为什么是 15px？** 约等于 m2 球的半径。太小了角色需要精准命中才能回杆，太大了角色在远处就会被吸过来。15px 在两帧之间（~30px 位移）刚好覆盖。

---

## 四、Player 修改

### 4.1 updateFly 中加入 snap 检测

```cpp
void Player::updateFly(double dt, const Pendulum& p, Vec2 pivot) {
    // 重力
    vel_.y += 600 * dt;
    pos_ += vel_ * dt;

    // --- snap 检测 ---
    const auto& s = p.state();
    const auto& pr = p.params();

    SurfacePoint sp = Physics::closestPointOnSurface(
        pos_, pivot,
        s.th1, s.th2,
        pr.L1, pr.L2,
        pr.R1, pr.R2
    );

    if (sp.dist < CAPTURE_RADIUS) {
        // snap 到步行面——这是段切换的合法路径之一（另一个是 CP5 钩锁）
        warpTo(sp.seg, sp.t);
        pos_ = sp.pos;
        state_ = State::ON_ROD;
    }
}
```

### 4.2 验证距离

```cpp
// 调试：打印 snap 时的距离
if (sp.dist < CAPTURE_RADIUS * 2) {
    printf("near surface: seg=%d dist=%.1f\n", (int)sp.seg, sp.dist);
}
```

---

## 五、调试辅助

### 5.1 绘制采样点

```cpp
// 在 Game::render() 中加——调试用
if (debugMode_ && player_.state() == Player::FLY) {
    const auto& s = pendulum_.state();
    const auto& pr = pendulum_.params();

    // 画 4 段的采样点
    auto drawSample = [&](Vec2 p, bool isClosest) {
        SDL_SetRenderDrawColor(renderer_, 100, 100, 100, 80);
        if (isClosest) {
            SDL_SetRenderDrawColor(renderer_, 0, 255, 0, 200);
            SDL_RenderDrawLine(renderer_, p.x, p.y,
                               player_.position().x, player_.position().y);
        }
        SDL_Rect r = {(int)p.x - 1, (int)p.y - 1, 2, 2};
        SDL_RenderFillRect(renderer_, &r);
    };

    for each sample in 4 segments:
        drawSample(samplePos, isClosestToPlayer);
}
```

### 5.2 画捕获范围

```cpp
// 在角色周围画一个浅色圆圈表示捕获范围
if (player_.state() == Player::FLY) {
    drawFillCircle(renderer_,
                   player_.position().x, player_.position().y,
                   CAPTURE_RADIUS, {255,255,255,30});
}
```

---

## 六、验证清单

| 操作 | 预期 |
|------|------|
| 按 W 跳离，pendelum 摆回来 | 角色靠近步行面时自动 snap 回去 |
| 从不同位置跳出 | 每次都能 snap 回不同段（不止杆，还有球面） |
| snap 到球面上 | 角色出现在球表面（不是球心）|
| snap 后按住 D | 角色继续沿步行面行走，段间过渡正常 |
| 在屏幕外跳出 | 不会 snap（因为距离远），自由落体 |
| 快速连跳 | 每次跳出后都能 snap 回去，不卡死 |

### 段特异性验证

| 段 | 验证方法 |
|----|---------|
| L1_ROD | 在 L1 中间跳出去，等 pendelum 摆回来，snap 回 L1 杆 |
| M1_BALL | 站在关节球上跳，snap 回球面 |
| L2_ROD | 同理 |
| M2_BALL | 站在末端球上跳，snap 回末端球面 |

---

## 七、翻车点排查

| 现象 | 原因 | 修复 |
|------|------|------|
| 角色永远 snap 不回 | 采样点太少，最近距离 > 15px | 增加 samplesPerSegment 到 20 |
| snap 一次后连续 snap | snap 后 pos_ 还在捕获范围内 | snap 后 state_=ON_ROD，update 不再进 fly |
| snap 到球上时位置不对 | 球面的弧参数计算有误 | 检查 `fromPolar(a, R)` 中的 a 是否正确 lerp |
| snap 到杆上但位置跑偏 | closestPointOnSurface 找到的 t 不对 | 打印 sp.t，检查采样点的坐标计算 |
| 捕获半径内但 snap 不了 | CAPTURE_RADIUS 类型是 float 但传成了 int | 确保 constexpr double |
| snap 后立即又飞出 | 角色 snap 时 pendelum 在高速运动 | snap 时重置 vel_ 为零 |

---

## 八、参数调优参考

| 参数 | 推荐值 | 调大 | 调小 |
|------|-------|------|------|
| CAPTURE_RADIUS | 15px | 吸力范围更大，易 snap | 需精准命中 |
| samplesPerSegment | 20 | 更精确但略慢（80 次循环） | 不精确可能漏掉 |
| 20 采样 × 4 段 = | 80 次/帧 | 开销约 0.01ms，忽略不计 | — |

---

*跑通后进入 [cp5-hook-eject.md](cp5-hook-eject.md)*
