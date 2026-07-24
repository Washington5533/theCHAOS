#pragma once
#define NOMINMAX
#include <graphics.h>
#include <vector>
#include <cstdlib>
#include <cmath>
#include "Vec2.h"

class Pendulum;

enum class State { ON_ROD, FLY, HOOKED, DEAD };
enum class SurfaceSeg { L1_ROD, M1_BALL, L2_ROD, M2_BALL };

struct SurfacePoint {
	SurfaceSeg seg;
	double t, distance, rodSide = 1.0;
	Vec2 worldPos;
};

struct Particle  { Vec2 pos, vel; float life; };
struct ScoreSpark { Vec2 pos, vel; float life, maxLife; };

class Player {
public:
	Player() { reset_Player(); }
	~Player() {}

	void reset_Player(SurfaceSeg seg = SurfaceSeg::L2_ROD, double t = 0.5);
	void update(double dt, Pendulum& p, Vec2 pivot);
	void moveLeft();
	void moveRight();
	void jump(Pendulum& p, Vec2 pivot);
	void hook(Vec2 mouseWorld, Pendulum& p, Vec2 pivot);
	void releaseHook();
	void die();
	void landOnPlatform(double standY);
	void addScore(int n)       { score_ += n; if (score_ < 0) score_ = 0; }
	void resetScore()          { score_ = 0; }
	void emitScoreSparks();
	int  score()         const { return score_; }
	Vec2 position()      const;
	State state()        const { return state_; }
	double velY()        const { return vel_.y; }
	double speed()       const { return sqrt(vel_.x*vel_.x + vel_.y*vel_.y); }
	void adjGravity(double d)  { gravScale_ += d; if (gravScale_ < 0.1) gravScale_ = 0.1; if (gravScale_ > 3.0) gravScale_ = 3.0; }
	double gravityScale()const { return gravScale_; }
	void toggleBallCapture()   { ballCaptureOn_ = !ballCaptureOn_; }
	bool isBallCaptureOn()const{ return ballCaptureOn_; }

	void draw() const;

private:
	// 步行面定位
	SurfaceSeg seg_ = SurfaceSeg::L1_ROD;
	State state_ = State::ON_ROD;
	Vec2 vel_, pos_;
	double t_ = 0.5, target_t_ = 0.5, rodSide_ = 1.0;
	bool inputLeft_ = false, inputRight_ = false;
	int  flyFrames_ = 0;
	DWORD deathTimer_ = 0;
	int  score_ = 0;
	double gravScale_ = 1.0;

	// 粒子
	std::vector<Particle>  particles_;
	std::vector<ScoreSpark> scoreSparks_;

	// 残影
	Vec2 trail_[12]; int trailHead_ = 0, trailCnt_ = 0;

	// 钩锁
	SurfaceSeg hookSeg_ = SurfaceSeg::L1_ROD;
	double hookT_ = 0.5, hookMaxLen_ = 0.0;
	Vec2 hookWorldPos_;
	bool ballCaptureOn_ = true;

	// 钩锁充能
	struct HookSlot { bool ready = true; float timer = 0.0f; };
	HookSlot slots_[2];
	static constexpr float HOOK_COOLDOWN = 1.0f;
	void updateCooldowns(double dt);
	void drawHUD() const;

	// 内部方法
	Vec2 footPos(const Pendulum& p, Vec2 pivot) const;
	Vec2 footVel(const Pendulum& p, Vec2 pivot) const;
	void warpTo(SurfaceSeg seg, double t);
	Vec2 surfacePosAt(SurfaceSeg seg, double t, const Pendulum& p, Vec2 pivot, double rodSide = 1.0) const;
	SurfacePoint findNearestSurface(const Pendulum& p, Vec2 pivot, Vec2 target) const;
	void resolveBallCollision(Vec2 ballCenter, double radius);
	void updateOnRod(double dt, const Pendulum& p, Vec2 pivot);
	void updateFly(double dt, const Pendulum& p, Vec2 pivot);
	void updateHooked(double dt, const Pendulum& p, Vec2 pivot);
	void updateDead(double dt, const Pendulum& p, Vec2 pivot);

	static constexpr double SLIDE_FACTOR = 0.25, ROD_TILT_THRESHOLD = 0.5236;
	static constexpr double JUMP_BOOST = 400.0, WALK_SPEED = 0.2, DAMPING = 0.85;
	static constexpr double ROD_OFFSET = 12.0, BALL_SLIDE_FACTOR = 0.0;
	static constexpr double GRAB_DISTANCE = 18.0, DEAD_Y = 1000.0;
	static constexpr double BALL_THROW_SPEED = 200.0, WARP_HYSTERESIS = 0.06;
	static constexpr double BALL_SAFE_ANGLE = 85.0 * M_PI / 180.0; // 球面安全行走角度(从球顶算起, ≈覆盖整个上半球)
	static constexpr int FLY_GRACE_FRAMES = 15, FLY_GRACE_BALL = 10;
	static constexpr double HOOK_MAX_RANGE = 400.0, HOOK_K = 7.0, HOOK_REST = 8.0;
	static constexpr double HOOK_MAX_STRETCH = 1.4, HOOK_SNAP_DIST = 5.0;

	Pendulum* pendulumRef_ = nullptr;
};
