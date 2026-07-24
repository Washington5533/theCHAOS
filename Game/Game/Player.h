#pragma once
#include <SDL.h>
#include <vector>
#include <cstdlib>
#include "Vec2.h"
#include <cmath>
class Pendulum;
struct SDL_Renderer;
enum class State // 玩家状态
{
    ON_ROD,
    FLY,    // 飞~起来
    HOOKED, // 钩子
    DEAD    // 菜
};
enum class SurfaceSeg
{
    L1_ROD,  // L1 杆, t∈[0,1], 0=支点端, 1=关节端
    M1_BALL, // m1 关节球面, t∈[0,1], 0=接L1侧(顶), 1=接L2侧(顶)
    L2_ROD,  // L2 杆, t∈[0,1], 0=关节端, 1=末端端
    M2_BALL, // m2 末端球面, t∈[0,1], 0=接L2侧(顶), 1=末端正上方

};

// CP4: 摆锤表面点查询结果
struct SurfacePoint {
    SurfaceSeg seg;
    double t;
    Vec2 worldPos;
    double distance;
    double rodSide = 1.0;   // 杆面侧: +1 或 -1
 };

struct Particle { Vec2 pos, vel; float life; };
struct ScoreSpark { Vec2 pos, vel; float life, maxLife; };


class Player
{
public:
    Player() { reset_Player(); }
    void reset_Player(SurfaceSeg seg = SurfaceSeg::L2_ROD, double t = 0.5);

    ~Player() {};

    // 每帧更新,cp3已重建
    void update(double dt, Pendulum &p, Vec2 pivot);

    // 控制接口
    void moveLeft();
    void moveRight();

    // 状态查询
    Vec2 position() const; // 脚底世界坐标
    State state() const { return state_; }

    // 渲染,待重建轨迹线
    void draw(SDL_Renderer *r) const;

    void jump(Pendulum &p, Vec2 pivot); // 跳跃

    // CP5: 钩锁
    void hook(Vec2 mouseWorld, Pendulum &p, Vec2 pivot); // 发射钩爪
    void releaseHook();                                         // W打断钩锁
    void die();                                                 // CP7: 触发死亡
    double velY() const { return vel_.y; }                      // 供 Game 检测平台
    double speed() const { return sqrt(vel_.x*vel_.x + vel_.y*vel_.y); }
    void landOnPlatform(double standY);                         // 落在平台上
    void addScore(int n) { score_ += n; if (score_ < 0) score_ = 0; }
    void resetScore() { score_ = 0; }
    void emitScoreSparks();  // CP8: 从角色位置喷射得分离子粒子
    int  score() const { return score_; }
    void adjGravity(double d) { gravScale_ += d; if (gravScale_ < 0.1) gravScale_ = 0.1; if (gravScale_ > 3.0) gravScale_ = 3.0; }
    double gravityScale() const { return gravScale_; }

private:
    // --- 步行面定位 ---
    SurfaceSeg seg_ = SurfaceSeg::L1_ROD;
    State state_ = State::ON_ROD;
    Vec2 vel_;
    double t_ = 0.5;
    double target_t_ = 0.5;
    double rodSide_ = 1.0;                             // 杆侧: +1=右侧, -1=左侧
    bool   inputLeft_ = false, inputRight_ = false; // A/D 持续输入
    int    flyFrames_ = 0;                          // CP4: 跳跃后保护计数
    Uint64 deathTimer_ = 0;                         // CP7: 死亡时刻 (ms)
    int    score_ = 0;                              // 得分
    double gravScale_ = 1.0;                        // 玩家重力倍率
    std::vector<Particle> particles_;               // 死亡粒子
    std::vector<ScoreSpark> scoreSparks_;             // CP8: 得分离子粒子
    Vec2 trail_[16]; int trailHead_=0, trailCnt_=0; // 运动残影 (16帧≈0.5s)
    Pendulum *pendulumRef_ = nullptr;  // CP8: 当前帧的摆锤引用 (供warpTo触发反馈)

    // --- 钩锁状态 ---
    SurfaceSeg hookSeg_ = SurfaceSeg::L1_ROD;       // 钩点所在段
    double hookT_ = 0.5;                            // 钩点段内参数
    double hookMaxLen_ = 0.0;                       // 绳长上限
    Vec2   hookWorldPos_;                           // 钩点世界坐标(缓存,供draw)

