#include "Player.h"
#include "Vec2.h"
#include "Physics.h"
#include "Pendulum.h"
#include <graphics.h>
#include <stdio.h>
#include <algorithm>

// ============================================================
// 射线检测工具
// ============================================================
namespace {

bool rayHitSegment(Vec2 O, Vec2 D, Vec2 A, Vec2 B, Vec2& hit, double& dist) {
	double dx = B.x - A.x, dy = B.y - A.y;
	double denom = dx * D.y - dy * D.x;
	if (fabs(denom) < 1e-9) return false;
	double t = (dy * (O.x - A.x) - dx * (O.y - A.y)) / denom;
	double s = ((O.x - A.x) * D.y - (O.y - A.y) * D.x) / denom;
	if (t < 0.0 || s < 0.0 || s > 1.0) return false;
	hit = O + D * t; dist = t;
	return true;
}

bool rayHitCircle(Vec2 O, Vec2 D, Vec2 C, double R, Vec2& hit, double& dist) {
	Vec2 OC = O - C;
	double a = D.x*D.x + D.y*D.y;
	double b = 2.0 * (OC.x*D.x + OC.y*D.y);
	double c = OC.x*OC.x + OC.y*OC.y - R*R;
	double disc = b*b - 4.0*a*c;
	if (disc < 0.0) return false;
	double t1 = (-b - sqrt(disc)) / (2.0*a);
	double t2 = (-b + sqrt(disc)) / (2.0*a);
	double t = (t1 > 0.0) ? t1 : ((t2 > 0.0) ? t2 : -1.0);
	if (t < 0.0) return false;
	hit = O + D * t; dist = t;
	return true;
}

} // anonymous

// ============================================================
// 坐标计算
// ============================================================
Vec2 Player::footPos(const Pendulum& p, Vec2 pivot) const {
	Vec2 joint = p.jointPos(pivot);
	Vec2 tip   = p.tipPos(pivot);
	switch (seg_) {
	case SurfaceSeg::L1_ROD: {
		double walkLen = p.params().L1 - p.params().R1;
		Vec2 center = pivot + Vec2::fromPolar(p.state().alpha1, t_ * walkLen);
		Vec2 offset = Vec2::fromPolar(p.state().alpha1 + M_PI / 2, ROD_OFFSET * rodSide_);
		return offset + center;
	}
	case SurfaceSeg::L2_ROD: {
		double walkLen = p.params().L2 - p.params().R1 - p.params().R2;
		Vec2 center = joint + Vec2::fromPolar(p.state().alpha2, p.params().R1 + t_ * walkLen);
		Vec2 offset = Vec2::fromPolar(p.state().alpha2 + M_PI / 2, ROD_OFFSET * rodSide_);
		return offset + center;
	}
	case SurfaceSeg::M1_BALL: {
		double a = M_PI / 2.0 + M_PI * t_;
		return joint + Vec2::fromPolar(a, p.params().R1);
	}
	case SurfaceSeg::M2_BALL: {
		double a = M_PI / 2.0 + M_PI * t_;
		return tip + Vec2::fromPolar(a, p.params().R2);
	}
	}
	return Vec2();
}

Vec2 Player::position() const { return pos_; }

Vec2 Player::surfacePosAt(SurfaceSeg seg, double t, const Pendulum& p, Vec2 pivot, double rodSide) const {
	Vec2 joint = p.jointPos(pivot), tip = p.tipPos(pivot);
	const auto& pr = p.params();
	switch (seg) {
	case SurfaceSeg::L1_ROD: {
		double walkLen = pr.L1 - pr.R1;
		Vec2 center = pivot + Vec2::fromPolar(p.state().alpha1, t * walkLen);
		return Vec2::fromPolar(p.state().alpha1 + M_PI / 2, ROD_OFFSET * rodSide) + center;
	}
	case SurfaceSeg::L2_ROD: {
		double walkLen = pr.L2 - pr.R1 - pr.R2;
		Vec2 center = joint + Vec2::fromPolar(p.state().alpha2, pr.R1 + t * walkLen);
		return Vec2::fromPolar(p.state().alpha2 + M_PI / 2, ROD_OFFSET * rodSide) + center;
	}
	case SurfaceSeg::M1_BALL: return joint + Vec2::fromPolar(M_PI/2.0 + M_PI*t, pr.R1);
	case SurfaceSeg::M2_BALL: return tip   + Vec2::fromPolar(M_PI/2.0 + M_PI*t, pr.R2);
	}
	return Vec2();
}

void Player::reset_Player(SurfaceSeg seg, double t) {
	state_ = State::ON_ROD; seg_ = seg; t_ = target_t_ = t;
	rodSide_ = 1.0; vel_ = Vec2(); flyFrames_ = 0;
}

void Player::warpTo(SurfaceSeg seg, double t) {
	seg_ = seg; t_ = target_t_ = t;
}

void Player::moveLeft()  { inputLeft_  = true; }
void Player::moveRight() { inputRight_ = true; }

