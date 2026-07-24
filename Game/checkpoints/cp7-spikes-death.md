# Checkpoint 7 — 尖刺 + 死亡

> 前置依赖：Checkpoint 6（钩锁充能 + HUD）通过验证。

---

## 一、目标

1. 场景中放置固定尖刺障碍
2. 角色碰到尖刺或飞出屏幕时死亡
3. **钩锁命中尖刺 → 拉到尖刺上 → 死亡**（CP5 的钩中尖刺在这里闭环）
4. 死亡后闪烁 0.5s → 重置回步行面

至此游戏核心玩法闭环：行走 → 跳 → 飞 → 钩 → 死 → 重置。MVP 完成。

---

## 二、新增 / 修改

```
src/
├── Stage.h       ★ 新建: 尖刺容器 + 绘制
├── Stage.cpp     ★ 新建
├── Player.h      (追加: die(), isDead())
├── Player.cpp    (实现 DEAD 状态)
├── Game.h/cpp    (追加: Stage, checkDeaths)
└── 其他文件      (不变)
```

---

## 三、Stage 类

### 3.1 类声明

```cpp
struct Spike {
    SDL_Rect rect;        // AABB 碰撞盒
};

class Stage {
public:
    Stage();

    // 构建固定场景
    void buildDefault();

    // 碰撞检测
    bool checkSpikeCollision(Vec2 charPos, int charW, int charH) const;

    // 绘制
    void draw(SDL_Renderer* r) const;

    const std::vector<Spike>& spikes() const { return spikes_; }

private:
    std::vector<Spike> spikes_;
};
```

### 3.2 默认场景布局

```cpp
void Stage::buildDefault() {
    spikes_.clear();

    // 底部地面尖刺（全宽）
    spikes_.push_back({{0, 580, 800, 20}});

    // 左侧空中孤立尖刺
    spikes_.push_back({{150, 350, 40, 20}});

    // 右侧尖刺集群
    spikes_.push_back({{500, 280, 30, 20}});
    spikes_.push_back({{560, 320, 30, 20}});
    spikes_.push_back({{620, 360, 30, 20}});

    // 顶部出屏线（视觉提醒）
    // 不画碰撞体，仅视觉
}
```

### 3.3 碰撞检测

```cpp
bool Stage::checkSpikeCollision(
    Vec2 charPos, int charW, int charH) const {

    SDL_Rect charRect = {
        (int)charPos.x - charW/2,
        (int)charPos.y - charH,
        charW, charH
    };

    for (const auto& spike : spikes_) {
        SDL_Rect result;
        if (SDL_IntersectRect(&charRect, &spike.rect, &result))
            return true;
    }
    return false;
}
```

### 3.4 绘制

```cpp
void Stage::draw(SDL_Renderer* r) const {
    for (const auto& spike : spikes_) {
        // 画尖刺形状（三角锯齿）
        SDL_Rect rect = spike.rect;
        int x = rect.x, y = rect.y, w = rect.w, h = rect.h;

        SDL_SetRenderDrawColor(r, 255, 50, 50, 255);
        SDL_RenderFillRect(r, &rect);

        // 尖刺上沿锯齿
        SDL_SetRenderDrawColor(r, 200, 30, 30, 255);
        for (int i = 0; i < w; i += 8) {
            int tx = x + i;
            SDL_RenderDrawLine(r, tx, y, tx + 4, y - 6);
        }
    }
}
```

---

## 四、死亡系统

> **CP7 State 扩展**：`State` 枚举从 `{ ON_ROD, FLY, HOOKED }` (CP5) 扩展为 `{ ON_ROD, FLY, HOOKED, DEAD }`。这是最后一次扩展。

### 4.1 死亡触发

```cpp
// Player.h
// ★ 新增常量
constexpr int DEAD_DURATION_MS = 500;  // 死亡动画持续时间
constexpr int SCREEN_MARGIN    = 50;   // 出屏判定缓冲（px）

// ★ 新增方法
void die();
bool isDead() const;
```

### 4.2 死亡条件（3 条路径）

