#include "Pendulum.h"
#include "Physics.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

// ============================================================
// 基础方法
// ============================================================

Pendulum::Pendulum()
{
    reset();
}

void Pendulum::reset()
{
    state_.alpha1 = M_PI / 2;
    state_.alpha2 = M_PI / 2;
    state_.w1 = 0;
    state_.w2 = 0;
    // 清除反馈和残影
    for (int i = 0; i < 2; i++) {
        feedback_[i] = BallFeedback();
        trail_[i].clear();
    }
    sparks_.clear();
    ripples_.clear();
}

void Pendulum::setParams(double R1, double R2, double L1, double L2)
{
    params_.R1 = R1;
    params_.R2 = R2;
    params_.L1 = L1;
    params_.L2 = L2;
}

void Pendulum::step(double dt, int substeps)
{
    Physics::substep(state_, params_, dt, substeps);
}

Vec2 Pendulum::jointPos(Vec2 pivot) const
{
    return state_.tip1(pivot, params_.L1);
}

Vec2 Pendulum::tipPos(Vec2 pivot) const
{
    return state_.tip2(pivot, params_.L1, params_.L2);
}

void Pendulum::setPreset(int idx)
{
    static const Physics::State presets[] = {
        {M_PI / 2, M_PI / 2, 0, 0},
        {2.0, 0.2, 0, 0},
        {3.0, -1.5, 0, 0},
        {M_PI / 2, -M_PI / 2, 0, 0},
        {1.2, 1.8, 0, 0},
    };
    state_ = presets[idx % 5];
}

Physics::Params& Pendulum::params() { return params_; }
const Physics::Params& Pendulum::params() const { return params_; }
const Physics::State& Pendulum::state() const { return state_; }

// ============================================================
// CP8 Phase 1: 预计算球体纹理 (灰度光照)
// ============================================================

void Pendulum::initTextures(SDL_Renderer *r)
{
    destroyTextures();
    generateBallTexture(r, 0, (int)params_.R1);
    generateBallTexture(r, 1, (int)params_.R2);
}

void Pendulum::destroyTextures()
{
    for (int i = 0; i < 2; i++) {
        if (ballTex_[i]) {
            SDL_DestroyTexture(ballTex_[i]);
            ballTex_[i] = nullptr;
        }
    }
}

void Pendulum::generateBallTexture(SDL_Renderer *r, int idx, int radius)
{
    int size = radius * 2 + 2;  // +2 for safety margin
    SDL_Texture *tex = SDL_CreateTexture(r,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, size, size);
    if (!tex) return;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    // 渲染到纹理
    SDL_Texture *prev = SDL_GetRenderTarget(r);
    SDL_SetRenderTarget(r, tex);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
    SDL_RenderClear(r);

    int cx = radius + 1;
    int cy = radius + 1;

    // 光源方向 (左上)
    double lx = -0.6, ly = -0.6, lz = 0.8;
    double lLen = sqrt(lx*lx + ly*ly + lz*lz);
    lx /= lLen; ly /= lLen; lz /= lLen;

    for (int y = -radius; y <= radius; y++) {
        int w = (int)std::sqrt((double)(radius * radius - y * y));
        for (int x = -w; x <= w; x++) {
            double dx = (double)x / radius;
            double dy = (double)y / radius;
            double distSq = dx * dx + dy * dy;
            if (distSq > 1.0) continue;

            // 球面法线
            double dist = std::sqrt(distSq);
            double nx, ny;
            if (dist > 0.001) {
                nx = dx / dist;
                ny = dy / dist;
            } else {
                nx = 0; ny = 0;
            }
            double nz = std::sqrt(1.0 - distSq);

            // Lambertian 漫反射
            double NdotL = nx*lx + ny*ly + nz*lz;
            if (NdotL < 0.0) NdotL = 0.0;

            double ambient = 0.18;
            double diffuse = ambient + (1.0 - ambient) * NdotL;

            // Blinn-Phong 镜面高光
            double hx = lx, hy = ly, hz = lz + 1.0; // half vector (view = 0,0,1)
            double hLen = sqrt(hx*hx + hy*hy + hz*hz);
            double NdotH = (nx*hx + ny*hy + nz*hz) / hLen;
            if (NdotH < 0.0) NdotH = 0.0;
            double specular = std::pow(NdotH, 32.0) * 0.7;

            // 边缘暗化 (Fresnel-like)
            double edge = 1.0 - distSq * 0.3;

            // 最终灰度值
            double val = diffuse * edge + specular;
            if (val > 1.0) val = 1.0;
            int c = (int)(val * 255);
            if (c > 255) c = 255;

            SDL_SetRenderDrawColor(r, (Uint8)c, (Uint8)c, (Uint8)c, 255);
            SDL_RenderDrawPoint(r, cx + x, cy + y);
        }
    }

    SDL_SetRenderTarget(r, prev);
    ballTex_[idx] = tex;
}

