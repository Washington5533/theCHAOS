#pragma once
#define NOMINMAX
#include "Vec2.h"
#include "Physics.h"
#include <graphics.h>
#include <vector>
#include <cmath>

// 球体反馈 (简化版: 仅保留尺寸变化)
struct BallFeedback {
	double landPulse  = 0.0;   // 踩上脉冲
	double hookFlash  = 0.0;   // 钩中闪光
	double jumpRipple = 0.0;   // 跳离挤压
	double ejectGlow  = 0.0;   // 甩出爆发
	double deathBurst = 0.0;   // 死亡红爆
	double scorePulse = 0.0;   // 得分脉冲
};

// 运动残影 (6帧简化版)
struct BallTrail {
	Vec2 positions[6];
	int  head  = 0;
	int  count = 0;

	void record(Vec2 pos) {
		positions[head] = pos;
		head = (head + 1) % 6;
		if (count < 6) count++;
	}
	void decay() { if (count > 0) count--; }
	void clear() { head = 0; count = 0; }
};

// 火花粒子
struct SparkParticle {
	Vec2  pos, vel;
	float life, maxLife;
	COLORREF color;
};

class Pendulum {
public:
	Pendulum();

	// 物理
	void step(double dt, int substeps = 8);

	// 参数
	Physics::Params& params()             { return params_; }
	const Physics::Params& params() const { return params_; }
	const Physics::State& state()   const { return state_; }

	// 重置 + 预设
	void reset();
	void setPreset(int idx);

	// 坐标查询
	Vec2 jointPos(Vec2 pivot) const;
	Vec2 tipPos(Vec2 pivot)   const;

	// 角速度微扰
	void nudgeW1(double d) { state_.w1 += d; }
	void nudgeW2(double d) { state_.w2 += d; }

	// ── 绘制 ──
	void draw(Vec2 pivot, DWORD frame);

	// ── 交互触发 ──
	void triggerLand(int ballIdx);
	void triggerHookHit(int ballIdx);
	void triggerJumpOff(int ballIdx);
	void triggerEject(int ballIdx);
	void triggerDeath();
	void triggerScore();

private:
	Physics::State  state_;
	Physics::Params params_;

	// ── 绘制辅助 ──
	static void fillCircle(int cx, int cy, int r, COLORREF c);
	static COLORREF lerpColor(COLORREF a, COLORREF b, double t);
	COLORREF computeBallColor(int ballIdx, double omega) const;

	// ── 球体渲染 ──
	void drawSphere(int cx, int cy, int radius, COLORREF color, double omega,
	                BallFeedback& fb, int ballIdx, DWORD frame);

	// ── 反馈 ──
	BallFeedback feedback_[2];
	void updateFeedback(double dt);

	// ── 残影 ──
	BallTrail trail_[2];
	void recordTrail(Vec2 joint, Vec2 tip);
	void drawTrail(int ballIdx, int radius, COLORREF base);

	// ── 火花粒子 ──
	std::vector<SparkParticle> sparks_;
	void updateSparks(double dt);
	void emitSparks(Vec2 center, int radius, double omega, COLORREF color);
	void drawSparks();
};
