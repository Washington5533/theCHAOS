# Checkpoint 6 — 钩锁充能 + HUD

> 前置依赖：Checkpoint 5（钩锁 + 球面脱落系统）通过验证。

---

## 一、目标

1. 钩锁 2 发充能，独立 0.3s 冷却
2. 屏幕左上角显示能量格 UI

---

## 二、新增 / 修改

```
src/
├── Player.h      (追加拿换联网能系统)
├── Player.cpp    (实现冷却计时)
├── Game.h/cpp    (追加 HUD 绘制)
└── 其他文件      (不变)
```

---

## 三、充能系统

### 3.1 数据结构

```cpp
// Player.h
struct HookCharge {
    bool ready = true;
    float timer = 0.0f;   // 剩余冷却秒数
};

HookCharge charges_[2];

constexpr float MAX_COOLDOWN = 0.3f;  // 秒
```

### 3.2 使用消耗

```cpp
bool Player::tryConsumeCharge() {
    for (auto& c : charges_) {
        if (c.ready) {
            c.ready = false;
            c.timer = MAX_COOLDOWN;
            return true;  // 消耗成功
        }
    }
    return false;  // 两发都在冷却中
}
```

### 3.3 每帧更新

```cpp
void Player::updateHookCooldowns(double dt) {
    for (auto& c : charges_) {
        if (!c.ready) {
            c.timer -= dt;
            if (c.timer <= 0.0f) {
                c.timer = 0.0f;
                c.ready = true;
            }
        }
    }
}

// Player::update() 中调用
void Player::update(double dt, const Pendulum& p, Vec2 pivot) {
    updateHookCooldowns(dt);
    // ... 其余逻辑 ...
}
```

### 3.4 查询接口

```cpp
int  Player::hookCharges() const;        // 返回剩余可用发数 (0/1/2)
float Player::chargeCooldown(int i) const; // 返回第 i 格的冷却进度 [0, 0.3]
```

---

## 四、HUD 绘制

### 4.1 能量格样式

```
┌─ 左上角 (20, 20) ────────────┐
│                               │
│  ┌──┐  ┌──┐                   │
│  │██│  │██│  ← 2 格能量       │
│  └──┘  └──┘                   │
│  可用  冷却中  (0.15/0.30)    │
│                               │
└───────────────────────────────┘
```

### 4.2 绘制实现

```cpp
void Game::drawHUD(SDL_Renderer* r) {
    const int x0 = 20, y0 = 20;
    const int W = 24, H = 28, GAP = 8;

    for (int i = 0; i < 2; i++) {
        int x = x0 + i * (W + GAP);
        int y = y0;

        bool ready = player_.hookCharges() > i;
        float cd = player_.chargeCooldown(i);
        float progress = 1.0f - cd / 0.3f;  // 0=刚消耗, 1=已恢复

        // 外框
        SDL_Rect bg = {x, y, W, H};
        SDL_SetRenderDrawColor(r, 40, 40, 60, 200);
        SDL_RenderFillRect(r, &bg);

        if (ready) {
            // 充满
            SDL_SetRenderDrawColor(r, 0, 255, 100, 220);
            SDL_RenderFillRect(r, &bg);
        } else {
            // 冷却填充：从底部向上
            int fillH = (int)(H * progress);
            if (fillH > 0) {
                SDL_Rect fill = {x, y + H - fillH, W, fillH};
                SDL_SetRenderDrawColor(r, 0, 200, 80, 180);
                SDL_RenderFillRect(r, &fill);
            }
        }

        // 边框
        SDL_SetRenderDrawColor(r, 100, 100, 120, 255);
        SDL_RenderDrawRect(r, &bg);
    }
}
```

---

## 五、钩锁逻辑修改

```cpp
void Player::hook(Vec2 mouseWorld) {
    if (state_ != State::FLY) return;
    if (!tryConsumeCharge()) return;  // ★ 改为消耗充能

    // ... 原有射线检测逻辑 ...
}
```

---

## 六、绘制层级

```
渲染顺序 (从底到顶):
  0. 背景              深灰
  1. 尖刺              (CP7)
  2. 轨迹              (CP1 已有)
  3. 杆 L1/L2          (CP1)
  4. 球                 (CP1)
  5. 钩锁绳             (CP5)
  6. 角色               (CP2)
  7. HUD 能量格         ★ 本 checkpoint 新增
```

---

## 七、验证清单

| 操作 | 预期 |
|------|------|
| 启动 | 左上角 2 个绿色能量格 |
| 发射一次钩锁 | 一格变灰，底部开始绿色填充（冷却） |
| 等待 0.3s | 填充完毕，变回全绿 |
| 发射两次 | 两格都变灰 |
| 第三次尝试 | 发射不出去 |
| 0.3s 后第一格恢复 | 可以再发射一次 |
| 0.6s 后两格全恢复 | 两发齐全 |

---

*跑通后进入 [cp7-spikes-death.md](cp7-spikes-death.md)*
