#pragma once

#include "Vec2.h"
#include "Physics.h"
#include <SDL.h>
#include <vector>
#include <cmath>

// ============================================================
// CP8: 球体交互反馈结构
// ============================================================
struct BallFeedback {
    double landPulse  = 0.0;   // 踩上脉冲 (球扩张, 0.2s回弹)
    double hookFlash  = 0.0;   // 钩中闪光 (高光扩大, 0.15s衰减)
    double jumpRipple = 0.0;   // 跳离挤压 (Y压缩8%, 0.3s回弹)
    double ejectGlow  = 0.0;   // 甩出爆发 (边缘白光, 0.2s衰减)
    double deathBurst = 0.0;   // 死亡红爆 (球变红+喷射红色粒子, 0.3s衰减)
    double scorePulse = 0.0;   // 得分脉冲 (球扩张+3px, 0.15s衰减)
};

// ============================================================
// CP8: 球体运动残影环形缓冲
// ============================================================
struct BallTrail {
    Vec2 positions[12];
    int  head  = 0;
    int  count = 0;

    void record(Vec2 pos) {
        positions[head] = pos;
        head = (head + 1) % 12;
        if (count < 12) count++;
    }
    void decay() {
        if (count > 0) count--;  // 丢弃最旧条目 (record时自然覆盖)
    }
    void clear() { head = 0; count = 0; }
};

// ============================================================
// CP8: 轨道火花粒子
// ============================================================
struct SparkParticle {
    Vec2  pos;
    Vec2  vel;
    float life;    // 剩余生命 (秒)
    float maxLife; // 最大生命 (用于alpha计算)
    SDL_Color color;
};

// ============================================================
// CP8: 碰撞波纹
// ============================================================
struct Ripple {
    Vec2   origin;     // 球面局部坐标
    double radius;     // 当前波纹半径
    double life;       // 剩余生命
    double maxLife;
    int    ballIdx;    // 0=M1, 1=M2
};

class Pendulum
{
public:
    Pendulum();

    // 物理
    void step(double dt, int substeps = 8);

    // 参数访问
    Physics::Params &params();
    const Physics::Params &params() const;
    const Physics::State &state() const;

    // 重置
    void reset();
    void setParams(double R1, double R2, double L1, double L2);

    // 预设切换
    void setPreset(int idx);

    // 微小角速度扰动
    void nudgeW1(double d) { state_.w1 += d; }
    void nudgeW2(double d) { state_.w2 += d; }

    // 坐标查询
    Vec2 jointPos(Vec2 pivot) const;
    Vec2 tipPos(Vec2 pivot) const;

    // ── CP8: 绘制 (增强版) ──
    void draw(SDL_Renderer *r, Vec2 pivot, uint64_t frame);

    // ── CP8: 交互触发接口 ──
    void triggerLand(int ballIdx);       // 玩家踩上球面
    void triggerHookHit(int ballIdx);    // 钩锁命中球面
    void triggerJumpOff(int ballIdx);    // 玩家从球面跳离
    void triggerEject(int ballIdx);      // 角色被球甩出
    void triggerDeath();                 // 玩家死亡 → 球红色爆裂
    void triggerScore();                 // 玩家得分 → 球脉冲扩张+白闪
    void emitBallSparks(int ballIdx);    // 指定球喷射火花 (钩锁命中时调用)

    // ── CP8: 初始化纹理 (需在 renderer 创建后调用一次) ──
    void initTextures(SDL_Renderer *r);
    void destroyTextures();

private:
    Physics::State state_;
    Physics::Params params_;
    Vec2 ballCenters_[2];  // 缓存球心世界坐标 (draw中更新)
    Vec2 pivot_{500, 300};  // 缓存支点 (draw中更新)

    // ── 原始绘制辅助 ──
    static void drawFillCircle(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color c);

    // ── CP8 Phase 1: 预计算球体纹理 ──
    SDL_Texture *ballTex_[2] = {nullptr, nullptr};  // [0]=M1, [1]=M2
    void generateBallTexture(SDL_Renderer *r, int idx, int radius);

    // ── CP8 Phase 1: 速度响应变色 ──
    SDL_Color computeBallColor(int ballIdx, double omega) const;
    static SDL_Color lerpColor(SDL_Color a, SDL_Color b, double t);

    // ── CP8 Phase 2: 交互反馈 ──
    BallFeedback feedback_[2];  // [0]=M1, [1]=M2
    void updateFeedback(double dt);

    // ── CP8 Phase 3: Bloom 外发光 ──
    void drawBloom(SDL_Renderer *r, int cx, int cy, int radius,
                   SDL_Color color, double intensity);

    // ── CP8 Phase 4: 运动残影 ──
    BallTrail trail_[2];  // [0]=M1, [1]=M2
    void recordTrail(Vec2 joint, Vec2 tip);
    void drawTrail(SDL_Renderer *r, int ballIdx, int radius, SDL_Color base);

    // ── CP8 Phase 4: 轨道火花粒子 ──
    std::vector<SparkParticle> sparks_;
    void updateSparks(double dt);
    void emitSparks(Vec2 center, int radius, double omega, SDL_Color color);
    void drawSparks(SDL_Renderer *r);

    // ── CP8 Phase 2: 碰撞波纹 ──
    std::vector<Ripple> ripples_;
    void updateRipples(double dt);
    void drawRipples(SDL_Renderer *r, Vec2 ballCenters[2]);

    // ── CP8: 3D球体绘制 (用纹理) ──
    void drawSphere(SDL_Renderer *r, int cx, int cy, int radius,
                    SDL_Color color, double omega, BallFeedback &fb,
                    int ballIdx, uint64_t frame);
};
