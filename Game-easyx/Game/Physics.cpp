#include "Physics.h"
#include <iostream>
#include <cmath>

// 拉格朗日双摆方程
void Physics::acceleration(const State& s, const Params& p, double& a1, double& a2) {
	double delta = s.alpha1 - s.alpha2;
	double denom = 2 * p.M1 + p.M2 - p.M2 * cos(2 * delta);

	double num1 = -p.g * (2 * p.M1 + p.M2) * sin(s.alpha1)
		- p.M2 * p.g * sin(s.alpha1 - 2 * s.alpha2)
		- 2 * sin(delta) * p.M2 * (s.w2 * s.w2 * p.L2 + s.w1 * s.w1 * p.L1 * cos(delta));

	double num2 = 2 * sin(delta) * (s.w1 * s.w1 * p.L1 * (p.M1 + p.M2)
		+ p.g * (p.M1 + p.M2) * cos(s.alpha1)
		+ s.w2 * s.w2 * p.L2 * p.M2 * cos(delta));

	a1 = num1 / (p.L1 * denom);
	a2 = num2 / (p.L2 * denom);
}

// Symplectic Euler 积分（先速度后位置，能量守恒好）
void Physics::step(State& s, const Params& p, double dt) {
	double a1, a2;
	acceleration(s, p, a1, a2);
	s.w1 += a1 * dt;
	s.w2 += a2 * dt;
	s.alpha1 += s.w1 * dt;
	s.alpha2 += s.w2 * dt;
	fold(s.alpha1);
	fold(s.alpha2);
}

void Physics::substep(State& s, const Params& p, double dt, int n) {
	double h = dt / n;
	for (int i = 0; i < n; i++) step(s, p, h);
}

void Physics::fold(double& a) {
	if (a > M_PI)      a -= 2 * M_PI;
	if (a < -M_PI)     a += 2 * M_PI;
}

double Physics::kinetic(const State& s, const Params& p) {
	double L1 = p.L1, L2 = p.L2;
	return 0.5 * (p.M1 + p.M2) * L1*L1 * s.w1 * s.w1
		+ 0.5 * p.M2 * L2*L2 * s.w2 * s.w2
		+ p.M2 * L1 * L2 * s.w1 * s.w2 * cos(s.alpha1 - s.alpha2);
}

double Physics::potential(const State& s, const Params& p) {
	return -p.g * p.L1 * (p.M1 + p.M2) * cos(s.alpha1)
		- p.g * p.M2 * p.L2 * cos(s.alpha2);
}

double Physics::rodSlideSpeed(double theta, double tiltThreshold, double slideFactor, double g) {
	double tilt = fabs(M_PI / 2 - theta);
	if (tilt <= tiltThreshold) return 0.0;
	double dir = (theta < M_PI / 2) ? 1.0 : -1.0;
	return dir * g * sin(tilt) * slideFactor;
}

double Physics::ballSurfaceSpeed(double omega, double radius) {
	return fabs(omega * radius);
}

double Physics::ballAngleFromTop(Vec2 charPos, Vec2 ballCenter) {
	Vec2 toChar = charPos - ballCenter;
	return fabs(atan2(toChar.x, toChar.y));
}