// ============================================================
// CP8 Phase 1: 速度响应变色
// ============================================================

SDL_Color Pendulum::lerpColor(SDL_Color a, SDL_Color b, double t)
{
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return {
        (Uint8)(a.r + (b.r - a.r) * t),
        (Uint8)(a.g + (b.g - a.g) * t),
        (Uint8)(a.b + (b.b - a.b) * t),
        255
    };
}

SDL_Color Pendulum::computeBallColor(int ballIdx, double omega) const
{
    double absOmega = fabs(omega);
    double maxOmega = 8.0;
    double t = absOmega / maxOmega;
    if (t > 1.0) t = 1.0;

    SDL_Color cold, hot, whiteHot;
    if (ballIdx == 0) {
        // M1: 暗红 → 正红 → 白热
        cold    = {80,  30,  30,  255};
        hot     = {255, 60,  60,  255};
        whiteHot= {255, 220, 180, 255};
    } else {
        // M2: 暗蓝 → 亮蓝 → 白热
        cold    = {30,  40,  80,  255};
        hot     = {140, 200, 255, 255};
        whiteHot= {220, 240, 255, 255};
    }

    // 两段插值
    if (t < 0.7) {
        return lerpColor(cold, hot, t / 0.7);
    } else {
        return lerpColor(hot, whiteHot, (t - 0.7) / 0.3);
    }
}

// ============================================================
// CP8 Phase 2: 交互反馈更新
// ============================================================

void Pendulum::updateFeedback(double dt)
{
    for (int i = 0; i < 2; i++) {
        feedback_[i].landPulse  *= std::exp(-dt * 12.0);  // ~0.2s
        feedback_[i].hookFlash  *= std::exp(-dt * 18.0);  // ~0.15s
        feedback_[i].jumpRipple *= std::exp(-dt * 8.0);   // ~0.3s
        feedback_[i].ejectGlow  *= std::exp(-dt * 14.0);  // ~0.2s
        feedback_[i].deathBurst *= std::exp(-dt * 8.0);   // ~0.3s
        feedback_[i].scorePulse *= std::exp(-dt * 14.0);  // ~0.15s
        // 钳制到0避免浮点残留
        if (feedback_[i].landPulse  < 0.001) feedback_[i].landPulse  = 0.0;
        if (feedback_[i].hookFlash  < 0.001) feedback_[i].hookFlash  = 0.0;
        if (feedback_[i].jumpRipple < 0.001) feedback_[i].jumpRipple = 0.0;
        if (feedback_[i].ejectGlow  < 0.001) feedback_[i].ejectGlow  = 0.0;
        if (feedback_[i].deathBurst < 0.001) feedback_[i].deathBurst = 0.0;
        if (feedback_[i].scorePulse < 0.001) feedback_[i].scorePulse = 0.0;
    }
}

// ============================================================
// CP8 Phase 2: 触发接口
// ============================================================

void Pendulum::triggerLand(int ballIdx)
{
    if (ballIdx >= 0 && ballIdx < 2) {
        feedback_[ballIdx].landPulse = 1.0;
        feedback_[ballIdx].hookFlash = 0.6;  // 踩球时白色闪烁
    }
}

void Pendulum::triggerHookHit(int ballIdx)
{
    if (ballIdx >= 0 && ballIdx < 2) {
        feedback_[ballIdx].hookFlash = 1.0;
        // 添加碰撞波纹
        Ripple rp;
        rp.origin = Vec2(0, 0);  // 中心
        rp.radius = 2.0;
        rp.life = 0.3;
        rp.maxLife = 0.3;
        rp.ballIdx = ballIdx;
        ripples_.push_back(rp);
        // 钩锁命中球时喷射火花
        emitBallSparks(ballIdx);
    }
}

