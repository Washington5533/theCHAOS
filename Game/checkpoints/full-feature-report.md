# 全功能 Checkpoint 报告

> 涵盖 checkpoint 文档中**已有**和**未记录**的全部功能。
> 生成日期：2026-07-17 | 总代码：3,482 行

---

## 目录

- [CP0 — SDL2 窗口初始化](#cp0--sdl2-窗口初始化)
- [CP1 — 双摆物理 + 绘制](#cp1--双摆物理--绘制)
- [CP2 — 角色步行面行走](#cp2--角色步行面行走)
- [CP3 — 跳跃、飞行与杆面下滑](#cp3--跳跃飞行与杆面下滑)
- [CP4 — 落回步行面检测](#cp4--落回步行面检测)
- [CP5 — 钩锁系统 + 球面脱落](#cp5--钩锁系统--球面脱落)
- [CP6 — 钩锁充能 + HUD](#cp6--钩锁充能--hud)
- [CP7 — 尖刺 + 死亡 + 得分平台](#cp7--尖刺--死亡--得分平台)
- [CP8 — 球体3D渲染 + 视觉特效](#cp8--球体3d渲染--视觉特效)
- [无CP — 未归入任何 Checkpoint 的功能](#无cp--未归入任何-checkpoint-的功能)
- [参数对照表：设计文档 vs 实际实现](#参数对照表设计文档-vs-实际实现)
- [功能总览矩阵](#功能总览矩阵)

---

## CP0 — SDL2 窗口初始化

> checkpoint 文档：无独立文档，在 dev-playbook.md 中描述

### 功能

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 0.1 | SDL2 初始化 + 窗口创建 | ✅ | Game.cpp:93-106 |
| 0.2 | 硬件加速渲染器 (SDL_RENDERER_ACCELERATED) | ✅ | Game.cpp:108-113 |
| 0.3 | Alpha 混合模式 (SDL_BLENDMODE_BLEND) | ✅ | Game.cpp:114 |
| 0.4 | 窗口尺寸 1000×940 | ✅ | Game.cpp:102 |
| 0.5 | 主循环 + dt 计算 + cap 0.05s | ✅ | Game.cpp:128-143 |
| 0.6 | ESC 退出 / 窗口关闭 | ✅ | Game.cpp:151-152 |

### 与设计文档差异

| 设计文档 | 实际实现 | 原因 |
|---------|---------|------|
| 800×600 窗口 | 1000×940 | 适配更大球体(R1=50,R2=40)和更长杆(L1=225,L2=180) |
| main.cpp 直接写循环 | Game 类封装 | CP2 引入 Game 类 |

---

## CP1 — 双摆物理 + 绘制

> checkpoint 文档：cp1-double-pendulum.md

### 功能

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 1.1 | Vec2 向量类 (+, -, *, /, len, norm, fromPolar, arcAngle) | ✅ | Vec2.h/cpp |
| 1.2 | Physics::State (α1, α2, ω1, ω2, tip1, tip2) | ✅ | Physics.h:8-23 |
| 1.3 | Physics::Params (L1=225, L2=180, m1=2, m2=1.5, g=600, R1=50, R2=40) | ✅ | Physics.h:25-32 |
| 1.4 | 拉格朗日加速度公式 (acceleration) | ✅ | Physics.cpp |
| 1.5 | Symplectic Euler 步进 (step) | ✅ | Physics.cpp |
| 1.6 | 多子步封装 (substep, 32子步/帧) | ✅ | Physics.cpp |
| 1.7 | 角度折叠 (fold, [-π, π]) | ✅ | Physics.cpp |
| 1.8 | 动能/势能计算 (kinetic, potential) | ✅ | Physics.h:39-40 |
| 1.9 | Pendulum 类 (step, reset, setPreset, jointPos, tipPos) | ✅ | Pendulum.h/cpp |
| 1.10 | 5种初始预设 (1~5键切换) | ✅ | Pendulum.cpp |
| 1.11 | 杆 L1/L2 绘制 (线条) | ✅ | Pendulum.cpp |
| 1.12 | 球体绘制 (支点黄/m1红/m2蓝) | ✅ | Pendulum.cpp |
| 1.13 | drawFillCircle 扫描线实心圆 | ✅ | Pendulum.cpp |
| 1.14 | Space 暂停 | ✅ | Game.cpp:159 |
| 1.15 | R 键重置 | ✅ | Game.cpp:156-158 |
| 1.16 | 角速度扰动 (nudgeW1/nudgeW2) | ✅ | Pendulum.h:83-84 |
| 1.17 | setParams 动态调参 | ✅ | Pendulum.h:77 |

### 与设计文档差异

| 设计文档 | 实际实现 | 原因 |
|---------|---------|------|
| L1=150, L2=120 | L1=225, L2=180 | 适配更大窗口(1000×940) |
| R1=15, R2=12 | R1=50, R2=40 | 球体需要足够大以提供步行面 |
| 8 子步/帧 | 32 子步/帧 | 更高精度，能量漂移更低 |
| 状态变量 th1/th2 | alpha1/alpha2 | 代码命名变更 |
| Tracker 轨迹类 | 未创建独立类 | 轨迹功能延后，球体残影在CP8实现 |

---

## CP2 — 角色步行面行走

> checkpoint 文档：cp2-surface-walk.md

### 功能

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 2.1 | SurfaceSeg 枚举 (L1_ROD, M1_BALL, L2_ROD, M2_BALL) | ✅ | Player.h:16-22 |
| 2.2 | Player 类 (pos_, seg_, t_, target_t_) | ✅ | Player.h:38-162 |
| 2.3 | footPos() 四段步行面坐标计算 | ✅ | Player.cpp |
| 2.4 | 杆面偏移 (ROD_OFFSET=12px, 垂直于杆中心线) | ✅ | Player.h:138 |
| 2.5 | 阻尼平滑移动 (DAMPING=0.85) | ✅ | Player.h:137 |
| 2.6 | A/D 左右移动 (WALK_SPEED=0.2) | ✅ | Player.h:136 |
| 2.7 | 段边界 clamp (球体挡住不跨段) | ✅ | Player.cpp |
| 2.8 | rodSide (±1) 杆面双侧支持 | ✅ | Player.h:82 |
| 2.9 | Game 类封装 (init/run/shutdown/handleInput/update/render) | ✅ | Game.h/cpp |
| 2.10 | 角色绘制 (8×12 像素块, 状态色) | ✅ | Player.cpp |

### 与设计文档差异

| 设计文档 | 实际实现 | 原因 |
|---------|---------|------|
| ROD_OFFSET=5px | 12px | 球半径增大后比例调整 |
| 角色从L1_ROD t=0.5起步 | 从L2_ROD t=0.5起步 | 默认出生位置改为L2 |
| 四段连续行走(自动过渡) | 四段隔离(球挡住) | 设计变更：段间切换只能通过钩锁/snap |
| Input.h/cpp 独立输入类 | 未创建 | 键盘3行内联到Game.cpp |
| 窗口800×600 | 1000×940 | 适配更大物理参数 |

---

## CP3 — 跳跃、飞行与杆面下滑

> checkpoint 文档：cp3-jump-fly.md

### 功能

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 3.1 | State 枚举 (ON_ROD, FLY, HOOKED, DEAD) | ✅ | Player.h:9-15 |
| 3.2 | jump() 跳跃脱手 + 速度继承 | ✅ | Player.cpp |
| 3.3 | footVel() 四段脚下速度计算 | ✅ | Player.cpp |
| 3.4 | updateFly() 自由落体 (vel_.y += g*dt) | ✅ | Player.cpp |
| 3.5 | JUMP_BOOST=400 px/s (W键附加上跳) | ✅ | Player.h:135 |
| 3.6 | 杆面下滑 (rodSlideSpeed, 倾角>30°触发) | ✅ | Physics.h:42-43 |
| 3.7 | SLIDE_FACTOR=0.25 | ✅ | Player.h:133 |
| 3.8 | Physics 工具方法 (rodSlideSpeed, ballSurfaceSpeed, ballAngleFromTop) | ✅ | Physics.h:42-49 |
| 3.9 | update 分派 (switch state) | ✅ | Player.cpp |
| 3.10 | 状态色区分 (ON_ROD绿 / FLY黄) | ✅ | Player.cpp |

### 与设计文档差异

| 设计文档 | 实际实现 | 原因 |
|---------|---------|------|
| JUMP_BOOST=200 | 400 | 球速峰值更高，需要更大跳跃力 |
| SLIDE_FACTOR=0.3 | 0.25 | 下滑速度微调 |
| GRAVITY 在Player.h定义 | GRAVITY 在Physics.h定义(600) | 统一物理常量 |

---

## CP4 — 落回步行面检测

> checkpoint 文档：cp4-snap-detection.md

### 功能

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 4.1 | SurfacePoint 结构体 (seg, t, worldPos, distance, rodSide) | ✅ | Player.h:26-32 |
| 4.2 | findNearestSurface() 四段采样最近点检测 | ✅ | Player.cpp |
| 4.3 | GRAB_DISTANCE=18px (捕获半径) | ✅ | Player.h:142 |
| 4.4 | snap 回面 (warpTo, FLY→ON_ROD) | ✅ | Player.cpp |
| 4.5 | FLY_GRACE_FRAMES=15 (跳跃保护帧数) | ✅ | Player.h:147 |
| 4.6 | FLY_GRACE_BALL=10 (球面保护偏移) | ✅ | Player.h:148 |
| 4.7 | WARP_HYSTERESIS=0.06 (段切换防抖) | ✅ | Player.h:146 |
| 4.8 | DEAD_Y=1000 (死亡Y阈值) | ✅ | Player.h:143 |

### 与设计文档差异

| 设计文档 | 实际实现 | 原因 |
|---------|---------|------|
| CAPTURE_RADIUS=15 | GRAB_DISTANCE=18 | 微调捕获灵敏度 |
| samplesPerSegment=20 | 代码内自行控制 | 不暴露参数 |
| Physics::closestPointOnSurface | Player::findNearestSurface | 检测逻辑内聚到Player |
| 出屏50px缓冲死亡 | DEAD_Y=1000 阈值 | 简化为Y坐标判断 |

---

## CP5 — 钩锁系统 + 球面脱落

> checkpoint 文档：cp5-hook-eject.md ✅ 已完成

### 功能

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 5.1 | hook() 发射钩爪 (射线检测4段) | ✅ | Player.cpp |
| 5.2 | 射线-线段检测 (rayHitSegment) | ✅ | Player.cpp 匿名ns |
| 5.3 | 射线-圆检测 (rayHitCircle) | ✅ | Player.cpp 匿名ns |
| 5.4 | HOOK_MAX_RANGE=320px | ✅ | Player.h:151 |
| 5.5 | HOOK_K=7.0 (弹性系数) | ✅ | Player.h:152 |
| 5.6 | HOOK_REST=8px (自然绳长) | ✅ | Player.h:153 |
| 5.7 | HOOK_MAX_STRETCH=1.4 (最大拉伸倍率) | ✅ | Player.h:154 |
| 5.8 | HOOK_SNAP_DIST=5px (自动抓回) | ✅ | Player.h:155 |
| 5.9 | updateHooked() 弹性绳物理 (拉力+绳长约束+重力) | ✅ | Player.cpp |
| 5.10 | 钩点坐标 (surfacePosAt, 随摆锤运动) | ✅ | Player.cpp |
| 5.11 | W键跳跃打断钩锁 (releaseHook + JUMP_BOOST) | ✅ | Player.cpp |
| 5.12 | 钩锁绳绘制 (黄色虚线) | ✅ | Player.cpp |
| 5.13 | 球面速度甩出 (BALL_THROW_SPEED=200 px/s) | ✅ | Player.h:144 |
| 5.14 | 球面角度脱落 (t<0.01 或 t>0.99) | ✅ | Player.cpp |
| 5.15 | resolveBallCollision() 球体碰撞 (下半硬弹/上半软推) | ✅ | Player.cpp |
| 5.16 | 鼠标跟踪 (SDL_MOUSEMOTION → mouseWorld_) | ✅ | Game.cpp:185-188 |
| 5.17 | 左键发射钩锁 (SDL_MOUSEBUTTONDOWN) | ✅ | Game.cpp:189-193 |
| 5.18 | hookSeg_/hookT_/hookMaxLen_/hookWorldPos_ | ✅ | Player.h:94-97 |

### 与设计文档差异

| 设计文档 | 实际实现 | 原因 |
|---------|---------|------|
| HOOK_K=8.0 | 7.0 | 手感微调 |
| HOOK_MAX_STRETCH=1.2 | 1.4 | 允许更长拉伸 |
| HOOK_MAX_RANGE=300 | 320 | 略增射程 |
| SPIN_EJECT=80 px/s | BALL_THROW_SPEED=200 | 球半径50/40后，等效|ω|>4 |
| 角度脱落>60° | t<0.01或t>0.99边界检测 | 简化为段参数边界 |
| Input.h/cpp | 未创建 | 鼠标3行内联Game.cpp |
| hitAABB (尖刺射线) | 延至CP7 | 尖刺在CP7才创建 |

---

## CP6 — 钩锁充能 + HUD

> checkpoint 文档：cp6-hud-cooldown.md

### 功能

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 6.1 | HookSlot 双槽位 (ready + timer) | ✅ | Player.h:100-101 |
| 6.2 | HOOK_COOLDOWN=0.5s | ✅ | Player.h:102 |
| 6.3 | updateCooldowns() 每帧计时 | ✅ | Player.cpp |
| 6.4 | 消耗充能 (tryConsumeCharge) | ✅ | Player.cpp |
| 6.5 | drawHUD() 能量格绘制 (左上角) | ✅ | Player.cpp |
| 6.6 | 冷却进度条 (从底部向上填充) | ✅ | Player.cpp |

### 与设计文档差异

| 设计文档 | 实际实现 | 原因 |
|---------|---------|------|
| MAX_COOLDOWN=0.3s | HOOK_COOLDOWN=0.5s | 平衡钩锁节奏 |
| 格子尺寸24×28 | 代码内控 | 适配1000×940窗口 |

---

## CP7 — 尖刺 + 死亡 + 得分平台

> checkpoint 文档：cp7-spikes-death.md

### 功能

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 7.1 | Stage 类 (spikes_, plats_, platScored_) | ✅ | Stage.h |
| 7.2 | Spike 结构体 (rect + inverted) | ✅ | Stage.h:6-9 |
| 7.3 | SafeBlock 结构体 (可站立平台) | ✅ | Stage.h:11-13 |
| 7.4 | regenerate() 随机生成场景 (2尖刺+2平台) | ✅ | Stage.cpp:12-69 |
| 7.5 | 尖刺随机参数 (位置/大小/正反置) | ✅ | Stage.cpp:19-47 |
| 7.6 | 30%概率顶部倒置+下部正置 / 70%双下部 | ✅ | Stage.cpp:19-46 |
| 7.7 | 得分平台随机生成 (A上部/B中部) | ✅ | Stage.cpp:49-68 |
| 7.8 | checkSpikeCollision() AABB碰撞 | ✅ | Stage.cpp:96-109 |
| 7.9 | checkPlatformTop() 平台站立检测 | ✅ | Stage.cpp:111-136 |
| 7.10 | checkPlatformTouch() 平台触碰得分 | ✅ | Stage.cpp:138-152 |
| 7.11 | regeneratePlatformAt() 单个平台刷新 | ✅ | Stage.cpp:81-94 |
| 7.12 | die() 触发死亡 | ✅ | Player.cpp |
| 7.13 | updateDead() 死亡状态 (0.5s后重置) | ✅ | Player.cpp |
| 7.14 | 死亡粒子爆发 (12个粒子) | ✅ | Player.cpp |
| 7.15 | reset_Player() 重置角色 | ✅ | Player.cpp |
| 7.16 | 得分系统 (addScore, score_) | ✅ | Player.h:69-71 |
| 7.17 | 死亡扣分 (-1) | ✅ | Game.cpp:226-232 |
| 7.18 | 死亡后场景刷新 (stage_.regenerate) | ✅ | Game.cpp:229 |
| 7.19 | 平台触碰得分 (+1) | ✅ | Game.cpp:247-254 |
| 7.20 | 平台得分后单个刷新 | ✅ | Game.cpp:252 |
| 7.21 | 尖刺碰撞 → 死亡 | ✅ | Game.cpp:236-237 |
| 7.22 | 平台站立 (landOnPlatform) | ✅ | Game.cpp:240-243 |

### 尖刺视觉 (cp8-ball-rendering.md 未记录，实际实装)

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 7.23 | 暗色基座 + 亮边 | ✅ | Stage.cpp:161-167 |
| 7.24 | 三角锥阵列 (扫描线填充) | ✅ | Stage.cpp:169-218 |
| 7.25 | 正置/倒置尖刺 | ✅ | Stage.cpp:177-218 |
| 7.26 | 尖端脉冲闪烁 (sin驱动, 相位偏移) | ✅ | Stage.cpp:190-196 |
| 7.27 | drawFillTriangle 扫描线三角形 | ✅ | Stage.cpp:277-308 |

### 得分平台视觉 (cp8-ball-rendering.md 未记录)

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 7.28 | 发光悬浮球 (逐行渐变扫描) | ✅ | Stage.cpp:222-261 |
| 7.29 | 外层光晕 | ✅ | Stage.cpp:237-239 |
| 7.30 | 高光点 | ✅ | Stage.cpp:256-260 |
| 7.31 | 浮动动画 (sin驱动, 相位偏移) | ✅ | Stage.cpp:230-232 |

---

## CP8 — 球体3D渲染 + 视觉特效

> checkpoint 文档：cp8-ball-rendering.md + cp8-issues.md

### Phase 1: 3D立体 + 速度变色

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 8.1 | 预计算球体纹理 (generateBallTexture, Blinn-Phong) | ✅ | Pendulum.cpp |
| 8.2 | SDL_Texture 管理 (initTextures/destroyTextures) | ✅ | Pendulum.h:103-104 |
| 8.3 | SDL_SetTextureColorMod 实时调色 | ✅ | Pendulum.cpp |
| 8.4 | computeBallColor() 速度响应变色 (冷→热→白热) | ✅ | Pendulum.cpp |
| 8.5 | lerpColor() 颜色线性插值 | ✅ | Pendulum.h:121 |
| 8.6 | M1 变色: 暗红(80,30,30)→正红(255,60,60)→白热(255,220,180) | ✅ | Pendulum.cpp |
| 8.7 | M2 变色: 暗蓝(30,40,80)→亮蓝(140,200,255)→白热(220,240,255) | ✅ | Pendulum.cpp |
| 8.8 | 心跳脉动 (±1.5% 半径, sin(frame*0.05)) | ✅ | Pendulum.cpp |
| 8.9 | 降级模式 (纹理未初始化→分环近似) | ✅ | Pendulum.cpp |

### Phase 2: 交互反馈

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 8.10 | BallFeedback 结构体 (6种反馈值) | ✅ | Pendulum.h:12-19 |
| 8.11 | landPulse 踩上脉冲 (球扩张+4px, 0.2s) | ✅ | Pendulum.cpp |
| 8.12 | hookFlash 钩中闪光 (高光扩大, 0.15s) | ✅ | Pendulum.cpp |
| 8.13 | jumpRipple 跳离挤压 (Y压缩8%, 0.3s) | ✅ | Pendulum.cpp |
| 8.14 | ejectGlow 甩出爆发 (边缘白光, 0.2s) | ✅ | Pendulum.cpp |
| 8.15 | deathBurst 死亡红爆 (球变红+喷红色粒子, 0.3s) | ✅ | Pendulum.h:17 |
| 8.16 | scorePulse 得分脉冲 (球扩张+3px, 0.15s) | ✅ | Pendulum.h:18 |
| 8.17 | triggerLand/HookHit/JumpOff/Eject/Death/Score 触发接口 | ✅ | Pendulum.h:94-100 |
| 8.18 | Player.cpp 调用触发 (warpTo/hook/jump/甩出) | ✅ | Player.cpp |
| 8.19 | 碰撞波纹 Ripple (钩中球时白色圆环扩散, 0.3s) | ✅ | Pendulum.h:54-60 |
| 8.20 | updateRipples / drawRipples | ✅ | Pendulum.cpp |

### Phase 3: Bloom 外发光

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 8.21 | 3层光晕 (R×1.4/R×2.0/R×2.5) | ✅ | Pendulum.cpp |
| 8.22 | 动能驱动强度 (KE/50000映射) | ✅ | Pendulum.cpp |
| 8.23 | 阈值控制 (intensity<0.15不绘制) | ✅ | Pendulum.cpp |

### Phase 4: 残影 + 火花

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| 8.24 | BallTrail 残影环形缓冲 (12帧) | ✅ | Pendulum.h:24-38 |
| 8.25 | 表面速度>150px/s时显示残影 | ✅ | Pendulum.cpp |
| 8.26 | SparkParticle 轨道火花 | ✅ | Pendulum.h:43-49 |
| 8.27 | |ω|>3 rad/s 时球面喷射火花 | ✅ | Pendulum.cpp |
| 8.28 | 火花颜色跟随球体速度色 | ✅ | Pendulum.cpp |
| 8.29 | drawSphere() 综合3D球体绘制 | ✅ | Pendulum.h:148-150 |

---

## 无CP — 未归入任何 Checkpoint 的功能

> 以下功能在实际代码中已实现，但未被任何 checkpoint 文档 (cp1~cp8) 记录。

### A. 像素字体系统

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| A.1 | 3×5 点阵数字字体 (DIGITS[10][5][3]) | ✅ | Game.cpp:7-18 |
| A.2 | 3×5 点阵字母字体 (ALPHA[26][5][3], A-Z完整) | ✅ | Game.cpp:20-47 |
| A.3 | drawText() 文本渲染 (支持dot缩放) | ✅ | Game.cpp:48-69 |
| A.4 | drawNum() 数字渲染 (支持0值) | ✅ | Game.cpp:71-91 |
| A.5 | 支持空格和横线特殊字符 | ✅ | Game.cpp:52-56 |

### B. 暂停菜单 (能量核心风格)

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| B.1 | 全屏暗色遮罩 (alpha=160) | ✅ | Game.cpp:287-289 |
| B.2 | 外层呼吸光晕 (放大版菱形) | ✅ | Game.cpp:296-305 |
| B.3 | 菱形主体 (逐行颜色渐变) | ✅ | Game.cpp:308-316 |
| B.4 | 内层高亮核心 (小菱形) | ✅ | Game.cpp:319-326 |
| B.5 | 中心白色高光点 (呼吸alpha) | ✅ | Game.cpp:329-332 |
| B.6 | PAUSED 标题 (dot=4放大) | ✅ | Game.cpp:336-339 |
| B.7 | SCORE/TIME 状态信息显示 | ✅ | Game.cpp:342-352 |
| B.8 | 键位提示分色渲染 (字母蓝色/描述灰色) | ✅ | Game.cpp:355-370 |
| B.9 | 脉冲动画 (sin(now*0.003) 驱动) | ✅ | Game.cpp:292-293 |

### C. 存活计时系统

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| C.1 | survivalTime_ 存活计时器 | ✅ | Game.h:30 |
| C.2 | 非死亡状态累加计时 | ✅ | Game.cpp:213-223 |
| C.3 | 每15秒自动+1分 | ✅ | Game.cpp:215-222 |
| C.4 | 死亡后重置计时器 | ✅ | Game.cpp:230-231 |

### D. 重力调节

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| D.1 | Q键 减少重力倍率 (-0.1x) | ✅ | Game.cpp:173-175 |
| D.2 | E键 增加重力倍率 (+0.1x) | ✅ | Game.cpp:176-178 |
| D.3 | gravScale_ 重力倍率 (范围0.1x~3.0x) | ✅ | Player.h:87,72 |
| D.4 | adjGravity() 接口 | ✅ | Player.h:72 |
| D.5 | 控制台打印当前倍率 | ✅ | Game.cpp:174,177 |

### E. 球捕获开关

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| E.1 | F键 切换球捕获模式 | ✅ | Game.cpp:167-172 |
| E.2 | ballCaptureOn_ 开关 | ✅ | Player.h:158 |
| E.3 | toggleBallCapture() / isBallCaptureOn() | ✅ | Player.h:160-161 |
| E.4 | 全程距离 / 仅杆端 两种模式 | ✅ | Player.h:160 |

### F. HUD 速度显示

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| F.1 | M1 表面线速度显示 (橙色) | ✅ | Game.cpp:273-274 |
| F.2 | M2 表面线速度显示 (蓝色) | ✅ | Game.cpp:276-277 |
| F.3 | 角色速度显示 (绿→红渐变, 0~1000px/s) | ✅ | Game.cpp:279-282 |
| F.4 | speed() 角色速度计算接口 | ✅ | Player.h:67 |

### G. 角色能量核心体视觉

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| G.1 | 菱形轮廓绘制 | ✅ | Player.cpp |
| G.2 | 中心高光点 | ✅ | Player.cpp |
| G.3 | 呼吸光晕 | ✅ | Player.cpp |
| G.4 | 状态色继承 (绿/黄/蓝/红闪烁) | ✅ | Player.cpp |
| G.5 | 死亡红色粒子爆发 | ✅ | Player.cpp |

### H. 钩锁绳能量视觉

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| H.1 | 发光能量绳 (黄色渐变) | ✅ | Player.cpp |
| H.2 | 能量残影 (仅拉伸时) | ✅ | Player.cpp |
| H.3 | 火花粒子沿绳飞散 | ✅ | Player.cpp |

### I. 得分离子迸溅

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| I.1 | ScoreSpark 结构体 (pos, vel, life, maxLife) | ✅ | Player.h:35 |
| I.2 | emitScoreSparks() 从角色位置喷射 | ✅ | Player.h:70 |
| I.3 | 得分时触发 (平台触碰+存活奖励) | ✅ | Game.cpp:219,251 |
| I.4 | 粒子绘制+衰减 | ✅ | Player.cpp |

### J. 角色运动残影

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| J.1 | trail_[] 16帧环形缓冲 | ✅ | Player.h:90 |
| J.2 | 彗星拖尾效果 | ✅ | Player.cpp |

### K. A/D 持续输入

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| K.1 | aHeld/dHeld 按键状态跟踪 | ✅ | Game.cpp:147 |
| K.2 | SDL_KEYUP 释放跟踪 | ✅ | Game.cpp:180-184 |
| K.3 | 持续输入每帧调用 moveLeft/moveRight | ✅ | Game.cpp:196-197 |

### L. W 键防重复触发

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| L.1 | !e.key.repeat 过滤 | ✅ | Game.cpp:160 |
| L.2 | 确保单次跳跃不连跳 | ✅ | Game.cpp:160 |

### M. 窗口标题

| # | 功能 | 状态 | 文件 |
|---|------|------|------|
| M.1 | "chaos - Checkpoint 5" | ✅ | Game.cpp:99 |

---

## 参数对照表：设计文档 vs 实际实现

| 参数 | game-design.md | 实际代码 | 差异原因 |
|------|---------------|---------|---------|
| 窗口 | 800×600 | 1000×940 | 适配更大物理尺寸 |
| L1 | 150 px | 225 px | 窗口放大后等比放大 |
| L2 | 120 px | 180 px | 同上 |
| R1 | 15 px | 50 px | 球体需足够大提供步行面 |
| R2 | 12 px | 40 px | 同上 |
| g | 600 px/s² | 600 px/s² | 一致 |
| 子步 | 8 | 32 | 更高精度 |
| JUMP_BOOST | 200 | 400 | 球速更高需更大跳跃力 |
| SPIN_EJECT | 80 px/s | 200 px/s (BALL_THROW_SPEED) | R增大后等效ω阈值调整 |
| HOOK_K | 8.0 | 7.0 | 手感微调 |
| HOOK_MAX_RANGE | 300 | 320 | 略增射程 |
| HOOK_MAX_STRETCH | 1.2 | 1.4 | 允许更长拉伸 |
| HOOK_COOLDOWN | 0.3s | 0.5s | 平衡钩锁节奏 |
| ROD_OFFSET | 5 px | 12 px | 比例调整 |
| SLIDE_FACTOR | 0.3 | 0.25 | 下滑速度微调 |
| CAPTURE_RADIUS | 15 px | 18 px (GRAB_DISTANCE) | 微调灵敏度 |
| 出生段 | L1_ROD t=0.5 | L2_ROD t=0.5 | 默认出生位置改到L2 |
| pivot | (400, 120) | (500, 300) | 窗口居中调整 |

---

## 功能总览矩阵

### 核心机制 (MVP)

| 功能 | CP | 状态 |
|------|-----|------|
| 混沌双摆物理 (拉格朗日+Symplectic Euler) | CP1 | ✅ |
| 4段步行面 (杆+球面弧) | CP2 | ✅ |
| W跳跃 + 速度继承 | CP3 | ✅ |
| 杆面下滑 (倾角>30°) | CP3 | ✅ |
| FLY落回面snap检测 | CP4 | ✅ |
| 钩锁弹性绳 (命中任意表面) | CP5 | ✅ |
| 球面速度甩出 | CP5 | ✅ |
| 球面角度脱落 | CP5 | ✅ |
| W跳跃打断钩锁 | CP5 | ✅ |
| 球体碰撞 (下半硬弹/上半软推) | CP5 | ✅ |
| 2发充能 + 冷却HUD | CP6 | ✅ |
| 尖刺碰撞 + 死亡 | CP7 | ✅ |
| 死亡闪烁 + 重置 | CP7 | ✅ |

### 得分与关卡

| 功能 | CP | 状态 |
|------|-----|------|
| 得分平台 (触碰得分+1) | CP7(实装) | ✅ |
| 存活计时 (每15秒+1) | 无CP | ✅ |
| 死亡扣分 (-1) | 无CP | ✅ |
| 随机场景生成 (尖刺+平台) | CP7(实装) | ✅ |
| 死亡后场景刷新 | 无CP | ✅ |
| 平台得分后单个刷新 | 无CP | ✅ |
| 平台站立检测 | CP7(实装) | ✅ |

### 视觉特效

| 功能 | CP | 状态 |
|------|-----|------|
| 球体3D立体光照 (Blinn-Phong预计算纹理) | CP8 | ✅ |
| 球体速度响应变色 (冷→热→白热) | CP8 | ✅ |
| 球体心跳脉动 | CP8 | ✅ |
| Bloom外发光 (3层光晕) | CP8 | ✅ |
| 球体运动残影 (12帧) | CP8 | ✅ |
| 轨道火花粒子 | CP8 | ✅ |
| 碰撞波纹 | CP8 | ✅ |
| 6种交互反馈 (踩/钩/跳/甩/死/分) | CP8 | ✅ |
| 尖刺三角锥阵列+脉冲闪烁 | 无CP | ✅ |
| 得分平台发光悬浮球 | 无CP | ✅ |
| 角色能量核心体 (菱形+光晕) | 无CP | ✅ |
| 钩锁绳能量残影+火花 | 无CP | ✅ |
| 得分离子迸溅 | 无CP | ✅ |
| 角色16帧运动残影 | 无CP | ✅ |
| 死亡粒子爆发 | 无CP | ✅ |

### UI / HUD

| 功能 | CP | 状态 |
|------|-----|------|
| 钩锁充能格 (2格+冷却进度) | CP6 | ✅ |
| 速度显示 (M1/M2/角色, 颜色渐变) | 无CP | ✅ |
| 像素字体系统 (数字0-9+字母A-Z) | 无CP | ✅ |
| 暂停菜单 (能量核心风格) | 无CP | ✅ |
| 暂停菜单键位提示 (分色渲染) | 无CP | ✅ |
| SCORE/TIME 显示 | 无CP | ✅ |

### 调试与操控

| 功能 | CP | 状态 |
|------|-----|------|
| R键全局重置 | CP1 | ✅ |
| 1~5键切换预设 | CP1 | ✅ |
| Space暂停 | CP1 | ✅ |
| Q/E重力调节 | 无CP | ✅ |
| F键球捕获切换 | 无CP | ✅ |
| A/D持续输入 | 无CP | ✅ |
| W防重复触发 | 无CP | ✅ |
| 角速度扰动 (nudgeW1/W2) | CP1 | ✅ |
| 动态参数调整 (setParams) | CP1 | ✅ |

### 未实现 / 延后

| 功能 | 来源 | 状态 | 说明 |
|------|------|------|------|
| 轨迹尾迹 (Tracker类) | CP1设计 | ❌ | 球体残影在CP8替代实现 |
| Input 独立输入类 | CP2设计 | ❌ | 键盘/鼠标内联Game.cpp |
| 钩锁命中尖刺 (hitAABB) | CP5/CP7设计 | ❌ | 尖刺不可钩，仅致死 |
| 目标点/收集物 | game-design.md | ❌ | 被得分平台系统替代 |
| 音效 | game-design.md | ❌ | 未实装 |
| 镜头跟随 | game-design.md | ❌ | 未实装 |
| 关卡列表 | game-design.md | ❌ | 被随机场景替代 |
| 背景图/视差 | CP8设计 | ❌ | 未实装 |
| 角色精灵图 | CP8设计 | ❌ | 使用代码绘制能量核心体 |
| 经纬线/自转标记 | CP8设计 | ❌ | 预计算纹理替代 |

---

*报告覆盖全部 3,482 行代码中实现的功能。*
*checkpoint 文档中标记的功能全部已实装，额外实现了大量未归入checkpoint的UI、得分、视觉和调试功能。*