    // --- 钩锁充能 (CP6) ---
    struct HookSlot { bool ready = true; float timer = 0.0f; };
    HookSlot slots_[2];
    static constexpr float HOOK_COOLDOWN = 0.5f;    // 每发冷却时间
    void updateCooldowns(double dt);
    void drawHUD(SDL_Renderer *r) const;

    // --- 世界坐标 ---
    Vec2 pos_;

    // --- 内部方法 ---

    Vec2 footPos(const Pendulum &p, Vec2 pivot) const;
    void warpTo(SurfaceSeg seg, double t); // 强制切段 (CP4/CP5 使用)

    Vec2 footVel(const Pendulum &p, Vec2 pivot) const;      // 速度继承
    void whetherSlide(const Pendulum &p, Vec2 pivot) const; // 是否下滑
    void updateslide(const Pendulum &p, Vec2 pivot);        // 下滑

    // CP4: 查找摆锤上距目标点最近的表面点
    SurfacePoint findNearestSurface(const Pendulum &p, Vec2 pivot, Vec2 target) const;

    // CP5: 计算任意段/参数的表面世界坐标 (rodSide=±1选杆侧)
    Vec2 surfacePosAt(SurfaceSeg seg, double t, const Pendulum &p, Vec2 pivot,
                      double rodSide = 1.0) const;

    // CP5: 球体碰撞 — 下半区硬反弹, 上半区软推出
    void resolveBallCollision(Vec2 ballCenter, double radius);

    void updateFly(double dt, const Pendulum &p, Vec2 pivot);   // 起飞~更新
    void updateOnRod(double dt, const Pendulum &p, Vec2 pivot); // 在杆上或球上更新
    void updateHooked(double dt, const Pendulum &p, Vec2 pivot); // CP4: 钩子抓回后稳定
    void updateDead(double dt, const Pendulum &p, Vec2 pivot);   // CP4: 死亡状态

    constexpr static double SLIDE_FACTOR = 0.25;           // 下滑速度系数
    constexpr static double ROD_TILT_THRESHOLD = 0.5236;   // π/6 ≈ 30°
    constexpr static double JUMP_BOOST = 400.0;            // W键向上初速度 (px/s)
    constexpr static double WALK_SPEED = 0.2;              // 每帧 t 增量
    constexpr static double DAMPING = 0.85;                // 平滑系数
    constexpr static double ROD_OFFSET = 12.0;             // 杆面偏移 (px) +50%

    // CP4 新增常量
    constexpr static double BALL_SLIDE_FACTOR = 0.0;         // CP4: 球面不下滑,自由站立
    constexpr static double GRAB_DISTANCE = 18.0;          // 飞行抓回最大距离 (px)
    constexpr static double DEAD_Y = 1000.0;                // 死亡 Y 阈值
    constexpr static double BALL_THROW_SPEED = 200.0;      // 球面甩出线速度阈值 (px/s), R=50等效|ω|>4
    constexpr static double ROD_THROW_ENABLED = false;     // CP4: 杆上甩出暂禁用
    constexpr static double WARP_HYSTERESIS = 0.06;       // 段切换防抖 margin (曾=0.015太小导致边界振荡)
    constexpr static int FLY_GRACE_FRAMES = 15;           // 杆上跳跃保护帧数 (~267ms@60fps)
    constexpr static int FLY_GRACE_BALL = 10;              // 球面保护偏移: 球脱落保护=15-10=5帧, 足够脱离球面

    // CP5 钩锁常量
    constexpr static double HOOK_MAX_RANGE = 320.0;        // 最大射程 (px)
    constexpr static double HOOK_K = 7.0;                  // 弹性系数
    constexpr static double HOOK_REST = 8.0;               // 自然绳长 (px)
    constexpr static double HOOK_MAX_STRETCH = 1.4;        // 最大拉伸倍率
    constexpr static double HOOK_SNAP_DIST = 5.0;          // 自动抓回距离 (px)

    // CP6: 球捕获开关
    bool ballCaptureOn_ = true;
public:
    void toggleBallCapture() { ballCaptureOn_ = !ballCaptureOn_; }
    bool isBallCaptureOn() const { return ballCaptureOn_; }
};