// ============================================================
// 速度计算
// ============================================================
Vec2 Player::footVel(const Pendulum& p, Vec2 pivot) const {
	const auto& s = p.state(); const auto& pr = p.params();
	switch (seg_) {
	case SurfaceSeg::L1_ROD: {
		double r = t_ * (pr.L1 - pr.R1);
		return Vec2::fromPolar(s.alpha1 + M_PI/2, s.w1*r)
		     + Vec2::fromPolar(s.alpha1 + M_PI,   s.w1*ROD_OFFSET*rodSide_);
	}
	case SurfaceSeg::M1_BALL: {
		double a = M_PI/2.0 + M_PI*t_;
		return Vec2::fromPolar(s.alpha1 + M_PI/2, s.w1*pr.L1)
		     + Vec2::fromPolar(a + M_PI/2, s.w1*pr.R1);
	}
	case SurfaceSeg::L2_ROD: {
		double r = pr.R1 + t_*(pr.L2 - pr.R1 - pr.R2);
		return Vec2::fromPolar(s.alpha1 + M_PI/2, s.w1*pr.L1)
		     + Vec2::fromPolar(s.alpha2 + M_PI/2, s.w2*r)
		     + Vec2::fromPolar(s.alpha2 + M_PI,   s.w2*ROD_OFFSET*rodSide_);
	}
	case SurfaceSeg::M2_BALL: {
		double a = M_PI/2.0 + M_PI*t_;
		Vec2 jointVel = Vec2::fromPolar(s.alpha1 + M_PI/2, s.w1*pr.L1);
		Vec2 tipVel = jointVel + Vec2::fromPolar(s.alpha2 + M_PI/2, s.w2*pr.L2);
		return tipVel + Vec2::fromPolar(a + M_PI/2, s.w2*pr.R2);
	}
	}
	return Vec2();
}

// ============================================================
// 主更新
// ============================================================
void Player::update(double dt, Pendulum& p, Vec2 pivot) {
	pendulumRef_ = &p;
	updateCooldowns(dt);

	// 粒子更新
	for (auto& pt : particles_) {
		pt.pos.x += pt.vel.x * dt; pt.pos.y += pt.vel.y * dt;
		pt.vel.y += GRAVITY * dt; pt.life -= (float)dt;
	}
	particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
		[](const Particle& p){ return p.life <= 0; }), particles_.end());

	for (auto& sp : scoreSparks_) {
		sp.pos.x += sp.vel.x * dt; sp.pos.y += sp.vel.y * dt;
		sp.vel.y += GRAVITY * 0.3f * dt;
		sp.vel.x *= (float)(1.0 - 1.5*dt); sp.vel.y *= (float)(1.0 - 1.5*dt);
		sp.life -= (float)dt;
	}
	scoreSparks_.erase(std::remove_if(scoreSparks_.begin(), scoreSparks_.end(),
		[](const ScoreSpark& s){ return s.life <= 0; }), scoreSparks_.end());

	// 残影
	static int trailSkip = 0;
	if (++trailSkip >= 2) {
		trailSkip = 0;
		trail_[trailHead_] = pos_;
		trailHead_ = (trailHead_ + 1) % 12;
		if (trailCnt_ < 12) trailCnt_++;
	}

	switch (state_) {
	case State::ON_ROD: updateOnRod(dt, p, pivot); break;
	case State::FLY:    updateFly(dt, p, pivot);    break;
	case State::HOOKED: updateHooked(dt, p, pivot); break;
	case State::DEAD:   updateDead(dt, p, pivot);   break;
	}
}

