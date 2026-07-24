#pragma once
#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_ttf.h"
#include "player.h"
#include"Physics.h"
#include "Pendulum.h"
#include "Stage.h"
// 游戏主类：管理游戏生命周期、主循环和渲染
class Game
{
public:
    Game() {};  // 构造函数：初始化成员变量
    ~Game() {}; // 析构函数：清理资源

    bool init();     // 初始化SDL、创建窗口和渲染器
    void run();      // 启动游戏主循环
    void shutdown(); // 关闭游戏并释放所有资源

private:
    SDL_Window *window_ = nullptr;     // SDL窗口指针
    SDL_Renderer *renderer_ = nullptr; // SDL渲染器指针
    Pendulum pendulum_;                // 钟摆对象
    Player player_;                    // 玩家对象
    Stage stage_;                      // CP7: 尖刺场景
    Vec2 pivot_ = {500, 300};          // 钟摆支点坐标
    bool paused_ = false;              // 游戏暂停状态
    bool running_ = false;             // 游戏运行状态
    Uint64 lastTick_ = 0;              // 上一帧的时间戳（用于计算dt）
    Vec2 mouseWorld_ = {0, 0};         // CP5: 鼠标世界坐标
    double survivalTime_ = 0.0;        // 存活计时 (秒)
    TTF_Font* font_ = nullptr;             // TTF 字体指针
    TTF_Font* fontCN_ = nullptr;           // 中文字体指针

    // ── 开场介绍状态 ──
    bool introActive_ = true;          // 是否处于开场阶段
    double introTimer_ = 0.0;          // 开场计时器(秒)
    int introCharIndex_ = 0;           // 打字机已显示字符数
    double introFadeAlpha_ = 0.0;      // 黑色蒙版透明度
    bool introTextDone_ = false;       // 文字是否全部显示完毕
    static const int INTRO_LINES = 6;  // 开场文案行数

    // ── 胜利状态 ──
    bool victory_ = false;             // 是否达成胜利条件
    double victoryTimer_ = 0.0;        // 胜利动画计时器(秒)

    void handleInput();        // 处理用户输入（键盘/鼠标事件）
    void update(double dt);    // 更新游戏逻辑（dt为时间增量）
    void render(uint64_t now); // 渲染当前帧（now为当前时间戳）
};