void Pendulum::triggerJumpOff(int ballIdx)
{
    if (ballIdx >= 0 && ballIdx < 2)
        feedback_[ballIdx].jumpRipple = 1.0;
}

void Pendulum::triggerEject(int ballIdx)
{
    if (ballIdx >= 0 && ballIdx < 2)
        feedback_[ballIdx].ejectGlow = 1.0;
}

void Pendulum::triggerDeath()
{
    for (int i = 0; i < 2; i++)
        feedback_[i].deathBurst = 1.0;
}

void Pendulum::triggerScore()
{
    for (int i = 0; i < 2; i++) {
        feedback_[i].scorePulse = 1.0;
        feedback_[i].hookFlash  = 0.5;   // 得分时白色闪光
    }
}

void Pendulum::emitBallSparks(int ballIdx)
{
    if (ballIdx < 0 || ballIdx >= 2) return;
    // 使用缓存的支点实时计算球心位置
    Vec2 center = (ballIdx == 0) ? jointPos(pivot_) : tipPos(pivot_);
    int radius = (ballIdx == 0) ? (int)params_.R1 : (int)params_.R2;
    double omega = (ballIdx == 0) ? state_.w1 : state_.w2;
    SDL_Color color = computeBallColor(ballIdx, omega);
    emitSparks(center, radius, omega * 1.5, color);  // 加大火花量
}

// ============================================================
// CP8 Phase 2: 碰撞波纹更新/绘制
// ============================================================

void Pendulum::updateRipples(double dt)
{
    for (auto &rp : ripples_) {
        rp.radius += 120.0 * dt;  // 扩散速度 120 px/s
        rp.life -= dt;
    }
    ripples_.erase(std::remove_if(ripples_.begin(), ripples_.end(),
        [](const Ripple &r) { return r.life <= 0; }), ripples_.end());
}

void Pendulum::drawRipples(SDL_Renderer *r, Vec2 ballCenters[2])
{
    for (const auto &rp : ripples_) {
        if (rp.life <= 0) continue;
        double alpha = rp.life / rp.maxLife;
        int rad = (int)rp.radius;
        if (rad < 2) continue;

        Vec2 center = ballCenters[rp.ballIdx] + rp.origin;
        int a = (int)(alpha * 200);
        SDL_SetRenderDrawColor(r, 255, 255, 255, (Uint8)a);

        // 画圆环 (逐点)
        for (int y = -rad; y <= rad; y++) {
            int w = (int)std::sqrt((double)(rad * rad - y * y));
            if (w < 1) {
                SDL_RenderDrawPoint(r, (int)center.x, (int)center.y + y);
            } else {
                SDL_RenderDrawPoint(r, (int)center.x - w, (int)center.y + y);
                SDL_RenderDrawPoint(r, (int)center.x + w, (int)center.y + y);
            }
        }
    }
}

// ============================================================
// CP8 Phase 3: Bloom 外发光
// ============================================================

void Pendulum::drawBloom(SDL_Renderer *r, int cx, int cy, int radius,
                         SDL_Color color, double intensity)
{
    if (intensity < 0.15) return;

    // 3层光晕: 从外到内
    struct Layer { double mult; int alphaBase; };
    Layer layers[] = {
        {1.5, 15},
        {1.3, 28},
        {1.15, 45},
    };

    for (int i = 0; i < 3; i++) {
        int glowR = (int)(radius * layers[i].mult);
        int alpha = (int)(layers[i].alphaBase * intensity);
        if (alpha > 255) alpha = 255;
        if (alpha < 3) continue;

        SDL_SetRenderDrawColor(r, color.r, color.g, color.b, (Uint8)alpha);
        drawFillCircle(r, cx, cy, glowR, {color.r, color.g, color.b, (Uint8)alpha});
    }
}

// ============================================================
// CP8 Phase 4: 运动残影
// ============================================================