// ============================================================
// ON_ROD 状态 (逻辑与原版完全一致)
// ============================================================
void Player::updateOnRod(double dt, const Pendulum& p, Vec2 pivot) {
	if (seg_ == SurfaceSeg::L1_ROD || seg_ == SurfaceSeg::L2_ROD) {
		double rodAngle = (seg_ == SurfaceSeg::L1_ROD) ? p.state().alpha1 : p.state().alpha2;
		double xSign = (sin(rodAngle) >= 0) ? 1.0 : -1.0;
		int dir = (inputRight_ ? 1 : 0) - (inputLeft_ ? 1 : 0);
		target_t_ += dir * xSign * WALK_SPEED * 1.5625 / 60.0;
		double rodLen = (seg_ == SurfaceSeg::L1_ROD)
			? p.params().L1 - p.params().R1
			: p.params().L2 - p.params().R1 - p.params().R2;
		double slide = Physics::rodSlideSpeed(rodAngle, ROD_TILT_THRESHOLD, SLIDE_FACTOR, GRAVITY);
		target_t_ += (slide / rodLen) * dt;
	} else {
		int dir = (inputRight_ ? 1 : 0) - (inputLeft_ ? 1 : 0);
		target_t_ -= dir * WALK_SPEED * 2.03125 / 60.0;
	}
	inputLeft_ = inputRight_ = false;

	if (target_t_ < 0.0) target_t_ = 0.0; if (target_t_ > 1.0) target_t_ = 1.0;
	t_ += (target_t_ - t_) * DAMPING;
	if (t_ < 0.0) t_ = 0.0; if (t_ > 1.0) t_ = 1.0;

	// 球面脱落
	if (seg_ == SurfaceSeg::M1_BALL || seg_ == SurfaceSeg::M2_BALL) {
		double omega = (seg_ == SurfaceSeg::M1_BALL) ? p.state().w1 : p.state().w2;
		double radius = (seg_ == SurfaceSeg::M1_BALL) ? p.params().R1 : p.params().R2;
		if (fabs(omega) * radius > BALL_THROW_SPEED) {
			int bIdx = (seg_ == SurfaceSeg::M1_BALL) ? 0 : 1;
			if (pendulumRef_) pendulumRef_->triggerEject(bIdx);
			vel_ = footVel(p, pivot);
			state_ = State::FLY; flyFrames_ = FLY_GRACE_FRAMES - FLY_GRACE_BALL;
			pos_ = footPos(p, pivot);
			Vec2 bc = (seg_ == SurfaceSeg::M1_BALL) ? p.jointPos(pivot) : p.tipPos(pivot);
			Vec2 out = pos_ - bc; double d = sqrt(out.x*out.x + out.y*out.y);
			if (d > 0.01) { pos_.x += out.x/d*6.0; pos_.y += out.y/d*6.0; vel_.x += out.x/d*250.0; vel_.y += out.y/d*250.0; }
			return;
		}
		const double SAFE_LO = 0.5 - BALL_SAFE_ANGLE / M_PI;
		const double SAFE_HI = 0.5 + BALL_SAFE_ANGLE / M_PI;
		if (t_ < SAFE_LO || t_ > SAFE_HI) {
			vel_ = footVel(p, pivot); state_ = State::FLY;
			flyFrames_ = FLY_GRACE_FRAMES - FLY_GRACE_BALL;
			pos_ = footPos(p, pivot);
			Vec2 bc = (seg_ == SurfaceSeg::M1_BALL) ? p.jointPos(pivot) : p.tipPos(pivot);
			Vec2 out = pos_ - bc; double d = sqrt(out.x*out.x + out.y*out.y);
			if (d > 0.01) { pos_.x += out.x/d*6.0; pos_.y += out.y/d*6.0; vel_.x += out.x/d*250.0; vel_.y += out.y/d*250.0; }
			return;
		}
	}

	pos_ = footPos(p, pivot);

	// 杆→球吸附
	if (seg_ == SurfaceSeg::L1_ROD || seg_ == SurfaceSeg::L2_ROD) {
		const double EDGE = 0.05, PROX = ROD_OFFSET + 8.0;
		const double SAFE_LO = 0.5 - BALL_SAFE_ANGLE / M_PI, SAFE_HI = 0.5 + BALL_SAFE_ANGLE / M_PI;
		Vec2 joint = p.jointPos(pivot), tip = p.tipPos(pivot);
		double r1 = p.params().R1, r2 = p.params().R2;

		auto trySuck = [&](Vec2 bc, SurfaceSeg ballSeg) {
			double dx = pos_.x - bc.x, dy = pos_.y - bc.y;
			double dist = sqrt(dx*dx+dy*dy);
			if (dist < 0.01) return;
			double a = atan2(dx, dy);
			while (a < M_PI/2.0) a += 2.0*M_PI;
			if (a > 3.0*M_PI/2.0) return;  // 下半球: 拒绝吸附
			double tBall = (a - M_PI/2.0) / M_PI;
			if (tBall < WARP_HYSTERESIS) tBall = WARP_HYSTERESIS;
			if (tBall > 1.0 - WARP_HYSTERESIS) tBall = 1.0 - WARP_HYSTERESIS;
			if (tBall < SAFE_LO || tBall > SAFE_HI) return;
			warpTo(ballSeg, tBall);
			pos_ = footPos(p, pivot);
		};

		auto nearBall = [&](Vec2 bc, double r) {
			double dx = pos_.x-bc.x, dy = pos_.y-bc.y;
			return sqrt(dx*dx+dy*dy) < r + PROX;
		};

		if (ballCaptureOn_) {
			double d1 = sqrt((pos_.x-joint.x)*(pos_.x-joint.x)+(pos_.y-joint.y)*(pos_.y-joint.y));
			if (d1 < r1 + PROX) trySuck(joint, SurfaceSeg::M1_BALL);
			double d2 = sqrt((pos_.x-tip.x)*(pos_.x-tip.x)+(pos_.y-tip.y)*(pos_.y-tip.y));
			if (d2 < r2 + PROX) trySuck(tip, SurfaceSeg::M2_BALL);
		} else {
			if (seg_ == SurfaceSeg::L1_ROD && t_ > 1.0 - EDGE) {
				trySuck(joint, SurfaceSeg::M1_BALL);
				if (nearBall(tip, r2)) trySuck(tip, SurfaceSeg::M2_BALL);
			}
			if (seg_ == SurfaceSeg::L2_ROD && t_ < EDGE) {
				trySuck(joint, SurfaceSeg::M1_BALL);
				if (nearBall(tip, r2)) trySuck(tip, SurfaceSeg::M2_BALL);
			}
			if (seg_ == SurfaceSeg::L2_ROD && t_ > 1.0 - EDGE) {
				trySuck(tip, SurfaceSeg::M2_BALL);
				if (nearBall(joint, r1)) trySuck(joint, SurfaceSeg::M1_BALL);
			}
		}
	}
}