```cpp
// Game::checkDeaths() — 每帧检测

void Game::checkDeaths() {
    // 只有 FLY 和 HOOKED 状态下检测死亡
    // ON_ROD 状态下角色在步行面上，不会被尖刺碰到
    if (player_.state() != Player::FLY &&
        player_.state() != Player::HOOKED) return;

    Vec2 pos = player_.position();

    // 条件 1: 出屏幕
    if (pos.x < -SCREEN_MARGIN || pos.x > 800 + SCREEN_MARGIN ||
        pos.y < -SCREEN_MARGIN || pos.y > 600 + SCREEN_MARGIN) {
        player_.die();
        return;
    }

    // 条件 2: 飞行中碰尖刺
    if (stage_.checkSpikeCollision(pos, 8, 12)) {
        player_.die();
        return;
    }

    // 条件 3: 钩锁拉到尖刺上（★ 新增路径）
    // 在 Player::updateHooked() 中处理：
    // 当钩锁目标为尖刺且角色到达钩点附近 (< 5px) 时，
    // 角色碰到尖刺碰撞盒 → 触发 die()
    // 见 CP5 的 updateHooked 实现
}
```

**钩锁→尖刺→死亡的完整路径：**

```
FLY 状态 → 鼠标左键发射钩爪 → hitAABB 命中尖刺 →
HOOKED 状态 + hookTarget.type == SPIKE →
弹性绳拉向尖刺 → dist < 5px → die() →
DEAD 闪烁 0.5s → 重置回 ON_ROD
```

> ⚠ 这是玩家需要主动规避的风险：向尖刺方向发射钩爪会把自己拉向死亡。

### 4.3 update 分派 — 追加 DEAD

```cpp
// Player::update() 中追加第四个 case
void Player::update(double dt, const Pendulum& p, Vec2 pivot) {
    updateHookCooldowns(dt);

    switch (state_) {
    case State::ON_ROD: updateOnRod(dt, p, pivot); break;
    case State::FLY:   updateFly(dt, p, pivot);   break;
    case State::HOOKED: updateHooked(dt, p, pivot); break;
    case State::DEAD:  updateDead();              break;  // ★ 新增
    }
}
```

### 4.4 die() + updateDead 实现

```cpp
void Player::die() {
    state_ = State::DEAD;
    deathStartTime_ = SDL_GetTicks64();
}

void Player::updateDead() {
    Uint64 elapsed = SDL_GetTicks64() - deathStartTime_;
    if (elapsed >= DEAD_DURATION_MS) {
        // 重置到步行面默认位置
        state_ = State::ON_ROD;
        seg_ = SurfaceSeg::L1_ROD;
        t_ = 0.5;
        target_t_ = 0.5;
        pos_ = footPos(/* 需要 pendelum 当前状态 */);
        vel_ = Vec2(0, 0);
    }
}
```

### 4.5 死亡闪烁绘制

```cpp
void Player::draw(SDL_Renderer* r) const {
    if (state_ == State::DEAD) {
        Uint64 elapsed = SDL_GetTicks64() - deathStartTime_;
        // 闪烁：每 80ms 交替显隐
        if ((elapsed / 80) % 2 == 1) {
            SDL_SetRenderDrawColor(r, 200, 50, 50, 255);
            SDL_Rect rect = {(int)pos_.x - 4, (int)pos_.y - 12, 8, 12};
            SDL_RenderFillRect(r, &rect);
        }
        return;
    }

    // ... 原有 ON_ROD / FLY / HOOKED 绘制 ...
}
```

### 4.6 resetOnDeath — 获取重置位置

```cpp
// die() 时记录当前 pendelum 状态，重置时使用
// 简单做法：重置时取 pendelum 当前实时 footPos
// 更稳妥：die() 时用当前 footPos 缓存一个位置

void Player::die(const Pendulum& p, Vec2 pivot) {
    state_ = State::DEAD;
    deathStartTime_ = SDL_GetTicks64();
    // 记录重置位置（保持 pendelum 继续摆动）
    deathResetSeg_ = SurfaceSeg::L1_ROD;
    deathResetT_ = 0.5;
}
```

---

## 五、Game 修改

### 5.1 完整的渲染流程

```cpp
void Game::render(uint64_t now) {
    // 0. 背景
    SDL_SetRenderDrawColor(renderer_, 15, 15, 25, 255);
    SDL_RenderClear(renderer_);

    // 1. 尖刺
    stage_.draw(renderer_);

    // 2. 轨迹 (CP1)
    // 3. 杆 (CP1)
    // 4. 球 (CP1)
    pendulum_.draw(renderer_, pivot_, now);

    // 5. 钩绳 (CP5)
    player_.drawHookRope(renderer_);

    // 6. 角色 (CP2/3/5/7)
    player_.draw(renderer_);

    // 7. HUD (CP6)
    drawHUD(renderer_);

    SDL_RenderPresent(renderer_);
}
```