void Pendulum::recordTrail(Vec2 joint, Vec2 tip)
{
    // 球面线速度
    double surfSpeed1 = fabs(state_.w1) * params_.R1;
    double surfSpeed2 = fabs(state_.w2) * params_.R2;

    // 甩飞阈值 (与 Player 一致)
    constexpr double THROW_SPEED = 200.0;

    // 超过阈值: 每帧记录新位置; 低于阈值: 旧位置逐帧衰减消失
    if (surfSpeed1 > THROW_SPEED) trail_[0].record(joint);
    else                           trail_[0].decay();

    if (surfSpeed2 > THROW_SPEED) trail_[1].record(tip);
    else                           trail_[1].decay();
}

void Pendulum::drawTrail(SDL_Renderer *r, int ballIdx, int radius, SDL_Color base)
{
    const BallTrail &tr = trail_[ballIdx];
    if (tr.count < 2) return;

    // 当前球面速度 → 计算拖尾强度 (0~1)
    double surfSpeed = (ballIdx == 0) ? fabs(state_.w1) * params_.R1
                                      : fabs(state_.w2) * params_.R2;
    constexpr double THROW_SPEED = 200.0;
    // 强度 = 超出阈值部分, 映射到 0~1 (速度越高拖尾越强)
    double intensity = std::min((surfSpeed - THROW_SPEED) / THROW_SPEED, 1.0);
    if (intensity < 0.0) intensity = 0.0;

    for (int i = 0; i < tr.count; i++) {
        int idx = (tr.head - 1 - i + 12) % 12;
        double t = 1.0 - (double)i / tr.count;  // 1→0 (近→远)
        // alpha 和半径随强度动态缩放
        int alpha = (int)(t * (40 + 120 * intensity));  // 最低40, 最高160
        int rr    = (int)(radius * t * (0.4 + 0.5 * intensity));  // 0.4x ~ 0.9x
        if (alpha < 3 || rr < 2) continue;

        SDL_SetRenderDrawColor(r, base.r, base.g, base.b, (Uint8)alpha);
        drawFillCircle(r, (int)tr.positions[idx].x,
                       (int)tr.positions[idx].y, rr,
                       {base.r, base.g, base.b, (Uint8)alpha});
    }
}

// ============================================================
// CP8 Phase 4: 轨道火花粒子
// ============================================================

void Pendulum::emitSparks(Vec2 center, int radius, double omega, SDL_Color color)
{
    double absOmega = fabs(omega);
    if (absOmega < 3.0) return;

    // 生成概率随角速度增加
    int count = (int)((absOmega - 3.0) * 0.8);
    if (count < 1) count = 1;
    if (count > 4) count = 4;

    for (int i = 0; i < count; i++) {
        if (rand() % 3 != 0) continue;  // 不是每帧都发

        SparkParticle sp;
        double ang = (rand() % 360) * M_PI / 180.0;
        sp.pos = center + Vec2::fromPolar(ang, radius);

        // 切向速度 + 向外
        double tangentAng = ang + M_PI / 2;
        double spd = 30 + (rand() % 60);
        double dir = (omega > 0) ? 1.0 : -1.0;
        sp.vel = Vec2::fromPolar(tangentAng, spd * dir)
               + Vec2::fromPolar(ang, 20 + (rand() % 30));
        sp.life = 0.45f + (rand() % 300) / 1000.0f;  // 0.45~0.75s (+50%)
        sp.maxLife = sp.life;
        sp.color = color;
        sparks_.push_back(sp);
    }
}

void Pendulum::updateSparks(double dt)
{
    for (auto &sp : sparks_) {
        sp.pos.x += sp.vel.x * dt;
        sp.pos.y += sp.vel.y * dt;
        sp.vel.y += 100.0 * dt;  // 轻微重力
        sp.life -= (float)dt;
    }
    sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(),
        [](const SparkParticle &s) { return s.life <= 0; }), sparks_.end());
}