// ============================================================
// FLY 状态
// ============================================================
void Player::updateFly(double dt, const Pendulum& p, Vec2 pivot) {
	vel_.y += GRAVITY * gravScale_ * dt;
	pos_.x += vel_.x * dt; pos_.y += vel_.y * dt;

	// 球体碰撞
	Vec2 joint = p.jointPos(pivot), tip = p.tipPos(pivot);
	resolveBallCollision(joint, p.params().R1);
	resolveBallCollision(tip, p.params().R2);

	if (pos_.y > DEAD_Y) { die(); return; }

	flyFrames_++;
	if (flyFrames_ > FLY_GRACE_FRAMES) {
		SurfacePoint sp = findNearestSurface(p, pivot, pos_);
		if (sp.distance < GRAB_DISTANCE) {
			Vec2 toSurface = sp.worldPos - pos_;
			double dot = vel_.x*toSurface.x + vel_.y*toSurface.y;
			if (dot > 0.0 || sp.distance < 4.0) {
				warpTo(sp.seg, sp.t); rodSide_ = sp.rodSide;
				if (seg_ == SurfaceSeg::M1_BALL || seg_ == SurfaceSeg::M2_BALL) {
					const double M = 0.06;
					if (t_ < M) t_ = target_t_ = M;
					if (t_ > 1.0-M) t_ = target_t_ = 1.0-M;
				} else if (seg_ == SurfaceSeg::L1_ROD) {
					if (t_ > 0.85) t_ = target_t_ = 0.85;
				} else if (seg_ == SurfaceSeg::L2_ROD) {
					if (t_ < 0.30) t_ = target_t_ = 0.30;
					if (t_ > 0.70) t_ = target_t_ = 0.70;
				}
				state_ = State::ON_ROD; vel_ = Vec2();
				pos_ = footPos(p, pivot);
				return;
			}
		}
	}
}

// ============================================================
// 跳跃
// ============================================================
void Player::jump(Pendulum& p, Vec2 pivot) {
	if (state_ == State::HOOKED) { releaseHook(); return; }
	if (state_ != State::ON_ROD) return;
	vel_ = footVel(p, pivot); vel_.y -= JUMP_BOOST;
	state_ = State::FLY;
	flyFrames_ = (seg_ == SurfaceSeg::M1_BALL || seg_ == SurfaceSeg::M2_BALL)
		? FLY_GRACE_FRAMES - FLY_GRACE_BALL : 0;
	pos_ = footPos(p, pivot);
	if (seg_ == SurfaceSeg::M1_BALL || seg_ == SurfaceSeg::M2_BALL) {
		int bIdx = (seg_ == SurfaceSeg::M1_BALL) ? 0 : 1;
		p.triggerJumpOff(bIdx);
		Vec2 bc = (seg_ == SurfaceSeg::M1_BALL) ? p.jointPos(pivot) : p.tipPos(pivot);
		Vec2 out = pos_ - bc; double d = sqrt(out.x*out.x+out.y*out.y);
		if (d > 0.01) { pos_.x += out.x/d*6.0; pos_.y += out.y/d*6.0; vel_.x += out.x/d*250.0; vel_.y += out.y/d*250.0; }
	}
	const double JUMP_NUDGE = 0.25;
	double sign = (pos_.x > pivot.x) ? -1.0 : 1.0;
	if (seg_ == SurfaceSeg::L1_ROD || seg_ == SurfaceSeg::M1_BALL) p.nudgeW1(sign*JUMP_NUDGE);
	else p.nudgeW2(sign*JUMP_NUDGE);
}

