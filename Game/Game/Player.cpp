#include "player.h"
#include "Vec2.h"
#include "Physics.h"
#include "Pendulum.h"
#include "SDL.h"
#include <stdio.h>
#include <algorithm>

// ============================================================
// CP5: 射线检测自由函数 (匿名namespace)
// ============================================================
namespace {

// 射线-线段求交: O + t*D = A + s*(B-A), t>=0, s∈[0,1]
bool rayHitSegment(Vec2 O, Vec2 D, Vec2 A, Vec2 B,
                   Vec2 &hit, double &dist) {
    double dx = B.x - A.x, dy = B.y - A.y;
    double denom = dx * D.y - dy * D.x;  // cross(AB, D)
    if (fabs(denom) < 1e-9) return false;

    double t = (dy * (O.x - A.x) - dx * (O.y - A.y)) / denom;
    double s = ((O.x - A.x) * D.y - (O.y - A.y) * D.x) / denom;
    if (t < 0.0 || s < 0.0 || s > 1.0) return false;

    hit = O + D * t;
    dist = t;
    return true;
}

// 射线-圆求交: |O + t*D - C|² = R²
bool rayHitCircle(Vec2 O, Vec2 D, Vec2 C, double R,
                  Vec2 &hit, double &dist) {
    Vec2 OC = O - C;
    double a = D.x * D.x + D.y * D.y;
    double b = 2.0 * (OC.x * D.x + OC.y * D.y);
    double c = OC.x * OC.x + OC.y * OC.y - R * R;
    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return false;

    double t1 = (-b - sqrt(disc)) / (2.0 * a);
    double t2 = (-b + sqrt(disc)) / (2.0 * a);
    double t = (t1 > 0.0) ? t1 : ((t2 > 0.0) ? t2 : -1.0);
    if (t < 0.0) return false;

    hit = O + D * t;
    dist = t;
    return true;
}

} // anonymous namespace

Vec2 Player::footPos(const Pendulum &p, Vec2 pivot) const
{

    // 球坐标
    Vec2 joint = p.jointPos(pivot);
    Vec2 tip = p.tipPos(pivot);
    switch (seg_)
    {
    case SurfaceSeg::L1_ROD:
    {
        // t=0: 支点; t=1: m1球表面 (L1-R1 处)
        double walkLen = p.params().L1 - p.params().R1;
        Vec2 center = pivot + Vec2::fromPolar(p.state().alpha1, t_ * walkLen);
        Vec2 offset = Vec2::fromPolar(p.state().alpha1 + M_PI / 2, ROD_OFFSET * rodSide_);
        return offset + center;
    }
    case SurfaceSeg::L2_ROD:
    {
        // t=0: m1球表面; t=1: m2球表面
        double walkLen = p.params().L2 - p.params().R1 - p.params().R2;
        Vec2 center = joint + Vec2::fromPolar(p.state().alpha2,
                        p.params().R1 + t_ * walkLen);
        Vec2 offset = Vec2::fromPolar(p.state().alpha2 + M_PI / 2, ROD_OFFSET * rodSide_);
        return offset + center;
    }
    case SurfaceSeg::M1_BALL:
    {
        // 固定上半球弧: a∈[π/2, 3π/2], t=0→右, t=0.5→顶, t=1→左
        double a = M_PI / 2.0 + M_PI * t_;
        return joint + Vec2::fromPolar(a, p.params().R1);
    }
    case SurfaceSeg::M2_BALL:
    {
        // 固定上半球弧: 同M1
        double a = M_PI / 2.0 + M_PI * t_;
        return tip + Vec2::fromPolar(a, p.params().R2);
    }
    default:
        break;
    }
    return Vec2();  // 安全兜底
}

// ============================================================
// CP5: 计算任意段/参数的表面世界坐标 (参数化版footPos)
// ============================================================
Vec2 Player::surfacePosAt(SurfaceSeg seg, double t,
                          const Pendulum &p, Vec2 pivot, double rodSide) const {
    Vec2 joint = p.jointPos(pivot);
    Vec2 tip   = p.tipPos(pivot);
    const auto &pr = p.params();
    switch (seg) {
    case SurfaceSeg::L1_ROD: {
        double walkLen = pr.L1 - pr.R1;
        Vec2 center = pivot + Vec2::fromPolar(p.state().alpha1, t * walkLen);
        Vec2 offset = Vec2::fromPolar(p.state().alpha1 + M_PI / 2, ROD_OFFSET * rodSide);
        return offset + center;
    }
    case SurfaceSeg::L2_ROD: {
        double walkLen = pr.L2 - pr.R1 - pr.R2;
        Vec2 center = joint + Vec2::fromPolar(p.state().alpha2,
                        pr.R1 + t * walkLen);
        Vec2 offset = Vec2::fromPolar(p.state().alpha2 + M_PI / 2, ROD_OFFSET * rodSide);
        return offset + center;
    }
    case SurfaceSeg::M1_BALL: {
        double a = M_PI / 2.0 + M_PI * t;
        return joint + Vec2::fromPolar(a, pr.R1);
    }
    case SurfaceSeg::M2_BALL: {
        double a = M_PI / 2.0 + M_PI * t;
        return tip + Vec2::fromPolar(a, pr.R2);
    }
    }
    return Vec2();
}

Vec2 Player::position() const { return pos_; }

void Player::reset_Player(SurfaceSeg seg, double t) {
    state_ = State::ON_ROD;
    seg_   = seg;
    t_     = target_t_ = t;
    rodSide_ = 1.0;
    vel_   = Vec2();
    flyFrames_ = 0;
}

void Player::update(double dt, Pendulum &p, Vec2 pivot)
{
    pendulumRef_ = &p;  // CP8: 缓存引用供 warpTo 触发反馈
    updateCooldowns(dt);  // CP6: 钩锁冷却计时

    // 粒子更新
    for (auto& pt : particles_) {
        pt.pos.x += pt.vel.x * dt;
        pt.pos.y += pt.vel.y * dt;
        pt.vel.y += GRAVITY * dt;
        pt.life -= (float)dt;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
        [](const Particle& p){ return p.life <= 0; }), particles_.end());

    // CP8: 得分离子粒子更新
    for (auto& sp : scoreSparks_) {
        sp.pos.x += sp.vel.x * dt;
        sp.pos.y += sp.vel.y * dt;
        sp.vel.y += GRAVITY * 0.3 * dt;  // 轻微重力
        sp.vel.x *= (float)(1.0 - 1.5 * dt);  // 空气阻力
        sp.vel.y *= (float)(1.0 - 1.5 * dt);
        sp.life -= (float)dt;
    }
    scoreSparks_.erase(std::remove_if(scoreSparks_.begin(), scoreSparks_.end(),
        [](const ScoreSpark& s){ return s.life <= 0; }), scoreSparks_.end());

    // 残影记录 (每 2 帧, 16帧缓冲≈0.5s)
    static int trailSkip = 0;
    if (++trailSkip >= 2) {
        trailSkip = 0;
        trail_[trailHead_] = pos_;
        trailHead_ = (trailHead_ + 1) % 16;
        if (trailCnt_ < 16) trailCnt_++;
    }

    switch (state_)
    {
    case State::ON_ROD:
        updateOnRod(dt, p, pivot);
        break;
    case State::FLY:
        updateFly(dt, p, pivot);
        break;
    case State::HOOKED:
        updateHooked(dt, p, pivot);
        break;
    case State::DEAD:
        updateDead(dt, p, pivot);
        break;
    }
}