void Pendulum::drawSparks(SDL_Renderer *r)
{
    for (const auto &sp : sparks_) {
        float t = sp.life / sp.maxLife;
        int a = (int)(t * 220);
        if (a < 5) continue;
        // 粒子尺寸: 速度越快越大 (得分迸溅粒子高速=大尺寸)
        double spd = sqrt(sp.vel.x * sp.vel.x + sp.vel.y * sp.vel.y);
        int sz = (spd > 100.0) ? 3 : (t > 0.5f) ? 2 : 1;
        SDL_SetRenderDrawColor(r, sp.color.r, sp.color.g, sp.color.b, (Uint8)a);
        SDL_Rect d = {(int)sp.pos.x - sz/2, (int)sp.pos.y - sz/2, sz, sz};
        SDL_RenderFillRect(r, &d);
        // 高速粒子额外加一层淡光晕
        if (spd > 120.0 && sz >= 3) {
            int ga = (int)(t * 60);
            SDL_SetRenderDrawColor(r, sp.color.r, sp.color.g, sp.color.b, (Uint8)ga);
            SDL_Rect gd = {(int)sp.pos.x - 2, (int)sp.pos.y - 2, 5, 5};
            SDL_RenderFillRect(r, &gd);
        }
    }
}

// ============================================================
// CP8: 3D 球体绘制 (纹理 + 调色 + 反馈变形)
// ============================================================

void Pendulum::drawSphere(SDL_Renderer *r, int cx, int cy, int radius,
                          SDL_Color color, double omega, BallFeedback &fb,
                          int ballIdx, uint64_t frame)
{
    // 1. 心跳脉动
    double heartbeat = 1.0 + sin(frame * 0.05) * 0.015;

    // 2. 反馈变形
    double scaleX = 1.0;
    double scaleY = 1.0;

    // 踩上脉冲: 半径扩张
    double pulseAdd = fb.landPulse * 6.0 + fb.scorePulse * 5.0;

    // 跳离挤压: Y压缩, X拉伸 (保持面积)
    if (fb.jumpRipple > 0.01) {
        double squash = 1.0 - fb.jumpRipple * 0.08;
        scaleY *= squash;
        scaleX *= (1.0 / sqrt(squash));
    }

    // 高速离心扁平
    double absOmega = fabs(omega);
    if (absOmega > 5.0) {
        double flat = (absOmega - 5.0) * 0.004;
        if (flat > 0.04) flat = 0.04;
        scaleY *= (1.0 - flat);
        scaleX *= (1.0 + flat * 0.5);
    }

    // 3. 计算最终绘制尺寸
    int rx = (int)(radius * scaleX * heartbeat + pulseAdd);
    int ry = (int)(radius * scaleY * heartbeat + pulseAdd);
    if (rx < 2) rx = 2;
    if (ry < 2) ry = 2;

    // 4. 绘制球体
    // deathBurst: 颜色向红色插值
    SDL_Color drawColor = color;
    if (fb.deathBurst > 0.01) {
        double t = fb.deathBurst;
        drawColor.r = (Uint8)(color.r + (255 - color.r) * t);
        drawColor.g = (Uint8)(color.g * (1.0 - t));
        drawColor.b = (Uint8)(color.b * (1.0 - t));
    }

    if (ballTex_[ballIdx]) {
        // 使用预计算纹理 + 实时调色
        SDL_SetTextureColorMod(ballTex_[ballIdx], drawColor.r, drawColor.g, drawColor.b);

        // hookFlash: 提亮
        if (fb.hookFlash > 0.01) {
            int boost = (int)(fb.hookFlash * 80);
            int cr = drawColor.r + boost; if (cr > 255) cr = 255;
            int cg = drawColor.g + boost; if (cg > 255) cg = 255;
            int cb = drawColor.b + boost; if (cb > 255) cb = 255;
            SDL_SetTextureColorMod(ballTex_[ballIdx], (Uint8)cr, (Uint8)cg, (Uint8)cb);
        }

        SDL_Rect dst = {cx - rx, cy - ry, rx * 2, ry * 2};
        SDL_RenderCopy(r, ballTex_[ballIdx], nullptr, &dst);
    } else {
        // 纹理未初始化, 降级为分环近似
        for (int i = 5; i >= 0; i--) {
            int rr_y = ry * (i + 1) / 5;
            int rr_x = rx * (i + 1) / 5;
            double t = (double)i / 5.0;
            int sr = (int)(color.r * (0.4 + t * 0.6) + (255 - color.r) * t * 0.15);
            int sg = (int)(color.g * (0.4 + t * 0.6) + (255 - color.g) * t * 0.15);
            int sb = (int)(color.b * (0.4 + t * 0.6) + (255 - color.b) * t * 0.15);
            if (sr > 255) sr = 255; if (sg > 255) sg = 255; if (sb > 255) sb = 255;
            SDL_SetRenderDrawColor(r, (Uint8)sr, (Uint8)sg, (Uint8)sb, 255);
            // 画椭圆
            for (int y = -rr_y; y <= rr_y; y++) {
                double ratio = (double)rr_x / (double)rr_y;
                int w = (int)(ratio * std::sqrt((double)(rr_y * rr_y - y * y)));
                SDL_RenderDrawLine(r, cx - w, cy + y, cx + w, cy + y);
            }
        }
        // 高光点
        int hx = cx - rx / 4;
        int hy = cy - ry / 4;
        int hr = rx / 5;
        if (hr < 1) hr = 1;
        SDL_SetRenderDrawColor(r, 255, 255, 255, 120);
        drawFillCircle(r, hx, hy, hr, {255, 255, 255, 120});
    }

    // 5. ejectGlow: 边缘白光
    if (fb.ejectGlow > 0.01) {
        int glowR = (int)(rx * (1.0 + fb.ejectGlow * 0.6));
        int glowA = (int)(fb.ejectGlow * 150);
        if (glowA > 255) glowA = 255;
        SDL_SetRenderDrawColor(r, 255, 255, 255, (Uint8)glowA);
        // 画圆环
        for (int y = -glowR; y <= glowR; y++) {
            int w = (int)std::sqrt((double)(glowR * glowR - y * y));
            int w2 = (int)std::sqrt((double)(glowR * glowR - y * y)) - 2;
            if (w2 < 0) w2 = 0;
            if (w > 0) {
                SDL_RenderDrawPoint(r, cx - w, cy + y);
                SDL_RenderDrawPoint(r, cx + w, cy + y);
            }
            if (w2 > 0) {
                SDL_RenderDrawPoint(r, cx - w2, cy + y);
                SDL_RenderDrawPoint(r, cx + w2, cy + y);
            }
        }
    }

    // 6. deathBurst: 喷射红色粒子
    if (fb.deathBurst > 0.05) {
        int count = (int)(fb.deathBurst * 3);
        for (int i = 0; i < count; i++) {
            SparkParticle sp;
            double ang = (rand() % 360) * M_PI / 180.0;
            sp.pos = Vec2(cx + cos(ang) * rx, cy + sin(ang) * ry);
            double spd = 60 + (rand() % 100);
            sp.vel = Vec2(cos(ang) * spd, sin(ang) * spd);
            sp.life = 0.2f + (rand() % 150) / 1000.0f;
            sp.maxLife = sp.life;
            sp.color = {255, (Uint8)(40 + rand() % 60), 0, 255};  // 红色系
            sparks_.push_back(sp);
        }
    }
}