// ============================================================
// 钩锁 (逻辑完全保留)
// ============================================================
void Player::hook(Vec2 mouseWorld, Pendulum& p, Vec2 pivot) {
	if (state_ != State::FLY) return;
	int slot = -1;
	for (int i = 0; i < 2; i++) if (slots_[i].ready) { slot = i; break; }
	if (slot < 0) return;
	slots_[slot].ready = false; slots_[slot].timer = HOOK_COOLDOWN;

	Vec2 dir = mouseWorld - pos_;
	double len = sqrt(dir.x*dir.x + dir.y*dir.y);
	if (len < 1.0) return;
	dir = dir / len;

	double bestDist = HOOK_MAX_RANGE;
	bool hitAny = false;
	SurfaceSeg bestSeg = SurfaceSeg::L1_ROD;
	double bestT = 0.5;

	Vec2 joint = p.jointPos(pivot), tip = p.tipPos(pivot);
	double R1 = p.params().R1, R2 = p.params().R2;
	Vec2 rodN1 = Vec2::fromPolar(p.state().alpha1 + M_PI/2, ROD_OFFSET);
	Vec2 rodN2 = Vec2::fromPolar(p.state().alpha2 + M_PI/2, ROD_OFFSET);

	for (int side = -1; side <= 1; ++side) {
		Vec2 off = rodN1 * (double)side;
		Vec2 A = pivot+off, B = joint+off;
		Vec2 hp; double d;
		if (rayHitSegment(pos_, dir, A, B, hp, d) && d < bestDist) {
			bestDist = d; bestSeg = SurfaceSeg::L1_ROD;
			double distOnRod = sqrt((hp.x-A.x)*(hp.x-A.x)+(hp.y-A.y)*(hp.y-A.y));
			double walkLen = p.params().L1 - p.params().R1;
			bestT = (walkLen > 0.0) ? (distOnRod/walkLen) : 0.0;
			if (bestT < 0.0) bestT = 0.0; if (bestT > 1.0) bestT = 1.0;
			hitAny = true;
		}
	}
	for (int side = -1; side <= 1; ++side) {
		Vec2 off = rodN2 * (double)side;
		Vec2 A = joint+off, B = tip+off;
		Vec2 hp; double d;
		if (rayHitSegment(pos_, dir, A, B, hp, d) && d < bestDist) {
			bestDist = d; bestSeg = SurfaceSeg::L2_ROD;
			double distOnRod = sqrt((hp.x-A.x)*(hp.x-A.x)+(hp.y-A.y)*(hp.y-A.y));
			double walkLen = p.params().L2 - p.params().R1 - p.params().R2;
			bestT = (walkLen > 0.0) ? ((distOnRod - p.params().R1)/walkLen) : 0.0;
			if (bestT < 0.0) bestT = 0.0; if (bestT > 1.0) bestT = 1.0;
			hitAny = true;
		}
	}
	{ Vec2 hp; double d;
		if (rayHitCircle(pos_, dir, joint, R1, hp, d) && d < bestDist) {
			double a = atan2(hp.x-joint.x, hp.y-joint.y);
			while (a < M_PI/2.0) a += 2.0*M_PI;
			if (a <= 3.0*M_PI/2.0) {
				bestDist = d; bestSeg = SurfaceSeg::M1_BALL;
				bestT = (a - M_PI/2.0)/M_PI;
				if (bestT < 0.0) bestT = 0.0; if (bestT > 1.0) bestT = 1.0;
				hitAny = true;
			}
		}
	}
	{ Vec2 hp; double d;
		if (rayHitCircle(pos_, dir, tip, R2, hp, d) && d < bestDist) {
			double a = atan2(hp.x-tip.x, hp.y-tip.y);
			while (a < M_PI/2.0) a += 2.0*M_PI;
			if (a <= 3.0*M_PI/2.0) {
				bestDist = d; bestSeg = SurfaceSeg::M2_BALL;
				bestT = (a - M_PI/2.0)/M_PI;
				if (bestT < 0.0) bestT = 0.0; if (bestT > 1.0) bestT = 1.0;
				hitAny = true;
			}
		}
	}

	if (!hitAny) return;

	hookSeg_ = bestSeg; hookT_ = bestT;
	hookMaxLen_ = bestDist * HOOK_MAX_STRETCH;
	hookWorldPos_ = surfacePosAt(bestSeg, bestT, p, pivot);
	state_ = State::HOOKED;

	const double HOOK_NUDGE = 0.12;
	if (bestSeg == SurfaceSeg::M1_BALL) p.nudgeW1(dir.x * HOOK_NUDGE);
	else if (bestSeg == SurfaceSeg::M2_BALL) p.nudgeW2(dir.x * HOOK_NUDGE);

	if (bestSeg == SurfaceSeg::M1_BALL || bestSeg == SurfaceSeg::M2_BALL)
		p.triggerHookHit(bestSeg == SurfaceSeg::M1_BALL ? 0 : 1);
}

void Player::releaseHook() {
	state_ = State::FLY; vel_.y -= JUMP_BOOST; flyFrames_ = 0;
}

// ============================================================
// 球体碰撞
// ============================================================
void Player::resolveBallCollision(Vec2 ballCenter, double radius) {
	double dx = pos_.x - ballCenter.x, dy = pos_.y - ballCenter.y;
	double dist = sqrt(dx*dx + dy*dy);
	if (dist >= radius || dist < 0.01) return;
	Vec2 normal(dx/dist, dy/dist);
	double penetration = radius - dist;
	double push = penetration; if (push > 8.0) push = 8.0;
	pos_.x += normal.x * push; pos_.y += normal.y * push;
	double vn = vel_.x*normal.x + vel_.y*normal.y;
	if (vn >= 0.0) return;
	if (dy > 0.0) {
		vel_.x -= 2.0*normal.x*vn; vel_.y -= 2.0*normal.y*vn;
		vel_.x *= 1.5; vel_.y *= 1.5;
	} else {
		vel_.x -= normal.x*vn; vel_.y -= normal.y*vn;
	}
}