void Player::updateOnRod(double dt, const Pendulum &p, Vec2 pivot)
{
    // 1. A/D 移动 + 表面下滑
    if (seg_ == SurfaceSeg::L1_ROD || seg_ == SurfaceSeg::L2_ROD) {
        // === 杆上 ===
        double rodAngle = (seg_ == SurfaceSeg::L1_ROD)
                              ? p.state().alpha1 : p.state().alpha2;
        double xSign = (sin(rodAngle) >= 0) ? 1.0 : -1.0;
        int dir = (inputRight_ ? 1 : 0) - (inputLeft_ ? 1 : 0);
        target_t_ += dir * xSign * WALK_SPEED * 0.8 / 60.0;

        double rodLen = (seg_ == SurfaceSeg::L1_ROD)
            ? p.params().L1 - p.params().R1
            : p.params().L2 - p.params().R1 - p.params().R2;
        double slide = Physics::rodSlideSpeed(rodAngle,
                         ROD_TILT_THRESHOLD, SLIDE_FACTOR, GRAVITY);
        target_t_ += (slide / rodLen) * dt;
        // CP4: 杆上甩出暂禁用 (ROD_THROW_ENABLED = false)
    }
    else {
        // === 球面 (M1_BALL / M2_BALL) — t_驱动, 固定上半球弧 ===
        // 弧: a=π/2+π·t_, t=0→右, t=0.5→顶, t=1→左
        // D(右)=减t, A(左)=增t, 恒定方向, 避免cos端点飘移
        int dir = (inputRight_ ? 1 : 0) - (inputLeft_ ? 1 : 0);
        target_t_ -= dir * WALK_SPEED / 60.0;

        // 球面下滑 (BALL_SLIDE_FACTOR=0 时跳过)
        if (BALL_SLIDE_FACTOR > 0.0) {
            Vec2 ballCenter = (seg_ == SurfaceSeg::M1_BALL)
                ? p.jointPos(pivot) : p.tipPos(pivot);
            Vec2 curPos = footPos(p, pivot);
            Vec2 toPlayer = curPos - ballCenter;
            double a2 = atan2(toPlayer.x, toPlayer.y);
            double angleRange = M_PI;
            target_t_ += -BALL_SLIDE_FACTOR * GRAVITY
                         * sin(a2) / angleRange * dt;
        }
    }
    inputLeft_ = inputRight_ = false;

    // 2. 段边界: 溢出即卡住 (四段互相隔离, 跨段仅钩锁/FLY snap)
    //    → 走到段边缘 t=0 或 t=1 就停住, 球面会触发角度脱落

    // 3. 阻尼平滑 + clamp
    if (target_t_ < 0.0) target_t_ = 0.0;
    if (target_t_ > 1.0) target_t_ = 1.0;
    t_ += (target_t_ - t_) * DAMPING;
    if (t_ < 0.0) t_ = 0.0;
    if (t_ > 1.0) t_ = 1.0;

    // 4. CP4: 球面脱落检测 (速度优先, 杆上不触发)
    if (seg_ == SurfaceSeg::M1_BALL || seg_ == SurfaceSeg::M2_BALL) {
        double omega = (seg_ == SurfaceSeg::M1_BALL)
            ? p.state().w1 : p.state().w2;
        double radius = (seg_ == SurfaceSeg::M1_BALL)
            ? p.params().R1 : p.params().R2;
        // 速度甩出: 球面线速度 |ω|×R > BALL_THROW_SPEED → 甩出
        if (fabs(omega) * radius > BALL_THROW_SPEED) {
            int bIdx = (seg_ == SurfaceSeg::M1_BALL) ? 0 : 1;
            if (pendulumRef_) pendulumRef_->triggerEject(bIdx);  // CP8: 甩出爆发反馈
            vel_ = footVel(p, pivot);
            state_ = State::FLY;
            flyFrames_ = FLY_GRACE_FRAMES - FLY_GRACE_BALL;
            pos_ = footPos(p, pivot);
            // pos向外推 + vel加径向分量, 确保脱离球面不被snap吸回
            { Vec2 bc = (seg_ == SurfaceSeg::M1_BALL)
                  ? p.jointPos(pivot) : p.tipPos(pivot);
              Vec2 out = pos_ - bc; double d = sqrt(out.x*out.x + out.y*out.y);
              if (d > 0.01) {
                  double nx = out.x/d, ny = out.y/d;
                  pos_.x += nx * 6.0;  pos_.y += ny * 6.0;
                  vel_.x += nx * 250.0; vel_.y += ny * 250.0;  // 径向脱离速度
              } }
            return;
        }

        // 角度脱落: 切面与水平夹角 > 80° 时滑落 (M1/M2 通用)
        // 球面弧: a = π/2 + π·t, 径向与竖直夹角 = |π·t - π/2|
        // 80° = 4π/9 → 安全区: |t - 0.5| < 4/9 → t ∈ [0.056, 0.944]
        const double ANGLE_SAFE_LO = 0.5 - 4.0 / 9.0;   // ≈ 0.056
        const double ANGLE_SAFE_HI = 0.5 + 4.0 / 9.0;   // ≈ 0.944
        if (t_ < ANGLE_SAFE_LO || t_ > ANGLE_SAFE_HI) {
            vel_ = footVel(p, pivot);
            state_ = State::FLY;
            flyFrames_ = FLY_GRACE_FRAMES - FLY_GRACE_BALL;
            pos_ = footPos(p, pivot);
            // pos向外推 + vel加径向分量, 确保脱离球面
            { Vec2 bc = (seg_ == SurfaceSeg::M1_BALL)
                  ? p.jointPos(pivot) : p.tipPos(pivot);
              Vec2 out = pos_ - bc; double d = sqrt(out.x*out.x + out.y*out.y);
              if (d > 0.01) {
                  double nx = out.x/d, ny = out.y/d;
                  pos_.x += nx * 6.0;  pos_.y += ny * 6.0;
                  vel_.x += nx * 250.0; vel_.y += ny * 250.0;
              } }
            return;
        }
    }

    // 5. 缓存世界坐标
    pos_ = footPos(p, pivot);

    // 6. 杆→球单向过渡 (OFF=杆端检测, ON=全程距离检测, F键切换)
    if (seg_ == SurfaceSeg::L1_ROD || seg_ == SurfaceSeg::L2_ROD) {
        const double EDGE    = 0.05;
        const double SAFE_LO = 0.5 - 4.0 / 9.0;
        const double SAFE_HI = 0.5 + 4.0 / 9.0;
        const double PROX    = ROD_OFFSET + 8.0;
        Vec2 joint = p.jointPos(pivot);
        Vec2 tip   = p.tipPos(pivot);
        double r1  = p.params().R1;
        double r2  = p.params().R2;

        auto trySuck = [&](Vec2 ballCenter, SurfaceSeg ballSeg) {
            double dx = pos_.x - ballCenter.x;
            double dy = pos_.y - ballCenter.y;
            double dist = sqrt(dx*dx + dy*dy);
            if (dist < 0.01) return;
            double a = atan2(dx, dy);
            // 归一化到 [-π/2, 3π/2]
            while (a < -M_PI/2.0) a += 2.0*M_PI;
            while (a > 3.0*M_PI/2.0) a -= 2.0*M_PI;
            // 下半球拒绝: 球面弧仅覆盖上半球 [π/2, 3π/2]
            if (a < M_PI/2.0 || a > 3.0*M_PI/2.0) return;
            double tBall = (a - M_PI/2.0) / M_PI;
            if (tBall < WARP_HYSTERESIS) tBall = WARP_HYSTERESIS;
            if (tBall > 1.0 - WARP_HYSTERESIS) tBall = 1.0 - WARP_HYSTERESIS;
            if (tBall < SAFE_LO || tBall > SAFE_HI) return;
            printf("[suck] seg=%d t=%.3f → ball=%d tBall=%.3f\n",
                   (int)seg_, t_, (int)ballSeg, tBall);
            warpTo(ballSeg, tBall);
            pos_ = footPos(p, pivot);
        };

        if (ballCaptureOn_) {
            // ── ON: 全程距离检测 ──
            double d1 = sqrt((pos_.x-joint.x)*(pos_.x-joint.x)+(pos_.y-joint.y)*(pos_.y-joint.y));
            if (d1 < r1 + PROX) trySuck(joint, SurfaceSeg::M1_BALL);
            double d2 = sqrt((pos_.x-tip.x)*(pos_.x-tip.x)+(pos_.y-tip.y)*(pos_.y-tip.y));
            if (d2 < r2 + PROX) trySuck(tip, SurfaceSeg::M2_BALL);
        } else {
            // ── OFF: 仅杆端检测 ──
            auto nearBall = [&](Vec2 bc, double r) {
                double dx = pos_.x - bc.x, dy = pos_.y - bc.y;
                return sqrt(dx*dx + dy*dy) < r + PROX;
            };
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

void Player::updateFly(double dt, const  Pendulum &p, Vec2 pivot)
{
    // 1. 重力 + 位移
    vel_.y += GRAVITY * gravScale_ * dt;
    pos_.x += vel_.x * dt;
    pos_.y += vel_.y * dt;

    // 2. CP5: 球体碰撞 (下半区硬反弹, 上半区软推出)
    {
        Vec2 joint = p.jointPos(pivot);
        Vec2 tip   = p.tipPos(pivot);
        resolveBallCollision(joint, p.params().R1);
        resolveBallCollision(tip,   p.params().R2);
    }

    // 3. CP4: 死亡判定 —— 飞出屏幕底部
    if (pos_.y > DEAD_Y) {
        die();
        return;
    }

    // 3. CP4: 抓回摆锤检测 (跳跃保护期内不抓)
    flyFrames_++;
    if (flyFrames_ > FLY_GRACE_FRAMES) {
        SurfacePoint sp = findNearestSurface(p, pivot, pos_);
        if (sp.distance < GRAB_DISTANCE) {
            // 仅当玩家朝表面移动时才抓回 (点积 > 0)
            Vec2 toSurface = sp.worldPos - pos_;
            double dot = vel_.x * toSurface.x + vel_.y * toSurface.y;
            if (dot > 0.0 || sp.distance < 4.0) {
                printf("[SNAP-fly] flyFrames=%d thr=%d → seg=%d t=%.4f dist=%.1f dot=%.1f\n",
                       flyFrames_, (int)FLY_GRACE_FRAMES, (int)sp.seg, sp.t, sp.distance, dot);
                warpTo(sp.seg, sp.t);
                rodSide_ = sp.rodSide;
                // 防杆端→球→杆端振荡: snap后钳制t远离杆端
                if (seg_ == SurfaceSeg::M1_BALL || seg_ == SurfaceSeg::M2_BALL) {
                    const double M = 0.06;  // 匹配80安全区 [0.056,0.944]
                    if (t_ < M) t_ = target_t_ = M;
                    if (t_ > 1.0 - M) t_ = target_t_ = 1.0 - M;
                } else if (seg_ == SurfaceSeg::L1_ROD) {
                    if (t_ > 0.85) t_ = target_t_ = 0.85;
                } else if (seg_ == SurfaceSeg::L2_ROD) {
                    if (t_ < 0.30) t_ = target_t_ = 0.30;  // L2短, 需更多margin
                    if (t_ > 0.70) t_ = target_t_ = 0.70;
                }
                state_ = State::ON_ROD;
                vel_ = Vec2();
                pos_ = footPos(p, pivot);
                return;
            }
        }
    }
}

void Player::jump(Pendulum &p, Vec2 pivot)
{
    // CP5: HOOKED中按W → 松钩 + 上跳打断
    if (state_ == State::HOOKED) {
        releaseHook();
        return;
    }
    if (state_ != State::ON_ROD)
        return;
    vel_ = footVel(p, pivot);
    vel_.y -= JUMP_BOOST;
    state_ = State::FLY;
    flyFrames_ = (seg_ == SurfaceSeg::M1_BALL || seg_ == SurfaceSeg::M2_BALL)
        ? FLY_GRACE_FRAMES - FLY_GRACE_BALL   // 球面跳: 保护较短
        : 0;                                   // 杆上跳: 满16帧保护
    pos_ = footPos(p, pivot);
    // 球面跳: pos向外推 + vel加径向分量, 确保脱离球面
    if (seg_ == SurfaceSeg::M1_BALL || seg_ == SurfaceSeg::M2_BALL) {
        int bIdx = (seg_ == SurfaceSeg::M1_BALL) ? 0 : 1;
        p.triggerJumpOff(bIdx);  // CP8: 跳离挤压反馈
        Vec2 bc = (seg_ == SurfaceSeg::M1_BALL)
            ? p.jointPos(pivot) : p.tipPos(pivot);
        Vec2 out = pos_ - bc;
        double d = sqrt(out.x*out.x + out.y*out.y);
        if (d > 0.01) {
            double nx = out.x/d, ny = out.y/d;
            pos_.x += nx * 6.0;  pos_.y += ny * 6.0;
            vel_.x += nx * 250.0; vel_.y += ny * 250.0;
        }
    }

    // 跳跃反作用力: 玩家向上跳 → 摆锤受向下推力, 微量 Δω
    const double JUMP_NUDGE = 0.25;
    double sign = (pos_.x > pivot.x) ? -1.0 : 1.0;
    if (seg_ == SurfaceSeg::L1_ROD || seg_ == SurfaceSeg::M1_BALL)
        p.nudgeW1(sign * JUMP_NUDGE);
    else
        p.nudgeW2(sign * JUMP_NUDGE);
}

/**
 * @brief 计算玩家脚部的速度向量
 * @param p 摆锤系统引用
 * @param pivot 支点位置
 * @return 玩家脚部的合速度向量
 *
 * 根据玩家当前所在的表面段计算速度：
 * - L1_ROD: 第一根杆上的切线速度 + 偏移旋转速度
 * - M1_BALL: 关节速度 + 球面旋转速度
 * - L2_ROD: 关节速度 + 第二根杆切线速度 + 偏移旋转速度
 * - M2_BALL: 杆尖速度 + 球面旋转速度
 */
Vec2 Player::footVel(const Pendulum &p, Vec2 pivot) const
{
    // d/dt[fromPolar(angle, len)] = fromPolar(angle + π/2, len·ω)
    // 因为 d/dt[Vec2(len·sin(a), len·cos(a))] = Vec2(len·cos(a)·ω, -len·sin(a)·ω)
    const auto &s = p.state();
    const auto &pr = p.params();

    switch (seg_)
    {
    case SurfaceSeg::L1_ROD:
    {
        double r = t_ * (pr.L1 - pr.R1);
        return Vec2::fromPolar(s.alpha1 + M_PI / 2, s.w1 * r)
             + Vec2::fromPolar(s.alpha1 + M_PI, s.w1 * ROD_OFFSET * rodSide_);
    }
    case SurfaceSeg::M1_BALL:
    {
        double a = M_PI / 2.0 + M_PI * t_;
        return Vec2::fromPolar(s.alpha1 + M_PI / 2, s.w1 * pr.L1)
             + Vec2::fromPolar(a + M_PI / 2, s.w1 * pr.R1);
    }
    case SurfaceSeg::L2_ROD:
    {
        double r = pr.R1 + t_ * (pr.L2 - pr.R1 - pr.R2);
        return Vec2::fromPolar(s.alpha1 + M_PI / 2, s.w1 * pr.L1)
             + Vec2::fromPolar(s.alpha2 + M_PI / 2, s.w2 * r)
             + Vec2::fromPolar(s.alpha2 + M_PI, s.w2 * ROD_OFFSET * rodSide_);
    }
    case SurfaceSeg::M2_BALL:
    {
        double a = M_PI / 2.0 + M_PI * t_;
        Vec2 jointVel = Vec2::fromPolar(s.alpha1 + M_PI / 2, s.w1 * pr.L1);
        Vec2 tipVel   = jointVel + Vec2::fromPolar(s.alpha2 + M_PI / 2, s.w2 * pr.L2);
        return tipVel + Vec2::fromPolar(a + M_PI / 2, s.w2 * pr.R2);
    }
    }
    return Vec2();
}

void Player::moveLeft()  { inputLeft_  = true; }
void Player::moveRight() { inputRight_ = true; }

void Player::warpTo(SurfaceSeg seg, double t)
{
    printf("[warpTo] %d->%d t=%.4f\n", (int)seg_, (int)seg, t);
    // CP8: 踩球脉冲反馈
    if (seg == SurfaceSeg::M1_BALL && pendulumRef_) pendulumRef_->triggerLand(0);
    if (seg == SurfaceSeg::M2_BALL && pendulumRef_) pendulumRef_->triggerLand(1);
    seg_ = seg;
    t_ = target_t_ = t;
}

// ============================================================
// CP5: 钩锁发射
// ============================================================
void Player::hook(Vec2 mouseWorld, Pendulum &p, Vec2 pivot)
{
    if (state_ != State::FLY) {
        printf("[hook] REJECT: not FLY (state=%d)\n", (int)state_);
        return;
    }

    // CP6: 消耗一发充能, 无可用的则拒绝
    int slot = -1;
    for (int i = 0; i < 2; i++)
        if (slots_[i].ready) { slot = i; break; }
    if (slot < 0) {
        printf("[hook] REJECT: no charges\n");
        return;
    }
    slots_[slot].ready = false;
    slots_[slot].timer = HOOK_COOLDOWN;

    Vec2 dir = mouseWorld - pos_;
    double len = sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len < 1.0) return;
    dir = dir / len;

    double bestDist = HOOK_MAX_RANGE;
    bool   hitAny   = false;
    SurfaceSeg bestSeg = SurfaceSeg::L1_ROD;
    double     bestT   = 0.5;

    Vec2 joint = p.jointPos(pivot);
    Vec2 tip   = p.tipPos(pivot);
    double R1  = p.params().R1;
    double R2  = p.params().R2;

    Vec2 rodNormal1 = Vec2::fromPolar(p.state().alpha1 + M_PI / 2, ROD_OFFSET);
    Vec2 rodNormal2 = Vec2::fromPolar(p.state().alpha2 + M_PI / 2, ROD_OFFSET);

    // L1杆: 中心线 + 两侧表面 (3线并行, 命中任一即生效)
    for (int side = -1; side <= 1; ++side) {
      Vec2 off = rodNormal1 * (double)side;
      Vec2 A = pivot + off, B = joint + off;
      Vec2 hp; double d;
      if (rayHitSegment(pos_, dir, A, B, hp, d) && d < bestDist) {
          bestDist = d; bestSeg = SurfaceSeg::L1_ROD;
          double distOnRod = sqrt((hp.x-A.x)*(hp.x-A.x)
                                + (hp.y-A.y)*(hp.y-A.y));
          double walkLen = p.params().L1 - p.params().R1;
          bestT = (walkLen > 0.0) ? (distOnRod / walkLen) : 0.0;
          if (bestT < 0.0) bestT = 0.0; if (bestT > 1.0) bestT = 1.0;
          hitAny = true;
      } }
    // L2杆: 中心线 + 两侧表面 (3线并行)
    for (int side = -1; side <= 1; ++side) {
      Vec2 off = rodNormal2 * (double)side;
      Vec2 A = joint + off, B = tip + off;
      Vec2 hp; double d;
      if (rayHitSegment(pos_, dir, A, B, hp, d) && d < bestDist) {
          bestDist = d; bestSeg = SurfaceSeg::L2_ROD;
          double distOnRod = sqrt((hp.x-A.x)*(hp.x-A.x)
                                + (hp.y-A.y)*(hp.y-A.y));
          double walkLen = p.params().L2 - p.params().R1 - p.params().R2;
          bestT = (walkLen > 0.0) ? ((distOnRod - p.params().R1) / walkLen) : 0.0;
          if (bestT < 0.0) bestT = 0.0; if (bestT > 1.0) bestT = 1.0;
          hitAny = true;
      } }
    // M1球
    { Vec2 hp; double d;
      if (rayHitCircle(pos_, dir, joint, R1, hp, d) && d < bestDist) {
          bestDist = d; bestSeg = SurfaceSeg::M1_BALL;
          double a = atan2(hp.x-joint.x, hp.y-joint.y);
          while (a < M_PI/2.0) a += 2.0*M_PI;
          if (a > 3.0*M_PI/2.0) a = 3.0*M_PI/2.0;
          bestT = (a - M_PI/2.0) / M_PI;
          if (bestT < 0.0) bestT = 0.0; if (bestT > 1.0) bestT = 1.0;
          hitAny = true;
      } }
    // M2球
    { Vec2 hp; double d;
      if (rayHitCircle(pos_, dir, tip, R2, hp, d) && d < bestDist) {
          bestDist = d; bestSeg = SurfaceSeg::M2_BALL;
          double a = atan2(hp.x-tip.x, hp.y-tip.y);
          while (a < M_PI/2.0) a += 2.0*M_PI;
          if (a > 3.0*M_PI/2.0) a = 3.0*M_PI/2.0;
          bestT = (a - M_PI/2.0) / M_PI;
          if (bestT < 0.0) bestT = 0.0; if (bestT > 1.0) bestT = 1.0;
          hitAny = true;
      } }

    if (!hitAny) {
        printf("[hook] MISS: no hit within 300px (mouse=%d,%d char=%d,%d)\n",
               (int)mouseWorld.x, (int)mouseWorld.y,
               (int)pos_.x, (int)pos_.y);
        return;
    }

    printf("[hook] HIT! seg=%d t=%.2f dist=%.0f\n",
           (int)bestSeg, bestT, bestDist);
    hookSeg_      = bestSeg;
    hookT_        = bestT;
    hookMaxLen_   = bestDist * HOOK_MAX_STRETCH;
    hookWorldPos_ = surfacePosAt(bestSeg, bestT, p, pivot);
    state_        = State::HOOKED;

    // 钩锁命中球: 微小角速度扰动 (复用前面已算的 dir)
    const double HOOK_NUDGE = 0.12;
    if (bestSeg == SurfaceSeg::M1_BALL)
        p.nudgeW1(dir.x * HOOK_NUDGE);
    else if (bestSeg == SurfaceSeg::M2_BALL)
        p.nudgeW2(dir.x * HOOK_NUDGE);

    // CP8: 钩锁命中球体反馈
    if (bestSeg == SurfaceSeg::M1_BALL || bestSeg == SurfaceSeg::M2_BALL)
        p.triggerHookHit(bestSeg == SurfaceSeg::M1_BALL ? 0 : 1);
}

// ============================================================
// CP5: 松钩
// ============================================================
void Player::releaseHook()
{
    state_ = State::FLY;
    vel_.y -= JUMP_BOOST;
    flyFrames_ = 0;
}

// ============================================================
// CP5: 球体碰撞 — 下半区硬反弹, 上半区软推出
// ============================================================
void Player::resolveBallCollision(Vec2 ballCenter, double radius)
{
    double dx = pos_.x - ballCenter.x;
    double dy = pos_.y - ballCenter.y;
    double dist = sqrt(dx * dx + dy * dy);
    if (dist >= radius || dist < 0.01) return;  // 未穿透

    Vec2 normal(dx / dist, dy / dist);  // 球心→玩家 方向
    double penetration = radius - dist;

    // 推出球体, 单帧上限8px防瞬移(球心移动造成的深穿透分多帧消化)
    double push = penetration;
    if (push > 8.0) push = 8.0;
    pos_.x += normal.x * push;
    pos_.y += normal.y * push;

    // 速度响应
    double vn = vel_.x * normal.x + vel_.y * normal.y;  // 冲向球心的分量
    if (vn >= 0.0) return;  // 已在远离, 不处理

    if (dy > 0.0) {
        // ===== 下半区 (非步行面侧) — 硬反弹 =====
        vel_.x -= 2.0 * normal.x * vn;  // 反射
        vel_.y -= 2.0 * normal.y * vn;
        vel_.x *= 1.5;                  // 额外弹力
        vel_.y *= 1.5;
    } else {
        // ===== 上半区 (步行面内侧) — 软推出 =====
        vel_.x -= normal.x * vn;        // 仅消去向内速度
        vel_.y -= normal.y * vn;
    }
}

/**
 * @brief 绘制玩家角色
 * @param  r SDL渲染器指针
 *
 * 根据玩家当前位置和状态绘制矩形：
 * - 在杆/球上时显示绿色
 * - 在空中时显示黄色
 */
void Player::draw(SDL_Renderer *r) const
{
    // ── 运动残影 ──
    if (trailCnt_ >= 2) {
        if (state_ == State::HOOKED) {
            // ══ 钩锁特殊形态: 发光能量绳 ══
            // 三层绘制: 外层光晕 → 核心线 → 内层高亮
            for (int i = 0; i < trailCnt_ - 1; i++) {
                int a = (trailHead_ - 1 - i + 16) % 16;
                int b = (trailHead_ - 2 - i + 16) % 16;
                float t = 1.0f - (float)i / trailCnt_;  // 1→0
                Vec2 d = trail_[a] - trail_[b];
                double len = sqrt(d.x*d.x + d.y*d.y);
                if (len < 0.5) continue;
                double nx = -d.y / len, ny = d.x / len;

                // 外层光晕 (宽6, 低alpha)
                int glowA = (int)(t * 50);
                if (glowA >= 3) {
                    SDL_SetRenderDrawColor(r, 80, 180, 255, (Uint8)glowA);
                    for (int k = -3; k <= 3; k++) {
                        SDL_RenderDrawLine(r,
                            (int)(trail_[a].x + nx*k), (int)(trail_[a].y + ny*k),
                            (int)(trail_[b].x + nx*k), (int)(trail_[b].y + ny*k));
                    }
                }
                // 核心线 (宽3, 中alpha)
                int coreA = (int)(t * 180);
                SDL_SetRenderDrawColor(r, 140, 220, 255, (Uint8)coreA);
                for (int k = -1; k <= 1; k++) {
                    SDL_RenderDrawLine(r,
                        (int)(trail_[a].x + nx*k), (int)(trail_[a].y + ny*k),
                        (int)(trail_[b].x + nx*k), (int)(trail_[b].y + ny*k));
                }
                // 内层高亮 (宽1, 高alpha, 近白色)
                int innerA = (int)(t * 220);
                SDL_SetRenderDrawColor(r, 220, 245, 255, (Uint8)innerA);
                SDL_RenderDrawLine(r,
                    (int)trail_[a].x, (int)trail_[a].y,
                    (int)trail_[b].x, (int)trail_[b].y);
            }
            // 末端发光点
            int last = (trailHead_ - trailCnt_ + 16) % 16;
            SDL_SetRenderDrawColor(r, 100, 200, 255, 40);
            SDL_Rect glow = {(int)trail_[last].x - 3, (int)trail_[last].y - 3, 6, 6};
            SDL_RenderFillRect(r, &glow);
            SDL_SetRenderDrawColor(r, 200, 240, 255, 80);
            SDL_Rect core = {(int)trail_[last].x - 1, (int)trail_[last].y - 1, 3, 3};
            SDL_RenderFillRect(r, &core);
        } else {
            // ══ 普通形态: 彗星拖尾 ══
            for (int i = 0; i < trailCnt_ - 1; i++) {
                int a = (trailHead_ - 1 - i + 16) % 16;
                int b = (trailHead_ - 2 - i + 16) % 16;
                float t = 1.0f - (float)i / trailCnt_;  // 1→0
                int alpha = (int)(t * 160);
                int w = (int)(t * 4 + 1);  // 粗→细
                Vec2 d = trail_[a] - trail_[b];
                double len = sqrt(d.x*d.x + d.y*d.y);
                if (len > 0.5) {
                    double nx = -d.y / len, ny = d.x / len;
                    for (int k = -w/2; k <= w/2; k++) {
                        if (state_ == State::ON_ROD)
                            SDL_SetRenderDrawColor(r, 0, 180, 60, alpha);
                        else if (state_ == State::FLY)
                            SDL_SetRenderDrawColor(r, 255, 220, 60, alpha);
                        SDL_RenderDrawLine(r,
                            (int)(trail_[a].x + nx*k), (int)(trail_[a].y + ny*k),
                            (int)(trail_[b].x + nx*k), (int)(trail_[b].y + ny*k));
                    }
                }
            }
            // 末尾小点
            int last = (trailHead_ - trailCnt_ + 16) % 16;
            SDL_SetRenderDrawColor(r, 180, 180, 180, 30);
            SDL_Rect dot = {(int)trail_[last].x - 1, (int)trail_[last].y - 1, 2, 2};
            SDL_RenderFillRect(r, &dot);
        }
    }

    // ── 死亡粒子 ──
    for (const auto& p : particles_) {
        float t = p.life / 0.4f;
        SDL_SetRenderDrawColor(r, 255, (int)(50*t), (int)(50*t), (int)(255*t));
        SDL_Rect d = {(int)p.pos.x - 1, (int)p.pos.y - 1, 2, 2};
        SDL_RenderFillRect(r, &d);
    }

    // ── CP8: 得分离子迸溅粒子 ──
    for (const auto& sp : scoreSparks_) {
        float t = sp.life / sp.maxLife;  // 1→0
        int a = (int)(t * 240);
        if (a < 5) continue;
        // 颜色: 金黄→白渐变
        Uint8 cr = 255;
        Uint8 cg = (Uint8)(180 + (1.0f - t) * 75);  // 金黄→白
        Uint8 cb = (Uint8)(80  + (1.0f - t) * 140);  // 橙黄→白
        int sz = (t > 0.6f) ? 3 : (t > 0.3f) ? 2 : 1;
        SDL_SetRenderDrawColor(r, cr, cg, cb, (Uint8)a);
        SDL_Rect d = {(int)sp.pos.x - sz/2, (int)sp.pos.y - sz/2, sz, sz};
        SDL_RenderFillRect(r, &d);
        // 高亮期加光晕
        if (t > 0.5f) {
            SDL_SetRenderDrawColor(r, cr, cg, cb, (Uint8)(a / 4));
            SDL_Rect gd = {(int)sp.pos.x - 2, (int)sp.pos.y - 2, 5, 5};
            SDL_RenderFillRect(r, &gd);
        }
    }

    // ═══ 能量核心体: 菱形 + 高光 + 呼吸光晕 ═══
    int cx = (int)pos_.x;
    int cy = (int)pos_.y - 6;  // 角色中心 (脚底偏上)
    int R = 7;  // 菱形半径

    // 状态色
    Uint8 sr, sg, sb;
    if (state_ == State::ON_ROD) { sr = 0; sg = 255; sb = 100; }     // 绿色
    else if (state_ == State::FLY) { sr = 255; sg = 200; sb = 0; }   // 黄色
    else if (state_ == State::HOOKED) { sr = 100; sg = 180; sb = 255; } // 蓝色
    else { // DEAD: 红色闪烁
        Uint64 elapsed = SDL_GetTicks64() - deathTimer_;
        if ((elapsed / 80) % 2 == 0) {
            sr = 255; sg = 50; sb = 50;
        } else {
            drawHUD(r); return;  // 闪烁暗帧不绘制
        }
    }

    float breath = (sinf((float)SDL_GetTicks() * 0.004f) + 1.0f) * 0.5f;  // 0~1

    // 层1: 外层呼吸光晕
    int glowR = R + 5 + (int)(breath * 4);  // 12~16px (+30%)
    int glowA = (int)(32 + breath * 46);    // 32~78 (+30%)
    for (int y = -glowR; y <= glowR; y++) {
        int hw = glowR - (y < 0 ? -y : y);  // 菱形
        if (hw < 1) continue;
        SDL_SetRenderDrawColor(r, sr, sg, sb, (Uint8)glowA);
        SDL_RenderDrawLine(r, cx - hw, cy + y, cx + hw, cy + y);
    }

    // 层2: 菱形主体 (逐行扫描, 边缘暗→中心亮)
    for (int y = -R; y <= R; y++) {
        int hw = R - (y < 0 ? -y : y);  // 菱形半宽
        if (hw < 1) continue;
        float t = 1.0f - (float)(y < 0 ? -y : y) / (float)R;  // 0(边缘)→1(中心行)
        // 边缘暗 (40%) → 中心亮 (100%)
        float brightness = 0.4f + t * 0.6f;
        Uint8 cr = (Uint8)(sr * brightness);
        Uint8 cg = (Uint8)(sg * brightness);
        Uint8 cb = (Uint8)(sb * brightness);
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
        SDL_RenderDrawLine(r, cx - hw, cy + y, cx + hw, cy + y);
    }

    // 层3: 中心高光点 (白色, 2×2)
    SDL_SetRenderDrawColor(r, 255, 255, 255, (Uint8)(180 + breath * 60));
    SDL_Rect hl = {cx - 1, cy - 1, 2, 2};
    SDL_RenderFillRect(r, &hl);

    // CP5: 钩锁绳 — D混合方案: 张力响应 + 能量脉冲 + 电弧抖动
    if (state_ == State::HOOKED) {
        double dx = hookWorldPos_.x - pos_.x;
        double dy = hookWorldPos_.y - pos_.y;
        double dist = sqrt(dx*dx + dy*dy);
        if (dist < 1.0) dist = 1.0;
        double nx = dx / dist, ny = dy / dist;  // 绳子方向单位向量
        // 法向量 (垂直于绳子)
        double fnx = -ny, fny = nx;

        // ── 张力计算: 0(松弛) ~ 1(接近断裂) ──
        double tension = (dist - HOOK_REST) / (hookMaxLen_ - HOOK_REST);
        if (tension < 0.0) tension = 0.0;
        if (tension > 1.0) tension = 1.0;

        int segLen = 6;  // 每段像素长度
        int segments = (int)(dist / segLen);
        if (segments < 1) segments = 1;

        // ═══ 层1: 张力响应基础绳 ═══
        // 松弛→细虚线暗淡; 拉紧→粗实线明亮
        int lineW = (int)(1 + tension * 3);   // 1~4px
        int baseA = (int)(80 + tension * 160); // 80~240
        Uint8 cr = (Uint8)(160 + tension * 95);  // 160→255
        Uint8 cg = (Uint8)(180 + tension * 75);  // 180→255
        Uint8 cb = (Uint8)(100 + tension * 80);  // 100→180

        for (int i = 0; i < segments; i++) {
            double t0 = (double)i / segments;
            double t1 = (double)(i + 1) / segments;
            // 松弛时画虚线 (跳偶数段), 拉紧时全画
            if (tension < 0.3 && (i % 2 != 0)) continue;
            SDL_SetRenderDrawColor(r, cr, cg, cb, (Uint8)baseA);
            // 画粗线: 法向偏移多层
            for (int k = -lineW/2; k <= lineW/2; k++) {
                SDL_RenderDrawLine(r,
                    (int)(pos_.x + dx*t0 + fnx*k), (int)(pos_.y + dy*t0 + fny*k),
                    (int)(pos_.x + dx*t1 + fnx*k), (int)(pos_.y + dy*t1 + fny*k));
            }
        }

        // ═══ 层2: 能量脉冲节点 ═══
        // 2个亮点沿绳子往返移动
        Uint32 now = SDL_GetTicks();
        for (int p = 0; p < 2; p++) {
            float phase = (p == 0) ? 0.0f : 0.5f;  // 两个脉冲相位错开
            float wave = sinf((float)now * 0.005f + phase * 6.28f);
            float pulseT = (wave + 1.0f) * 0.5f;  // 0~1 沿绳位置

            int px = (int)(pos_.x + dx * pulseT);
            int py = (int)(pos_.y + dy * pulseT);

            // 亮白色核心
            int coreA = (int)(160 + tension * 80);
            SDL_SetRenderDrawColor(r, 240, 250, 255, (Uint8)coreA);
            SDL_Rect core = {px - 1, py - 1, 3, 3};
            SDL_RenderFillRect(r, &core);

            // 淡蓝光晕
            int glowA = (int)(40 + tension * 50);
            SDL_SetRenderDrawColor(r, 120, 200, 255, (Uint8)glowA);
            SDL_Rect glow = {px - 2, py - 2, 5, 5};
            SDL_RenderFillRect(r, &glow);
        }

        // ═══ 层3: 高张力电弧抖动 ═══
        if (tension > 0.6f) {
            float arcStr = (tension - 0.6f) / 0.4f;  // 0~1
            int arcSegs = (int)(dist / 12);
            if (arcSegs < 1) arcSegs = 1;
            for (int i = 0; i < arcSegs; i++) {
                double t0 = (double)i / arcSegs;
                double t1 = (double)(i + 1) / arcSegs;
                // 随机法向偏移 (每段独立, 模拟电弧抖动)
                float off0 = (float)((rand() % 11) - 5) * arcStr * 3.0f;
                float off1 = (float)((rand() % 11) - 5) * arcStr * 3.0f;
                int a = (int)(arcStr * 180);
                SDL_SetRenderDrawColor(r, 180, 220, 255, (Uint8)a);
                SDL_RenderDrawLine(r,
                    (int)(pos_.x + dx*t0 + fnx*off0), (int)(pos_.y + dy*t0 + fny*off0),
                    (int)(pos_.x + dx*t1 + fnx*off1), (int)(pos_.y + dy*t1 + fny*off1));
            }
        }
    }

    // CP6: 钩锁充能 HUD
    drawHUD(r);
}

// ============================================================
// CP5: HOOKED 状态 — 弹性绳物理
// ============================================================
void Player::updateHooked(double dt, const Pendulum &p, Vec2 pivot)
{
    // 1. 更新钩点世界坐标 (随摆锤运动)
    hookWorldPos_ = surfacePosAt(hookSeg_, hookT_, p, pivot);

    // 2. 弹性拉力
    Vec2 rope = hookWorldPos_ - pos_;
    double dist = sqrt(rope.x * rope.x + rope.y * rope.y);
    if (dist > 0.01) {
        Vec2 ropeDir = rope / dist;
        if (dist > HOOK_REST) {
            double force = HOOK_K * (dist - HOOK_REST);
            vel_.x += ropeDir.x * force * dt;
            vel_.y += ropeDir.y * force * dt;
        }
        // 绳长上限约束
        if (dist > hookMaxLen_) {
            pos_ = hookWorldPos_ - ropeDir * hookMaxLen_;
            double vDot = vel_.x * ropeDir.x + vel_.y * ropeDir.y;
            if (vDot > 0.0) {
                vel_.x -= ropeDir.x * vDot;
                vel_.y -= ropeDir.y * vDot;
            }
        }
    }

    // 3. 重力 + 积分
    vel_.y += GRAVITY * gravScale_ * dt;
    pos_.x += vel_.x * dt;
    pos_.y += vel_.y * dt;

    // 4. CP5: 球体碰撞
    {
        Vec2 joint = p.jointPos(pivot);
        Vec2 tip   = p.tipPos(pivot);
        resolveBallCollision(joint, p.params().R1);
        resolveBallCollision(tip,   p.params().R2);
    }

    // 5. 出屏死亡
    if (pos_.y > DEAD_Y) {
        die();
        return;
    }

    // 5. 接近表面 → snap回ON_ROD
    SurfacePoint sp = findNearestSurface(p, pivot, pos_);
    if (sp.distance < HOOK_SNAP_DIST) {
        printf("[SNAP-hook] → seg=%d t=%.4f dist=%.1f pos=(%.0f,%.0f)\n",
               (int)sp.seg, sp.t, sp.distance, pos_.x, pos_.y);
        warpTo(sp.seg, sp.t);
        // 杆侧: 以玩家实际位置判断, 不用几何最近侧(防钩锁拉过中线)
        if (sp.seg == SurfaceSeg::L1_ROD || sp.seg == SurfaceSeg::L2_ROD) {
            double rodA = (sp.seg == SurfaceSeg::L1_ROD)
                ? p.state().alpha1 : p.state().alpha2;
            Vec2 normal = Vec2::fromPolar(rodA + M_PI / 2, 1.0);
            Vec2 center = surfacePosAt(sp.seg, sp.t, p, pivot, 0.0);
            rodSide_ = ((pos_.x - center.x) * normal.x
                      + (pos_.y - center.y) * normal.y >= 0) ? 1.0 : -1.0;
        } else {
            rodSide_ = sp.rodSide;
        }
        // 球面snap: 钳制t到安全区, 防边缘频闪
        if (seg_ == SurfaceSeg::M1_BALL || seg_ == SurfaceSeg::M2_BALL) {
            const double M = 0.06;
            if (t_ < M) t_ = target_t_ = M;
            if (t_ > 1.0 - M) t_ = target_t_ = 1.0 - M;
        } else if (seg_ == SurfaceSeg::L1_ROD) {
            if (t_ > 0.85) t_ = target_t_ = 0.85;
        } else if (seg_ == SurfaceSeg::L2_ROD) {
            if (t_ < 0.30) t_ = target_t_ = 0.30;
            if (t_ > 0.70) t_ = target_t_ = 0.70;
        }
        state_ = State::ON_ROD;
        vel_ = Vec2();
        pos_ = footPos(p, pivot);
    }
}

// ============================================================
// CP7: 触发死亡
// ============================================================
void Player::die()
{
    if (state_ == State::DEAD) return;  // 已死亡, 不重复触发
    state_ = State::DEAD;
    deathTimer_ = SDL_GetTicks64();

    // CP8: 摆锤红色爆裂反馈
    if (pendulumRef_) pendulumRef_->triggerDeath();

    // 死亡粒子爆发
    particles_.clear();
    for (int i = 0; i < 12; i++) {
        Particle p;
        p.pos = pos_;
        double ang = (rand() % 360) * M_PI / 180.0;
        double spd = 80 + rand() % 200;
        p.vel = Vec2(cos(ang) * spd, sin(ang) * spd);
        p.life = 0.4f;
        particles_.push_back(p);
    }
}

// ============================================================
// CP8: 得分离子迸溅 (从角色位置全向喷射)
// ============================================================
void Player::emitScoreSparks()
{
    constexpr int COUNT = 14;
    for (int i = 0; i < COUNT; i++) {
        ScoreSpark sp;
        double ang = (2.0 * M_PI * i) / COUNT + (rand() % 20 - 10) * 0.01;  // 轻微随机扰动

        // 从角色中心出发, 稍向外偏移避免聚集
        sp.pos = pos_ + Vec2::fromPolar(ang, 4.0);

        // 径向向外高速射出
        double spd = 140.0 + (rand() % 120);  // 140~260 px/s
        sp.vel = Vec2::fromPolar(ang, spd);

        sp.life    = 0.35f + (rand() % 150) / 1000.0f;  // 0.35~0.5s
        sp.maxLife = sp.life;
        scoreSparks_.push_back(sp);
    }
}

// ============================================================
// CP7: 落在安全平台上
// ============================================================
void Player::landOnPlatform(double standY)
{
    if (state_ != State::FLY) return;
    pos_.y = standY;
    vel_.y = 0.0;
    // 保持在 FLY 状态, 可 W 跳起
}

// ============================================================
// CP7: DEAD 状态 —— 闪烁 0.5s → 自动重置
// ============================================================
void Player::updateDead(double dt, const Pendulum &p, Vec2 pivot)
{
    (void)dt;
    (void)p;
    (void)pivot;
    Uint64 elapsed = SDL_GetTicks64() - deathTimer_;
    if (elapsed > 500) {
        reset_Player();
        pos_ = footPos(p, pivot);
    }
}

// ============================================================
// CP4: 查找摆锤上距目标点最近的表面点
// ============================================================
SurfacePoint Player::findNearestSurface(const Pendulum &p, Vec2 pivot,
                                        Vec2 target) const
{
    SurfacePoint best;
    best.distance = 1e9;
    best.seg = SurfaceSeg::L2_ROD;
    best.t = 0.5;

    const auto &s = p.state();
    const auto &pr = p.params();
    Vec2 joint = p.jointPos(pivot);
    Vec2 tip    = p.tipPos(pivot);

    // ====== 球面优先 (CP4: 球吸附优先级 > 杆) ======

    // ------ M1 球面 ------
    {
        double dx = target.x - joint.x;
        double dy = target.y - joint.y;
        double distC = sqrt(dx*dx + dy*dy);
        if (distC > 0.0) {
            double a = atan2(dx, dy);
            while (a < -M_PI/2.0) a += 2.0*M_PI;
            while (a > 3.0*M_PI/2.0) a -= 2.0*M_PI;
            double lo = M_PI/2.0, hi = 3.0*M_PI/2.0;
            // 下半球拒绝: 球面弧仅覆盖上半球
            if (a >= lo && a <= hi) {
                double tVal = (a - lo) / M_PI;
                Vec2 surf = joint + Vec2::fromPolar(a, pr.R1);
                double d = sqrt((target.x-surf.x)*(target.x-surf.x)
                              + (target.y-surf.y)*(target.y-surf.y));
                if (d < best.distance) {
                    best.distance = d;
                    best.seg = SurfaceSeg::M1_BALL;
                    best.t = tVal;
                    best.worldPos = surf;
                }
            }
        }
    }

    // ------ M2 球面 ------
    {
        double dx = target.x - tip.x;
        double dy = target.y - tip.y;
        double distC = sqrt(dx*dx + dy*dy);
        if (distC > 0.0) {
            double a = atan2(dx, dy);
            while (a < -M_PI/2.0) a += 2.0*M_PI;
            while (a > 3.0*M_PI/2.0) a -= 2.0*M_PI;
            double lo = M_PI/2.0, hi = 3.0*M_PI/2.0;
            // 下半球拒绝: 球面弧仅覆盖上半球
            if (a >= lo && a <= hi) {
                double tVal = (a - lo) / M_PI;
                Vec2 surf = tip + Vec2::fromPolar(a, pr.R2);
                double d = sqrt((target.x-surf.x)*(target.x-surf.x)
                              + (target.y-surf.y)*(target.y-surf.y));
                if (d < best.distance) {
                    best.distance = d;
                    best.seg = SurfaceSeg::M2_BALL;
                    best.t = tVal;
                    best.worldPos = surf;
                }
            }
        }
    }

    // ====== 杆面 (球优先后置) ======

    // ------ L1 杆面 (±两侧) ------
    {
        double dx = joint.x - pivot.x;
        double dy = joint.y - pivot.y;
        double lenSq = dx*dx + dy*dy;
        if (lenSq > 0.0) {
            double invLen = 1.0 / sqrt(lenSq);
            double tProj = ((target.x - pivot.x)*dx + (target.y - pivot.y)*dy) / lenSq;
            if (tProj < 0.0) tProj = 0.0;
            if (tProj > 1.0) tProj = 1.0;

            double nx = -dy * invLen, ny = dx * invLen;
            double walkLen = pr.L1 - pr.R1;
            if (walkLen > 0.0) {
                double wt = (tProj * pr.L1) / walkLen;
                if (wt >= 0.0 && wt <= 1.0) {
                    for (int side = -1; side <= 1; side += 2) {
                        Vec2 surf(pivot.x + tProj*dx + nx*ROD_OFFSET*side,
                                  pivot.y + tProj*dy + ny*ROD_OFFSET*side);
                        double d = sqrt((target.x-surf.x)*(target.x-surf.x)
                                      + (target.y-surf.y)*(target.y-surf.y));
                        if (d < best.distance) {
                            best.distance = d;
                            best.seg = SurfaceSeg::L1_ROD;
                            best.t = wt;
                            best.worldPos = surf;
                            best.rodSide = (double)side;
                        }
                    }
                }
            }
        }
    }

    // ------ L2 杆面 (±两侧) ------
    {
        double dx = tip.x - joint.x;
        double dy = tip.y - joint.y;
        double lenSq = dx*dx + dy*dy;
        if (lenSq > 0.0) {
            double invLen = 1.0 / sqrt(lenSq);
            double tProj = ((target.x - joint.x)*dx + (target.y - joint.y)*dy) / lenSq;
            if (tProj < 0.0) tProj = 0.0;
            if (tProj > 1.0) tProj = 1.0;

            double nx = -dy * invLen, ny = dx * invLen;
            double walkLen = pr.L2 - pr.R1 - pr.R2;
            if (walkLen > 0.0) {
                double wt = (tProj * pr.L2 - pr.R1) / walkLen;
                if (wt >= 0.0 && wt <= 1.0) {
                    for (int side = -1; side <= 1; side += 2) {
                        Vec2 surf(joint.x + tProj*dx + nx*ROD_OFFSET*side,
                                  joint.y + tProj*dy + ny*ROD_OFFSET*side);
                        double d = sqrt((target.x-surf.x)*(target.x-surf.x)
                                      + (target.y-surf.y)*(target.y-surf.y));
                        if (d < best.distance) {
                            best.distance = d;
                            best.seg = SurfaceSeg::L2_ROD;
                            best.t = wt;
                            best.worldPos = surf;
                            best.rodSide = (double)side;
                        }
                    }
                }
            }
        }
    }

    return best;
}

// ============================================================
// CP6: 钩锁充能更新
// ============================================================
void Player::updateCooldowns(double dt)
{
    for (int i = 0; i < 2; i++) {
        if (slots_[i].ready) continue;
        slots_[i].timer -= (float)dt;
        if (slots_[i].timer <= 0.0f) {
            slots_[i].ready = true;
            slots_[i].timer = 0.0f;
        }
    }
}

// ============================================================
// CP6: 钩锁充能 HUD (左上角)
// ============================================================
void Player::drawHUD(SDL_Renderer *r) const
{
    const int X = 16, Y = 16, W = 18, H = 28, GAP = 5;
    for (int i = 0; i < 2; i++) {
        SDL_Rect rect = { X + i * (W + GAP), Y, W, H };
        if (slots_[i].ready) {
            // 满: 亮绿
            SDL_SetRenderDrawColor(r, 0, 220, 80, 255);
            SDL_RenderFillRect(r, &rect);
        } else {
            // 空: 暗灰底
            SDL_SetRenderDrawColor(r, 40, 40, 50, 255);
            SDL_RenderFillRect(r, &rect);
            // 冷却进度: 从底部向上填充
            float pct = 1.0f - slots_[i].timer / HOOK_COOLDOWN;
            int fillH = (int)(H * pct);
            SDL_Rect fill = { X + i * (W + GAP), Y + H - fillH, W, fillH };
            SDL_SetRenderDrawColor(r, 0, 180, 60, 255);
            SDL_RenderFillRect(r, &fill);
        }
        // 边框
        SDL_SetRenderDrawColor(r, 100, 100, 120, 255);
        SDL_RenderDrawRect(r, &rect);
    }

    // 得分数字 (3x5 点阵, X=16 Y=54)
    static const bool DIGITS[10][5][3] = {
        {{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}}, // 0
        {{0,0,1},{0,0,1},{0,0,1},{0,0,1},{0,0,1}}, // 1
        {{1,1,1},{0,0,1},{1,1,1},{1,0,0},{1,1,1}}, // 2
        {{1,1,1},{0,0,1},{1,1,1},{0,0,1},{1,1,1}}, // 3
        {{1,0,1},{1,0,1},{1,1,1},{0,0,1},{0,0,1}}, // 4
        {{1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1}}, // 5
        {{1,1,1},{1,0,0},{1,1,1},{1,0,1},{1,1,1}}, // 6
        {{1,1,1},{0,0,1},{0,0,1},{0,0,1},{0,0,1}}, // 7
        {{1,1,1},{1,0,1},{1,1,1},{1,0,1},{1,1,1}}, // 8
        {{1,1,1},{1,0,1},{1,1,1},{0,0,1},{1,1,1}}, // 9
    };
    int num = score_;
    int cx = 80, cy = 56, dot = 3, gap = 2;
    if (num == 0) {
        for (int row = 0; row < 5; row++)
            for (int col = 0; col < 3; col++)
                if (DIGITS[0][row][col]) {
                    SDL_Rect d = {cx + col*(dot+1), cy + row*(dot+1), dot, dot};
                    SDL_SetRenderDrawColor(r, 255, 255, 100, 255);
                    SDL_RenderFillRect(r, &d);
                }
    } else {
        char buf[12]; int len = 0;
        while (num > 0) { buf[len++] = '0' + (num % 10); num /= 10; }
        for (int i = len-1; i >= 0; i--) {
            int dg = buf[i] - '0';
            for (int row = 0; row < 5; row++)
                for (int col = 0; col < 3; col++)
                    if (DIGITS[dg][row][col]) {
                        SDL_Rect d = {cx + col*(dot+1), cy + row*(dot+1), dot, dot};
                        SDL_SetRenderDrawColor(r, 255, 255, 100, 255);
                        SDL_RenderFillRect(r, &d);
                    }
            cx += (3*(dot+1) + gap);
        }
    }

    // 重力倍率指示: 小色条 (绿=轻, 红=重)
    int gx = 16, gy = 80, gw = 40, gh = 6;
    double gt = (gravScale_ - 0.1) / 2.9;  // 0.1~3.0 → 0~1
    int gr = (int)(255 * gt), gg = (int)(255 * (1.0 - gt));
    SDL_SetRenderDrawColor(r, gr, gg, 60, 255);
    SDL_Rect gbar = {gx, gy, (int)(gw * gt + 2), gh};
    SDL_RenderFillRect(r, &gbar);
    SDL_SetRenderDrawColor(r, 100, 100, 120, 255);
    SDL_Rect gframe = {gx, gy, gw, gh};
    SDL_RenderDrawRect(r, &gframe);
}