// ============================================================
// CP8: 主绘制函数 (增强版)
// ============================================================

void Pendulum::draw(SDL_Renderer *r, Vec2 pivot, uint64_t frame)
{
    Vec2 joint = jointPos(pivot);
    Vec2 tip   = tipPos(pivot);

    // 缓存支点和球心世界坐标 (供 triggerHookHit 等外部调用使用)
    pivot_ = pivot;
    ballCenters_[0] = joint;
    ballCenters_[1] = tip;

    const int ROD_W = 3;  // 细杆基础宽度

    // ── 脉搏亮环杆: 细实杆 + 亮环沿杆循环移动 ──
    auto pulseRod = [&](Vec2 a, Vec2 b, Uint8 br, Uint8 bg, Uint8 bb, float phaseOff) {
        double dx = b.x - a.x, dy = b.y - a.y;
        double len = sqrt(dx * dx + dy * dy);
        if (len < 1.0) return;
        double dirx = dx / len, diry = dy / len;   // 杆方向单位向量
        double px = -diry, py = dirx;              // 法向量

        // 脉冲位置: 0.5s 周期, A→B 循环
        float t_pulse = fmodf((float)SDL_GetTicks() * 0.001f + phaseOff, 0.5f) / 0.5f;  // 0~1
        double pulsePos = t_pulse * len;
        double pulseRadius = 30.0;  // 影响范围 px

        int steps = (int)len;
        for (int s = 0; s < steps; s++) {
            double dist = (double)s;
            // 距离脉冲位置的衰减
            double pd = fabs(dist - pulsePos);
            double glow = (pd < pulseRadius) ? (1.0 - pd / pulseRadius) : 0.0;
            glow = glow * glow;  // 平方衰减, 更集中

            int cx = (int)(a.x + dirx * s);
            int cy = (int)(a.y + diry * s);

            // 当前点宽度: 基础宽度 + 脉冲加粗
            int halfW = ROD_W / 2 + (int)(glow * 2.0);

            // 颜色: 基础色 → 脉冲时亮白
            Uint8 cr = (Uint8)(br + (255 - br) * glow);
            Uint8 cg = (Uint8)(bg + (255 - bg) * glow);
            Uint8 cb = (Uint8)(bb + (255 - bb) * glow);
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);

            // 沿法向量画短线 (模拟粗像素)
            SDL_RenderDrawLine(r,
                (int)(cx + px * halfW), (int)(cy + py * halfW),
                (int)(cx - px * halfW), (int)(cy - py * halfW));
        }
    };

    // L1 杆: 支点 → 关节 (亮白基色)
    pulseRod(pivot, joint, 220, 220, 240, 0.0f);
    // L2 杆: 关节 → 端点 (略暗基色, 相位偏移 0.25s 交错)
    pulseRod(joint, tip, 180, 180, 220, 0.25f);

    // ── 更新所有效果系统 ──
    double dt = 1.0 / 60.0;  // 近似帧时间
    updateFeedback(dt);
    updateRipples(dt);
    updateSparks(dt);

    // ── 记录残影 ──
    recordTrail(joint, tip);

    // ── 计算球体颜色 ──
    SDL_Color col1 = computeBallColor(0, state_.w1);
    SDL_Color col2 = computeBallColor(1, state_.w2);

    // ── 动能 (用于 Bloom) ──
    double kinetic = Physics::kinetic(state_, params_);
    double bloomIntensity = std::min(kinetic / 50000.0, 1.0);
    bloomIntensity = 0.1 + 0.9 * bloomIntensity;

    // ── 球心数组 (供波纹使用) ──
    Vec2 ballCenters[2] = {joint, tip};

    // ═══════════════════════════════════════
    // 绘制顺序: 残影 → Bloom → 球体 → 波纹 → 火花
    // ═══════════════════════════════════════

    // ── M1 残影 ──
    drawTrail(r, 0, (int)params_.R1, col1);
    // ── M2 残影 ──
    drawTrail(r, 1, (int)params_.R2, col2);

    // ── M1 Bloom ──
    drawBloom(r, (int)joint.x, (int)joint.y, (int)params_.R1, col1, bloomIntensity);
    // ── M2 Bloom ──
    drawBloom(r, (int)tip.x, (int)tip.y, (int)params_.R2, col2, bloomIntensity);

    // ── 支点 ──
    drawFillCircle(r, (int)pivot.x, (int)pivot.y, 4, {255, 255, 100, 255});

    // ── M1 球体 ──
    drawSphere(r, (int)joint.x, (int)joint.y, (int)params_.R1,
               col1, state_.w1, feedback_[0], 0, frame);
    // ── M2 球体 ──
    drawSphere(r, (int)tip.x, (int)tip.y, (int)params_.R2,
               col2, state_.w2, feedback_[1], 1, frame);

    // ── 波纹 ──
    drawRipples(r, ballCenters);

    // ── 火花粒子 ──
    emitSparks(joint, (int)params_.R1, state_.w1, col1);
    emitSparks(tip,   (int)params_.R2, state_.w2, col2);
    drawSparks(r);
}

// ============================================================
// 原始 drawFillCircle
// ============================================================

void Pendulum::drawFillCircle(SDL_Renderer *r, int cx, int cy,
                              int radius, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int y = -radius; y <= radius; y++)
    {
        int w = (int)std::sqrt(radius * radius - y * y);
        SDL_RenderDrawLine(r, cx - w, cy + y,
                           cx + w, cy + y);
    }
}