### 5.2 更新顺序

```cpp
void Game::update(double dt) {
    if (paused_) return;

    pendulum_.step(dt, 8);
    player_.update(dt, pendulum_, pivot_);
    checkDeaths();      // ★ 死亡检测放在 player update 之后
}
```

---

## 六、验证清单

### 6.1 死亡验证

| 操作 | 预期 |
|------|------|
| 跳向屏幕边缘 | 出屏幕后角色闪烁 0.5s → 重置回步行面 |
| 跳向底部尖刺 | 碰刺后闪烁 → 重置 |
| 跳向空中尖刺 | 同上 |
| FLY 中碰刺 | 触发死亡 |
| HOOKED（钩步行面）时碰刺 | 同样触发死亡 |
| **向尖刺发射钩爪** ★ | 钩中尖刺 → 被拉向尖刺 → 碰到尖刺 → 死亡 |
| 站在步行面上被尖刺碰到 | 不会触发（步行面高于尖刺）|
| 连续死亡 | 每次都正常重置，不卡死 |

### 6.2 死亡后重置验证

```cpp
// 重置后检查:
//   - state_ == ON_ROD
//   - seg_ == L1_ROD
//   - t_ ∈ [0.4, 0.6]
//   - pos_ 在支点附近
```

---

## 七、翻车点排查

| 现象 | 原因 | 修复 |
|------|------|------|
| 没碰尖刺也死亡 | 碰撞盒太大 | 角色碰撞盒用 8×12 而不是全屏 |
| 碰了尖刺不死亡 | 碰撞检测没调用 | 确认 checkDeaths 在每帧 update 中被调用 |
| 钩中尖刺但不死亡 | hookTarget.type 没正确设为 SPIKE | 检查 hitAABB 命中后的 target 类型设置 |
| 钩中尖刺死亡但钩绳不画 | 死亡后 state_=DEAD 导致钩绳不绘制 | 在 die() 前让钩绳先画一帧 |
| 死亡闪烁后不重置 | updateDead 没重置 seg_ 和 t_ | 确认重置逻辑执行 |
| 重置后角色位置不在步行面上 | die() 时没保存重置位置 | footPos 在重置时用当前 pendelum 状态重算 |
| 死亡闪烁太快/太慢 | DEAD_DURATION_MS 和闪烁间隔不对 | 确认 500ms / 80ms |
| 出屏检测不触发 | SCREEN_MARGIN 太小 | 确认是 +50px 不是 -50px |

---

## 八、最终代码统计

```
src/
├── Vec2.h       ~25 行
├── Physics.h    ~70 行  (+ closestPointOnSurface, hitSegment, hitCircle, hitAABB)
├── Physics.cpp  ~120 行
├── Pendulum.h   ~40 行
├── Pendulum.cpp ~90 行
├── Player.h     ~100 行 (SurfaceSeg + 全状态 + HookTarget + 脱落参数)
├── Player.cpp   ~290 行 (4段行走 + FLY + HOOKED + 离心 + 角度脱落 + 杆面下滑 + 死亡)
├── Stage.h      ~25 行  ★ 新增
├── Stage.cpp    ~45 行  ★ 新增
├── Input.h      ~40 行
├── Input.cpp    ~60 行
├── Game.h       ~40 行
├── Game.cpp     ~110 行 (+ 钩刺死亡检测)
└── main.cpp     ~10 行
总计              ~1065 行
```

---

**MVP 完成。** 现在你有一个完整的游戏循环：在 pendelum 上走 → 杆面下滑漂移 → 跳出去 → 被球甩飞 → 钩锁回来 → 小心不要钩到尖刺 → 碰刺/出屏死亡 → 重置。

核心机制全覆盖：
- 双摆混沌物理
- 4 段步行面（杆+球面弧）
- W 跳跃 + 速度继承
- 杆面下滑（倾角 >30°）
- 速度甩出（球面线速度 >80px/s）
- 角度脱落（球面切面 >60°）
- 钩锁弹性绳（命中任意表面含尖刺）
- W 跳跃打断钩锁（附加向上速度）
- 2 发充能 + 冷却 HUD
- 尖刺碰撞 + 出屏 + 钩刺死亡
- 死亡 0.5s 闪烁 → 重置

下一步可以加的东西：轨迹尾迹颜色、音效、收集物、关卡切换、计分、精灵图。设计文档里的「可选（迭代）」清单随意挑选。