// ============================================================
// HOOKED / DEAD / 平台
// ============================================================
void Player::updateHooked(double dt, const Pendulum& p, Vec2 pivot) {
	hookWorldPos_ = surfacePosAt(hookSeg_, hookT_, p, pivot);
	Vec2 rope = hookWorldPos_ - pos_;
	double dist = sqrt(rope.x*rope.x + rope.y*rope.y);
	if (dist > 0.01) {
		Vec2 ropeDir = rope / dist;
		if (dist > HOOK_REST) {
			double force = HOOK_K * (dist - HOOK_REST);
			vel_.x += ropeDir.x*force*dt; vel_.y += ropeDir.y*force*dt;
		}
		if (dist > hookMaxLen_) {
			pos_ = hookWorldPos_ - ropeDir * hookMaxLen_;
			double vDot = vel_.x*ropeDir.x + vel_.y*ropeDir.y;
			if (vDot > 0.0) { vel_.x -= ropeDir.x*vDot; vel_.y -= ropeDir.y*vDot; }
		}
	}
	vel_.y += GRAVITY*gravScale_*dt;
	pos_.x += vel_.x*dt; pos_.y += vel_.y*dt;

	Vec2 joint = p.jointPos(pivot), tip = p.tipPos(pivot);
	resolveBallCollision(joint, p.params().R1);
	resolveBallCollision(tip, p.params().R2);

	if (pos_.y > DEAD_Y) { die(); return; }

	SurfacePoint sp = findNearestSurface(p, pivot, pos_);
	if (sp.distance < HOOK_SNAP_DIST) {
		warpTo(sp.seg, sp.t);
		if (sp.seg == SurfaceSeg::L1_ROD || sp.seg == SurfaceSeg::L2_ROD) {
			double rodA = (sp.seg == SurfaceSeg::L1_ROD) ? p.state().alpha1 : p.state().alpha2;
			Vec2 normal = Vec2::fromPolar(rodA + M_PI/2, 1.0);
			Vec2 center = surfacePosAt(sp.seg, sp.t, p, pivot, 0.0);
			rodSide_ = ((pos_.x-center.x)*normal.x + (pos_.y-center.y)*normal.y >= 0) ? 1.0 : -1.0;
		} else rodSide_ = sp.rodSide;
		if (seg_ == SurfaceSeg::M1_BALL || seg_ == SurfaceSeg::M2_BALL) {
			const double M = 0.06;
			if (t_ < M) t_ = target_t_ = M;
			if (t_ > 1.0-M) t_ = target_t_ = 1.0-M;
		} else if (seg_ == SurfaceSeg::L1_ROD) {
			if (t_ > 0.85) t_ = target_t_ = 0.85;
		} else if (seg_ == SurfaceSeg::L2_ROD) {
			if (t_ < 0.30) t_ = target_t_ = 0.30;
			if (t_ > 0.70) t_ = target_t_ = 0.70;
		}
		state_ = State::ON_ROD; vel_ = Vec2(); pos_ = footPos(p, pivot);
	}
}

void Player::die() {
	if (state_ == State::DEAD) return;
	state_ = State::DEAD; deathTimer_ = GetTickCount64();
	if (pendulumRef_) pendulumRef_->triggerDeath();
	particles_.clear();
	for (int i = 0; i < 12; i++) {
		Particle p; p.pos = pos_;
		double ang = (rand()%360)*M_PI/180.0;
		p.vel = Vec2(cos(ang)*(80+rand()%200), sin(ang)*(80+rand()%200));
		p.life = 0.4f;
		particles_.push_back(p);
	}
}

void Player::emitScoreSparks() {
	constexpr int N = 12;
	for (int i = 0; i < N; i++) {
		ScoreSpark sp;
		double ang = (2.0*M_PI*i)/N + (rand()%20-10)*0.01;
		sp.pos = pos_ + Vec2::fromPolar(ang, 4.0);
		double spd = 140.0 + (rand()%120);
		sp.vel = Vec2::fromPolar(ang, spd);
		sp.life = 0.35f + (rand()%150)/1000.0f;
		sp.maxLife = sp.life;
		scoreSparks_.push_back(sp);
	}
}

void Player::landOnPlatform(double standY) {
	if (state_ != State::FLY) return;
	pos_.y = standY; vel_.y = 0.0;
}

void Player::updateDead(double dt, const Pendulum& p, Vec2 pivot) {
	(void)dt;
	if (GetTickCount64() - deathTimer_ > 500) {
		reset_Player(); pos_ = footPos(p, pivot);
	}
}

