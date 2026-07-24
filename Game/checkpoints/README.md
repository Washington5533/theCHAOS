# Checkpoints

按顺序开发，每个 checkpoint 都是**可编译、可运行**的独立里程碑。

| # | 文档 | 产物 | 累计行数 | 预计时间 |
|---|------|------|---------|---------|
| 1 | [cp1-double-pendulum.md](cp1-double-pendulum.md) | 双摆物理 + 绘制 | ~290 | 30min |
| 2 | [cp2-surface-walk.md](cp2-surface-walk.md) | Player 步行面行走 (atan2 球面弧) | ~460 | 45min |
| 3 | [cp3-jump-fly.md](cp3-jump-fly.md) | 跳跃 + 飞行 + 杆面下滑 | ~580 | 35min |
| 4 | [cp4-snap-detection.md](cp4-snap-detection.md) | 落回步行面检测 | ~660 | 30min |
| 5 | [cp5-hook-eject.md](cp5-hook-eject.md) | 钩锁 + 速度甩出 + 角度脱落 + 钩尖刺 | ~850 | 50min |
| 6 | [cp6-hud-cooldown.md](cp6-hud-cooldown.md) | 充能 + HUD | ~910 | 20min |
| 7 | [cp7-spikes-death.md](cp7-spikes-death.md) | 尖刺 + 死亡重置 + 钩刺死 | ~970 | 30min |
| 8 | [cp8-issues.md](cp8-issues.md) | 待解决问题 & 优化项 | — | 持续 |

**规则：**
- 每个 checkpoint 做完后，**必须通过验证清单所有项**再继续
- 按 `R` 键重置全程保留
- 有 bug 就在本 checkpoint 修，绝不留到下一步
