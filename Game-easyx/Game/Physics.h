#pragma once
#include "Vec2.h"
#include <cmath>

constexpr double GRAVITY = 600.0;  // 重力加速度 (px/s²)

class Physics {
public:
	struct State {
		double alpha1 = 0, alpha2 = 0;  // 摆角 rad
		double w1 = 0, w2 = 0;           // 角速度 rad/s

		Vec2 tip1(Vec2 pivot, double L1) const {
			return pivot + Vec2(L1 * sin(alpha1), L1 * cos(alpha1));
		}
		Vec2 tip2(Vec2 pivot, double L1, double L2) const {
			return tip1(pivot, L1) + Vec2(L2 * sin(alpha2), L2 * cos(alpha2));
		}
	};

	struct Params {
		double L1 = 225, L2 = 180;
		double M1 = 2.0, M2 = 1.5;
		double g  = GRAVITY;
		double R1 = 50,  R2 = 40;
	};

	static void acceleration(const State& s, const Params& p, double& a1, double& a2);
	static void step(State& s, const Params& p, double dt);
	static void substep(State& s, const Params& p, double dt, int n = 8);
	static void fold(double& a);
	static double kinetic(const State& s, const Params& p);
	static double potential(const State& s, const Params& p);
	static double rodSlideSpeed(double theta, double tiltThreshold, double slideFactor, double g);
	static double ballSurfaceSpeed(double omega, double radius);
	static double ballAngleFromTop(Vec2 charPos, Vec2 ballCenter);
};
