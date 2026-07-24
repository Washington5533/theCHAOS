#include "Pendulum.h"
#include "ball_lighting.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

// ============================================================
// 基础
// ============================================================
Pendulum::Pendulum() { reset(); }

void Pendulum::reset() {
	state_.alpha1 = M_PI / 2; state_.alpha2 = M_PI / 2;
	state_.w1 = 0; state_.w2 = 0;
	for (int i = 0; i < 2; i++) { feedback_[i] = BallFeedback(); trail_[i].clear(); }
	sparks_.clear();
}

void Pendulum::step(double dt, int substeps) { Physics::substep(state_, params_, dt, substeps); }

Vec2 Pendulum::jointPos(Vec2 pivot) const { return state_.tip1(pivot, params_.L1); }
Vec2 Pendulum::tipPos(Vec2 pivot)   const { return state_.tip2(pivot, params_.L1, params_.L2); }

void Pendulum::setPreset(int idx) {
	static const Physics::State presets[] = {
		{M_PI/2, M_PI/2, 0, 0}, {2.0, 0.2, 0, 0}, {3.0, -1.5, 0, 0},
		{M_PI/2, -M_PI/2, 0, 0}, {1.2, 1.8, 0, 0},
	};
	state_ = presets[idx % 5];
}

// ============================================================
// 颜色工具
// ============================================================
COLORREF Pendulum::lerpColor(COLORREF a, COLORREF b, double t) {
	if (t < 0.0) t = 0.0; if (t > 1.0) t = 1.0;
	return RGB(
		(int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
		(int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
		(int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t)
	);
}

COLORREF Pendulum::computeBallColor(int ballIdx, double omega) const {
	double absOmega = fabs(omega);
	double maxOmega = 8.0;
	double t = absOmega / maxOmega;
	if (t > 1.0) t = 1.0;

	// 霓虹配色: 更鲜艳、更高饱和度，甩出时保持饱和不发白
	COLORREF cold, hot, whiteHot;
	if (ballIdx == 0) {
		cold = RGB(180, 20, 20);    // 霓虹红 (暗)
		hot  = RGB(255, 50, 50);    // 霓虹红 (亮)
		whiteHot = RGB(255, 80, 80); // 高速仍保持饱和红
	} else {
		cold = RGB(20, 60, 220);    // 霓虹蓝 (暗)
		hot  = RGB(80, 180, 255);   // 霓虹蓝 (亮)
		whiteHot = RGB(100, 200, 255); // 高速仍保持饱和蓝
	}
	if (t < 0.7) return lerpColor(cold, hot, t / 0.7);
	else         return lerpColor(hot, whiteHot, (t - 0.7) / 0.3);
}

// ============================================================
// 填充圆辅助
// ============================================================
void Pendulum::fillCircle(int cx, int cy, int r, COLORREF c) {
	setfillcolor(c);
	for (int y = -r; y <= r; y++) {
		int w = (int)std::sqrt((double)(r * r - y * y));
		solidrectangle(cx - w, cy + y, cx + w, cy + y);
	}
}

// ============================================================
// 反馈更新
// ============================================================
void Pendulum::updateFeedback(double dt) {
	for (int i = 0; i < 2; i++) {
		feedback_[i].landPulse  *= exp(-dt * 12.0);
		feedback_[i].hookFlash  *= exp(-dt * 18.0);
		feedback_[i].jumpRipple *= exp(-dt * 8.0);
		feedback_[i].ejectGlow  *= exp(-dt * 14.0);
		feedback_[i].deathBurst *= exp(-dt * 8.0);
		feedback_[i].scorePulse *= exp(-dt * 14.0);
		if (feedback_[i].landPulse  < 0.001) feedback_[i].landPulse  = 0.0;
		if (feedback_[i].hookFlash  < 0.001) feedback_[i].hookFlash  = 0.0;
		if (feedback_[i].jumpRipple < 0.001) feedback_[i].jumpRipple = 0.0;
		if (feedback_[i].ejectGlow  < 0.001) feedback_[i].ejectGlow  = 0.0;
		if (feedback_[i].deathBurst < 0.001) feedback_[i].deathBurst = 0.0;
		if (feedback_[i].scorePulse < 0.001) feedback_[i].scorePulse = 0.0;
	}
}

// ============================================================
// 霓虹发光球体渲染
// ============================================================
void Pendulum::drawSphere(int cx, int cy, int radius, COLORREF color, double omega,
                          BallFeedback& fb, int ballIdx, DWORD frame)
{
	// 1. 霓虹呼吸 (仅用于亮核亮度, 不影响半径)
	double neonPulse = 0.85 + sin(frame * 0.08 + ballIdx * 3.14) * 0.15;

	// 2. 反馈变形
	double scaleX = 1.0, scaleY = 1.0;
	double pulseAdd = fb.landPulse * 6.0 + fb.scorePulse * 5.0;
	if (fb.jumpRipple > 0.01) {
		double squash = 1.0 - fb.jumpRipple * 0.08;
		scaleY *= squash; scaleX *= (1.0 / sqrt(squash));
	}
	double absOmega = fabs(omega);
	if (absOmega > 5.0) {
		double flat = (absOmega - 5.0) * 0.004;
		if (flat > 0.04) flat = 0.04;
		scaleY *= (1.0 - flat); scaleX *= (1.0 + flat * 0.5);
	}

	int rx = (int)(radius * scaleX + pulseAdd);
	int ry = (int)(radius * scaleY + pulseAdd);
	if (rx < 2) rx = 2; if (ry < 2) ry = 2;

	// 3. 死亡红色调
	int baseR = GetRValue(color), baseG = GetGValue(color), baseB = GetBValue(color);
	if (fb.deathBurst > 0.01) {
		double t = fb.deathBurst;
		baseR = (int)(baseR + (255 - baseR) * t);
		baseG = (int)(baseG * (1.0 - t));
		baseB = (int)(baseB * (1.0 - t));
	}

	// 4. 钩中闪光提亮
	int brightBoost = 0;
	if (fb.hookFlash > 0.01) brightBoost = (int)(fb.hookFlash * 80);

	// ─ 球体边缘固定光晕 (不随帧变化, 避免闪烁) ──
	// 仅在球半径变化时绘制, 用固定alpha避免每帧颜色跳变
	{
		int rimR = rx + 2;
		int gr = (int)(baseR * 0.3);
		int gg = (int)(baseG * 0.3);
		int gb = (int)(baseB * 0.3);
		if (gr < 5 && baseR > 30) gr = 5;
		if (gg < 5 && baseG > 30) gg = 5;
		if (gb < 5 && baseB > 30) gb = 5;
		setlinecolor(RGB(gr, gg, gb));
		circle(cx, cy, rimR);
		circle(cx, cy, rimR + 1);
	}

	// 5. 选择预计算亮度数据
	const unsigned char* brightArr;
	int brightSize;
	if (ballIdx == 0) { brightArr = BALL1_BRIGHT; brightSize = 102; }
	else              { brightArr = BALL2_BRIGHT; brightSize = 82; }
	int lookupR = (ballIdx == 0) ? 50 : 40;

	// 6. 逐行渲染 (亮度 × 颜色 + 霓虹增强)
	for (int y = -ry; y <= ry; y++) {
		int hw = (int)((double)rx / ry * sqrt((double)(ry*ry - y*y)));
		if (hw < 1) continue;

		// 映射到查找表行
		int ly = (int)((double)y / ry * lookupR + lookupR + 1 + 0.5);
		if (ly < 0) ly = 0;  if (ly >= brightSize) ly = brightSize - 1;

		// 取该行中心亮度
		unsigned char bright = brightArr[ly * brightSize + lookupR + 1];

		// 霓虹增强: 整体提亮 + 边缘rim light
		double distFromEdge = 1.0 - (double)abs(y) / ry; // 离边缘越近值越小
		double rimFactor = pow(1.0 - distFromEdge, 4.0) * 0.25; // 边缘微亮
		double neonBright = (double)bright / 255.0;
		// 提升整体亮度 (x1.25) + 轻微边缘光
		neonBright = neonBright * 1.25 + rimFactor;
		if (neonBright > 1.0) neonBright = 1.0;
		int finalBright = (int)(neonBright * 255);

		int cr = (baseR * finalBright) >> 8;
		int cg = (baseG * finalBright) >> 8;
		int cb = (baseB * finalBright) >> 8;
		cr += brightBoost; cg += brightBoost; cb += brightBoost;
		if (cr > 255) cr = 255; if (cg > 255) cg = 255; if (cb > 255) cb = 255;

		setlinecolor(RGB(cr, cg, cb));
		line(cx - hw, cy + y, cx + hw, cy + y);
	}

	// 7. 内部亮核 (能量核心效果)
	{
		int coreR = (int)(rx * 0.25);
		if (coreR >= 1) {
			double coreAlpha = 0.5 * neonPulse;
			int cr = (int)(baseR * coreAlpha + 255 * (1.0 - coreAlpha));
			int cg = (int)(baseG * coreAlpha + 255 * (1.0 - coreAlpha));
			int cb = (int)(baseB * coreAlpha + 255 * (1.0 - coreAlpha));
			if (cr > 255) cr = 255; if (cg > 255) cg = 255; if (cb > 255) cb = 255;
			setfillcolor(RGB(cr, cg, cb));
			for (int y = -coreR; y <= coreR; y++) {
				int w = (int)sqrt((double)(coreR * coreR - y * y));
				if (w < 1) continue;
				solidrectangle(cx - w, cy + y, cx + w, cy + y);
			}
		}
	}

	// 8. 甩出白光边缘 (同心圆模拟)
	if (fb.ejectGlow > 0.01) {
		int glowR = (int)(rx * (1.0 + fb.ejectGlow * 0.5));
		int factor = (int)(fb.ejectGlow * 180);
		if (factor > 255) factor = 255;
		setlinecolor(RGB(factor, factor, factor));
		circle(cx, cy, glowR);
		circle(cx, cy, glowR + 1);
	}

	// 9. 死亡粒子 (红色迸溅)
	if (fb.deathBurst > 0.05 && (rand() % 3 == 0)) {
		SparkParticle sp;
		double ang = (rand() % 360) * M_PI / 180.0;
		sp.pos = Vec2(cx + cos(ang) * rx, cy + sin(ang) * ry);
		double spd = 60 + (rand() % 100);
		sp.vel = Vec2(cos(ang) * spd, sin(ang) * spd);
		sp.life = 0.2f + (rand() % 150) / 1000.0f;
		sp.maxLife = sp.life;
		sp.color = RGB(255, 40 + rand() % 60, 0);
		sparks_.push_back(sp);
	}
}

// ============================================================
// 运动残影 (6帧简化)
// ============================================================
void Pendulum::recordTrail(Vec2 joint, Vec2 tip) {
	double surf1 = fabs(state_.w1) * params_.R1;
	double surf2 = fabs(state_.w2) * params_.R2;
	constexpr double THROW = 200.0;
	if (surf1 > THROW) trail_[0].record(joint); else trail_[0].decay();
	if (surf2 > THROW) trail_[1].record(tip);   else trail_[1].decay();
}

void Pendulum::drawTrail(int ballIdx, int radius, COLORREF base) {
	const BallTrail& tr = trail_[ballIdx];
	if (tr.count < 2) return;

	double surfSpeed = (ballIdx == 0) ? fabs(state_.w1) * params_.R1 : fabs(state_.w2) * params_.R2;
	double intensity = std::min((surfSpeed - 200.0) / 200.0, 1.0);
	if (intensity < 0.0) intensity = 0.0;

	int ir = GetRValue(base), ig = GetGValue(base), ib = GetBValue(base);

	for (int i = 0; i < tr.count; i++) {
		int idx = (tr.head - 1 - i + 6) % 6;
		double t = 1.0 - (double)i / 6.0;
		double alpha = t * (0.15 + 0.5 * intensity);  // 用暗度模拟透明度
		int rr = (int)(radius * t * (0.3 + 0.5 * intensity));
		if (rr < 2) continue;
		int cr = (int)(ir * alpha), cg = (int)(ig * alpha), cb = (int)(ib * alpha);
		if (cr > 255) cr = 255; if (cg > 255) cg = 255; if (cb > 255) cb = 255;
		fillCircle((int)tr.positions[idx].x, (int)tr.positions[idx].y, rr, RGB(cr, cg, cb));
	}
}

// ============================================================
// 火花粒子 (简化)
// ============================================================
void Pendulum::emitSparks(Vec2 center, int radius, double omega, COLORREF color) {
	double absOmega = fabs(omega);
	// 降低阈值, 让低速也有粒子
	if (absOmega < 1.5) return;
	// 粒子数量随速度增加 (1~8个)
	int count = (int)((absOmega - 1.5) * 1.2);
	if (count < 1) count = 1;
	if (count > 8) count = 8;
	for (int i = 0; i < count; i++) {
		// 更频繁发射
		if (rand() % 2 != 0 && count > 2) continue;
		SparkParticle sp;
		double ang = (rand() % 360) * M_PI / 180.0;
		sp.pos = center + Vec2::fromPolar(ang, radius);
		double tangentAng = ang + M_PI / 2;
		double spd = 30 + (rand() % 60);
		double dir = (omega > 0) ? 1.0 : -1.0;
		sp.vel = Vec2::fromPolar(tangentAng, spd * dir) + Vec2::fromPolar(ang, 15 + (rand() % 25));
		sp.life = 0.4f + (rand() % 250) / 1000.0f;
		sp.maxLife = sp.life;
		sp.color = color;
		sparks_.push_back(sp);
	}
}

void Pendulum::updateSparks(double dt) {
	for (auto& sp : sparks_) {
		sp.pos.x += sp.vel.x * dt; sp.pos.y += sp.vel.y * dt;
		sp.vel.y += 100.0 * dt;
		sp.life -= (float)dt;
	}
	sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(),
		[](const SparkParticle& s) { return s.life <= 0; }), sparks_.end());
}

