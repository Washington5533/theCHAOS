#include "Game.h"

// ============================================================
//  参数重置 & 生效位置索引
// ============================================================
//
// [构造时 — 自动调用]
//   Game::Game() — 成员默认构造:
//     Pendulum::Pendulum()          → Pendulum.cpp:5   reset()
//     Player::Player()              → Player.h:25       reset_Player(L2_ROD, 0.5)
//   Physics::Params 默认值          → Physics.h:26-31   L1=150 L2=120 R1=22 R2=18
//
// [运行时 — R 键]
//   Game::handleInput() SDLK_r      → Game.cpp:62       pendulum_.reset()
//                                                       player_.reset_Player(L2_ROD, 0.5)
//
// [运行时 — 1~5 键]
//   Game::handleInput() SDLK_1~5    → Game.cpp:67-69    pendulum_.setPreset(N)
//
// [运行时 — 手动调参]
//   pendulum_.setParams(R1,R2,L1,L2)→ Pendulum.h:22     改球半径+杆长
//   pendulum_.params().R1 = X       → Pendulum.h:16     直接改单个参数
//   pendulum_.params().g  = X       →                    改重力加速度
//   player_.reset_Player(seg, t)    → Player.h:26       改出生段/位置
//
// [物理每帧]
//   Game::update() dt               → Game.cpp:87       pendulum_.step(dt,8)
//                                                       player_.update(dt,...)
// ============================================================

int main() {
    Game game;
    if (!game.init()) return 1;
    game.run();
    game.shutdown();
    return 0;
}