// ============================================================
// 最近表面查找
// ============================================================
SurfacePoint Player::findNearestSurface(const Pendulum& p, Vec2 pivot, Vec2 target) const {
	SurfacePoint best; best.distance = 1e9; best.seg = SurfaceSeg::L2_ROD; best.t = 0.5;
	const auto& s = p.state(); const auto& pr = p.params();
	Vec2 joint = p.jointPos(pivot), tip = p.tipPos(pivot);

	// M1
	{
		double dx = target.x - joint.x, dy = target.y - joint.y;
		double dc = sqrt(dx*dx+dy*dy);
		if (dc > 0.0) {
			double a = atan2(dx, dy);
			while (a < M_PI/2.0) a += 2.0*M_PI;
			if (a <= 3.0*M_PI/2.0) {
				double tv = (a - M_PI/2.0)/M_PI;
				Vec2 surf = joint + Vec2::fromPolar(a, pr.R1);
				double d = sqrt((target.x-surf.x)*(target.x-surf.x)+(target.y-surf.y)*(target.y-surf.y));
				if (d < best.distance) { best.distance=d; best.seg=SurfaceSeg::M1_BALL; best.t=tv; best.worldPos=surf; }
			}
		}
	}
	// M2
	{
		double dx = target.x - tip.x, dy = target.y - tip.y;
		double dc = sqrt(dx*dx+dy*dy);
		if (dc > 0.0) {
			double a = atan2(dx, dy);
			while (a < M_PI/2.0) a += 2.0*M_PI;
			if (a <= 3.0*M_PI/2.0) {
				double tv = (a - M_PI/2.0)/M_PI;
				Vec2 surf = tip + Vec2::fromPolar(a, pr.R2);
				double d = sqrt((target.x-surf.x)*(target.x-surf.x)+(target.y-surf.y)*(target.y-surf.y));
				if (d < best.distance) { best.distance=d; best.seg=SurfaceSeg::M2_BALL; best.t=tv; best.worldPos=surf; }
			}
		}
	}
	// L1
	{
		double dx = joint.x-pivot.x, dy = joint.y-pivot.y;
		double lenSq = dx*dx+dy*dy;
		if (lenSq > 0.0) {
			double inv=1.0/sqrt(lenSq), tp = ((target.x-pivot.x)*dx+(target.y-pivot.y)*dy)/lenSq;
			if (tp<0) tp=0; if (tp>1) tp=1;
			double nx=-dy*inv, ny=dx*inv;
			double wl = pr.L1-pr.R1;
			if (wl > 0.0) {
				double wt = (tp*pr.L1)/wl;
				if (wt>=0 && wt<=1) {
					for (int side=-1; side<=1; side+=2) {
						Vec2 surf(pivot.x+tp*dx+nx*ROD_OFFSET*side, pivot.y+tp*dy+ny*ROD_OFFSET*side);
						double d = sqrt((target.x-surf.x)*(target.x-surf.x)+(target.y-surf.y)*(target.y-surf.y));
						if (d < best.distance) { best.distance=d; best.seg=SurfaceSeg::L1_ROD; best.t=wt; best.worldPos=surf; best.rodSide=(double)side; }
					}
				}
			}
		}
	}
	// L2
	{
		double dx = tip.x-joint.x, dy = tip.y-joint.y;
		double lenSq = dx*dx+dy*dy;
		if (lenSq > 0.0) {
			double inv=1.0/sqrt(lenSq), tp = ((target.x-joint.x)*dx+(target.y-joint.y)*dy)/lenSq;
			if (tp<0) tp=0; if (tp>1) tp=1;
			double nx=-dy*inv, ny=dx*inv;
			double wl = pr.L2-pr.R1-pr.R2;
			if (wl > 0.0) {
				double wt = (tp*pr.L2-pr.R1)/wl;
				if (wt>=0 && wt<=1) {
					for (int side=-1; side<=1; side+=2) {
						Vec2 surf(joint.x+tp*dx+nx*ROD_OFFSET*side, joint.y+tp*dy+ny*ROD_OFFSET*side);
						double d = sqrt((target.x-surf.x)*(target.x-surf.x)+(target.y-surf.y)*(target.y-surf.y));
						if (d < best.distance) { best.distance=d; best.seg=SurfaceSeg::L2_ROD; best.t=wt; best.worldPos=surf; best.rodSide=(double)side; }
					}
				}
			}
		}
	}
	return best;
}

// ============================================================
// 充能更新 + HUD
// ============================================================
void Player::updateCooldowns(double dt) {
	for (int i = 0; i < 2; i++) {
		if (slots_[i].ready) continue;
		slots_[i].timer -= (float)dt;
		if (slots_[i].timer <= 0.0f) { slots_[i].ready = true; slots_[i].timer = 0.0f; }
	}
}

void Player::drawHUD() const {
	const int X = 16, Y = 16, W = 18, H = 28, GAP = 5;
	for (int i = 0; i < 2; i++) {
		if (slots_[i].ready) {
			setfillcolor(RGB(0, 220, 80));
			solidrectangle(X+i*(W+GAP), Y, X+i*(W+GAP)+W, Y+H);
		} else {
			setfillcolor(RGB(40, 40, 50));
			solidrectangle(X+i*(W+GAP), Y, X+i*(W+GAP)+W, Y+H);
			float pct = 1.0f - slots_[i].timer / HOOK_COOLDOWN;
			int fh = (int)(H * pct);
			setfillcolor(RGB(0, 180, 60));
			solidrectangle(X+i*(W+GAP), Y+H-fh, X+i*(W+GAP)+W, Y+H);
		}
		setlinecolor(RGB(100, 100, 120));
		rectangle(X+i*(W+GAP), Y, X+i*(W+GAP)+W, Y+H);
	}

	// 分数 (3×5点阵)
	static const bool DIGITS[10][5][3] = {
		{{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}},
		{{0,0,1},{0,0,1},{0,0,1},{0,0,1},{0,0,1}},
		{{1,1,1},{0,0,1},{1,1,1},{1,0,0},{1,1,1}},
		{{1,1,1},{0,0,1},{1,1,1},{0,0,1},{1,1,1}},
		{{1,0,1},{1,0,1},{1,1,1},{0,0,1},{0,0,1}},
		{{1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1}},
		{{1,1,1},{1,0,0},{1,1,1},{1,0,1},{1,1,1}},
		{{1,1,1},{0,0,1},{0,0,1},{0,0,1},{0,0,1}},
		{{1,1,1},{1,0,1},{1,1,1},{1,0,1},{1,1,1}},
		{{1,1,1},{1,0,1},{1,1,1},{0,0,1},{1,1,1}},
	};
	int num = score_, cx = 80, cy = 56, dot = 3, gap = 2;
	if (num == 0) {
		for (int row=0; row<5; row++) for (int col=0; col<3; col++)
			if (DIGITS[0][row][col]) {
				setfillcolor(RGB(255,255,100));
				solidrectangle(cx+col*(dot+1), cy+row*(dot+1), cx+col*(dot+1)+dot, cy+row*(dot+1)+dot);
			}
	} else {
		char buf[12]; int len = 0;
		while (num > 0) { buf[len++] = '0' + (num%10); num/=10; }
		for (int i=len-1; i>=0; i--) {
			int dg = buf[i]-'0';
			for (int row=0; row<5; row++) for (int col=0; col<3; col++)
				if (DIGITS[dg][row][col]) {
					setfillcolor(RGB(255,255,100));
					solidrectangle(cx+col*(dot+1), cy+row*(dot+1), cx+col*(dot+1)+dot, cy+row*(dot+1)+dot);
				}
			cx += 3*(dot+1) + gap;
		}
	}

	// 重力条
	int gx = 16, gy = 80, gw = 40, gh = 6;
	double gt = (gravScale_ - 0.1) / 2.9;
	int gr = (int)(255*gt), gg = (int)(255*(1.0-gt));
	setfillcolor(RGB(gr, gg, 60));
	solidrectangle(gx, gy, gx+(int)(gw*gt+2), gy+gh);
	setlinecolor(RGB(100, 100, 120));
	rectangle(gx, gy, gx+gw, gy+gh);
}

