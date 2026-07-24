#include "Physics.h"
#include <iostream>
#include <cmath>
#include "SDL.h"
// 默认配置
// struct State {
//     double alpha1 = 0, alpha2 = 0;   // 角度 (rad)
//     double w1 = 0, w2 = 0;   // 角速度 (rad/s)
//
//     // 关节球心 = pivot + 沿 th1 方向走 L1
//     Vec2 tip1(Vec2 pivot, double L1) const;
//
//     // 末端球心 = tip1 + 沿 th2 方向走 L2
//     Vec2 tip2(Vec2 pivot, double L1, double L2) const;
// };
// struct Params {
//     double L1 = 150;    // 第一杆长 (px)
//     double L2 = 120;    // 第二杆长 (px)
//     double m1 = 2.0;    // 关节球质量
//     double m2 = 1.5;    // 末端球质量
//     double g = 600;    // 重力加速度 (px/s²)
//     double R1 = 15;     // 关节球绘制半径 (px)
//     double R2 = 12;     // 末端球绘制半径 (px)
// };

/*Δ = θ₁ - θ₂
D = 2·m₁ + m₂ - m₂·cos(2·Δ)

分子₁ = -g·(2·m₁+m₂)·sin(θ₁)  -  m₂·g·sin(θ₁-2·θ₂)
       - 2·sin(Δ)·m₂·(ω₂²·L₂ + ω₁²·L₁·cos(Δ))

分子₂ = 2·sin(Δ)·( ω₁²·L₁·(m₁+m₂)
                 + g·(m₁+m₂)·cos(θ₁)
                 + ω₂²·L₂·m₂·cos(Δ) )

a₁ = 分子₁ / (L₁ · D)
a₂ = 分子₂ / (L₂ · D)
*/
void Physics::acceleration(const State &s, const Params &p, double &a1, double &a2)
{
    double delta = s.alpha1 - s.alpha2;
    double denom = 2 * p.M1 + p.M2 - p.M2 * cos(2 * delta);

    double num1 = -p.g * (2 * p.M1 + p.M2) * sin(s.alpha1) - p.M2 * p.g * sin(s.alpha1 - 2 * s.alpha2) - 2 * sin(delta) * p.M2 * (s.w2 * s.w2 * p.L2 + s.w1 * s.w1 * p.L1 * cos(delta));

    double num2 = 2 * sin(delta) * (s.w1 * s.w1 * p.L1 * (p.M1 + p.M2) + p.g * (p.M1 + p.M2) * cos(s.alpha1) + s.w2 * s.w2 * p.L2 * p.M2 * cos(delta));

    a1 = num1 / (p.L1 * denom);
    a2 = num2 / (p.L2 * denom);
}
void Physics::step(State &s, const Params &p, double dt)
{
    double a1, a2;
    acceleration(s, p, a1, a2);
    // 先速度，后位置（Symplectic Euler 的定义）!!!维护能量守恒

    s.w1 += a1 * dt;
    s.w2 += a2 * dt;
    s.alpha1 += s.w1 * dt;
    s.alpha2 += s.w2 * dt;

    fold(s.alpha1);
    fold(s.alpha2);
}
// 子步进, 用于积分
void Physics::substep(State &s, const Params &p,
                      double dt, int n)
{
    double h = dt / n;
    for (int i = 0; i < n; i++)
        step(s, p, h);
}
// 角度折叠, 确保在 [-π, π] 范围内
void Physics::fold(double &a)
{
    if (a > M_PI)
        a -= 2 * M_PI;
    if (a < -M_PI)
        a += 2 * M_PI;
}

// 动能: T = ½(m₁+m₂)L₁²ω₁² + ½m₂L₂²ω₂² + m₂L₁L₂ω₁ω₂cos(θ₁-θ₂)
double Physics::kinetic(const State &s, const Params &p)
{
    double L1 = p.L1, L2 = p.L2;
    double T = 0.5 * (p.M1 + p.M2) * L1*L1 * s.w1 * s.w1
             + 0.5 * p.M2 * L2*L2 * s.w2 * s.w2
             + p.M2 * L1 * L2 * s.w1 * s.w2 * cos(s.alpha1 - s.alpha2);
    return T;
}
// 势能: V = -g·L₁·(m₁+m₂)·cosθ₁ - g·m₂·L₂·cosθ₂  (支点为零势面)
double Physics::potential(const State &s, const Params &p)
{
    return -p.g * p.L1 * (p.M1 + p.M2) * cos(s.alpha1)
           - p.g * p.M2 * p.L2 * cos(s.alpha2);
}

// Vec2 State::tip1(Vec2 pivot, double L1) const {
//     return pivot + Vec2(L1 * cos(alpha1), L1 * sin(alpha1));
// }
//
// Vec2 State::tip2(Vec2 pivot, double L1, double L2) const {
//     return tip1(pivot, L1) + Vec2(L2 * cos(alpha2), L2 * sin(alpha2));
// }
//  杆面下滑速度 (t单位/s), 正=向t增大方向
double Physics::rodSlideSpeed(double theta, double tiltThreshold,
                               double slideFactor, double g)
{
    double tilt = fabs(M_PI / 2 - theta);
    if (tilt <= tiltThreshold) return 0.0;
    double dir = (theta < M_PI / 2) ? 1.0 : -1.0;
    return dir * g * sin(tilt) * slideFactor;
}

double Physics::ballSurfaceSpeed(double omega, double radius)
{
    return fabs(omega * radius);
}

double Physics::ballAngleFromTop(Vec2 charPos, Vec2 ballCenter)
{
    Vec2 toChar = charPos - ballCenter;
    return fabs(atan2(toChar.x, toChar.y));
}