void Pendulum::drawSparks() {
	for (const auto& sp : sparks_) {
		float t = sp.life / sp.maxLife;
		int cr = (int)(GetRValue(sp.color) * t);
		int cg = (int)(GetGValue(sp.color) * t);
		int cb = (int)(GetBValue(sp.color) * t * 0.6);
		int sz = (t > 0.5f) ? 3 : (t > 0.25f) ? 2 : 1;
		setfillcolor(RGB(cr, cg, cb));
		solidrectangle((int)sp.pos.x - sz/2, (int)sp.pos.y - sz/2,
		               (int)sp.pos.x + sz/2, (int)sp.pos.y + sz/2);
	}
}

// ============================================================
// 触发接口
// ============================================================
void Pendulum::triggerLand(int ballIdx) {
	if (ballIdx >= 0 && ballIdx < 2) feedback_[ballIdx].landPulse = 1.0;
}
void Pendulum::triggerHookHit(int ballIdx) {
	if (ballIdx >= 0 && ballIdx < 2) {
		feedback_[ballIdx].hookFlash = 1.0;
	}
}
void Pendulum::triggerJumpOff(int ballIdx) {
	if (ballIdx >= 0 && ballIdx < 2) feedback_[ballIdx].jumpRipple = 1.0;
}
void Pendulum::triggerEject(int ballIdx) {
	if (ballIdx >= 0 && ballIdx < 2) feedback_[ballIdx].ejectGlow = 1.0;
}
void Pendulum::triggerDeath() {
	for (int i = 0; i < 2; i++) feedback_[i].deathBurst = 1.0;
}
void Pendulum::triggerScore() {
	for (int i = 0; i < 2; i++) { feedback_[i].scorePulse = 1.0; feedback_[i].hookFlash = 0.5; }
}