// ============================================================
// 绘制角色 (简化版)
// ============================================================
void Player::draw() const {
	int cx = (int)pos_.x, cy = (int)pos_.y - 6;

	// ── 状态色 ──
	COLORREF color;
	if (state_ == State::ON_ROD)      color = RGB(0, 255, 100);
	else if (state_ == State::FLY)    color = RGB(255, 200, 0);
	else if (state_ == State::HOOKED) color = RGB(100, 180, 255);
	else {
		if (((GetTickCount64() - deathTimer_) / 80) % 2 == 0) {
			color = RGB(255, 50, 50);
		} else {
			drawHUD(); return;  // 死亡闪烁暗帧
		}
	}

	// ── 菱形主体 (纯色, 无呼吸光晕) ──
	int R = 7;
	int rr = GetRValue(color), rg = GetGValue(color), rb = GetBValue(color);
	for (int y = -R; y <= R; y++) {
		int hw = R - (y < 0 ? -y : y);
		if (hw < 1) continue;
		float t = 1.0f - (float)(y < 0 ? -y : y) / (float)R;
		float brightness = 0.4f + t * 0.6f;
		int cr = (int)(rr * brightness), cg = (int)(rg * brightness), cb = (int)(rb * brightness);
		setlinecolor(RGB(cr, cg, cb));
		line(cx - hw, cy + y, cx + hw, cy + y);
	}

	// ── 中心高光点 ──
	setfillcolor(RGB(255, 255, 255));
	solidrectangle(cx - 1, cy - 1, cx + 1, cy + 1);

	// ── 钩锁绳 (单线, 宽度随张力变化) ──
	if (state_ == State::HOOKED) {
		double dx = hookWorldPos_.x - pos_.x, dy = hookWorldPos_.y - pos_.y;
		double dist = sqrt(dx*dx+dy*dy);
		if (dist > 0) {
			double tension = (dist - HOOK_REST) / (hookMaxLen_ - HOOK_REST);
			if (tension < 0) tension = 0; if (tension > 1) tension = 1;
			int alpha = (int)(80 + tension * 175);
			int cr = (int)(160 + tension * 95), cg = (int)(180 + tension * 75), cb = (int)(100 + tension * 80);
			if (cr>255) cr=255; if (cg>255) cg=255; if (cb>255) cb=255;
			setlinecolor(RGB(cr, cg, cb));
			line((int)pos_.x, (int)pos_.y, (int)hookWorldPos_.x, (int)hookWorldPos_.y);
			// 粗线: 并行偏移
			double nx = -dy/dist, ny = dx/dist;
			int w = (int)(1 + tension * 2);
			for (int k = -w/2; k <= w/2; k++) {
				line((int)(pos_.x+nx*k), (int)(pos_.y+ny*k),
				     (int)(hookWorldPos_.x+nx*k), (int)(hookWorldPos_.y+ny*k));
			}
		}
	}

	// ── 死亡粒子 ──
	for (const auto& p : particles_) {
		float t = p.life / 0.4f;
		int cr = 255, cg = (int)(50*t), cb = (int)(50*t);
		setfillcolor(RGB(cr, cg, cb));
		solidrectangle((int)p.pos.x-1, (int)p.pos.y-1, (int)p.pos.x+1, (int)p.pos.y+1);
	}

	// ── 得分离子 ──
	for (const auto& sp : scoreSparks_) {
		float t = sp.life / sp.maxLife;
		int sz = (t > 0.6f) ? 3 : (t > 0.3f) ? 2 : 1;
		int a = (int)(t * 255);
		setfillcolor(RGB(255, (int)(180+(1-t)*75), (int)(80+(1-t)*140)));
		solidrectangle((int)sp.pos.x-sz/2, (int)sp.pos.y-sz/2, (int)sp.pos.x+sz/2, (int)sp.pos.y+sz/2);
	}

	// ── 运动残影 ──
	if (trailCnt_ >= 2 && state_ != State::HOOKED) {
		for (int i = 0; i < trailCnt_ - 1; i++) {
			int a = (trailHead_ - 1 - i + 12) % 12;
			float t = 1.0f - (float)i / trailCnt_;
			int alpha = (int)(t * 120);
			if (state_ == State::ON_ROD)
				setfillcolor(RGB(0, alpha/2, alpha/3));
			else if (state_ == State::FLY)
				setfillcolor(RGB(alpha, alpha*3/4, alpha/4));
			int rr = (int)(t * 3 + 1);
			solidcircle((int)trail_[a].x, (int)trail_[a].y - 6, rr);
		}
	}

	drawHUD();
}