// ============================================================
// 主绘制
// ============================================================
void Pendulum::draw(Vec2 pivot, DWORD frame)
{
	Vec2 joint = jointPos(pivot);
	Vec2 tip   = tipPos(pivot);

	// ── 杆: 纯色实线 ──
	setlinecolor(RGB(180, 180, 220));
	line((int)pivot.x, (int)pivot.y, (int)joint.x, (int)joint.y);
	line((int)joint.x, (int)joint.y, (int)tip.x, (int)tip.y);

	// ── 粗杆（画2条并行线模拟） ──
	{
		double d1x = joint.x - pivot.x, d1y = joint.y - pivot.y;
		double l1 = sqrt(d1x*d1x + d1y*d1y); if (l1 < 1) l1 = 1;
		double nx1 = d1y / l1, ny1 = -d1x / l1;
		line((int)(pivot.x + nx1), (int)(pivot.y + ny1), (int)(joint.x + nx1), (int)(joint.y + ny1));
		line((int)(pivot.x - nx1), (int)(pivot.y - ny1), (int)(joint.x - nx1), (int)(joint.y - ny1));

		double d2x = tip.x - joint.x, d2y = tip.y - joint.y;
		double l2 = sqrt(d2x*d2x + d2y*d2y); if (l2 < 1) l2 = 1;
		double nx2 = d2y / l2, ny2 = -d2x / l2;
		line((int)(joint.x + nx2), (int)(joint.y + ny2), (int)(tip.x + nx2), (int)(tip.y + ny2));
		line((int)(joint.x - nx2), (int)(joint.y - ny2), (int)(tip.x - nx2), (int)(tip.y - ny2));
	}

	// ── 更新所有效果 ──
	double dt = 1.0 / 60.0;
	updateFeedback(dt);
	updateSparks(dt);
	recordTrail(joint, tip);

	// ── 计算颜色 ──
	COLORREF col1 = computeBallColor(0, state_.w1);
	COLORREF col2 = computeBallColor(1, state_.w2);

	// ── 绘制顺序: 残影 → 球体 → 支点 → 火花 ──
	drawTrail(0, (int)params_.R1, col1);
	drawTrail(1, (int)params_.R2, col2);

	// 支点
	fillCircle((int)pivot.x, (int)pivot.y, 4, RGB(255, 255, 100));

	// M1 球体
	drawSphere((int)joint.x, (int)joint.y, (int)params_.R1, col1, state_.w1, feedback_[0], 0, frame);
	// M2 球体
	drawSphere((int)tip.x, (int)tip.y, (int)params_.R2, col2, state_.w2, feedback_[1], 1, frame);

	// 火花
	emitSparks(joint, (int)params_.R1, state_.w1, col1);
	emitSparks(tip,   (int)params_.R2, state_.w2, col2);
	drawSparks();
}
