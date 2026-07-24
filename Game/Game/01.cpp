// #include <SDL.h>
// #include "Pendulum.h"

// int main(int argc, char* argv[]) {
//     // ---- 初始化 ----
//     SDL_Init(SDL_INIT_VIDEO);

//     SDL_Window* window = SDL_CreateWindow(
//         "Chaos - Checkpoint 1",
//         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
//         800, 600, 0);

//     SDL_Renderer* renderer = SDL_CreateRenderer(
//         window, -1, SDL_RENDERER_ACCELERATED);

//     // ---- 游戏对象 ----
//     Pendulum pendulum;

//     pendulum.setPreset(2);

//     Vec2 pivot = { 400, 120 };   // 支点固定在屏幕上方居中
//     bool running = true;
//     bool paused = false;
//     Uint64 lastTick = SDL_GetTicks64();

//     // ---- 主循环 ----
//     while (running) {
//         // 1. 事件
//         SDL_Event e;
//         while (SDL_PollEvent(&e)) {
//             if (e.type == SDL_QUIT) running = false;

//             if (e.type == SDL_KEYDOWN) {
//                 switch (e.key.keysym.sym) {
//                 case SDLK_ESCAPE: running = false; break;
//                 case SDLK_r:      pendulum.reset(); break;
//                 case SDLK_SPACE:  paused = !paused; break;
//                 case SDLK_1:      pendulum.setPreset(0); break;
//                 case SDLK_2:      pendulum.setPreset(1); break;
//                 case SDLK_3:      pendulum.setPreset(2); break;
//                 case SDLK_4:      pendulum.setPreset(3); break;
//                 case SDLK_5:      pendulum.setPreset(4); break;
//                 }
//             }
//         }

//         // 2. 时间
//         Uint64 now = SDL_GetTicks64();
//         double dt = (now - lastTick) / 1000.0;
//         lastTick = now;
//         if (dt > 0.05) dt = 0.05;  // 防螺旋

//         // 3. 物理
//         if (!paused)
//             pendulum.step(dt, 8);

//         // 4. 绘制
//         SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
//         SDL_RenderClear(renderer);

//         pendulum.draw(renderer, pivot, now);

//         SDL_RenderPresent(renderer);

//         // 5. 控制帧率
//         SDL_Delay(1);
//     }

//     // ---- 清理 ----
//     SDL_DestroyRenderer(renderer);
//     SDL_DestroyWindow(window);
//     SDL_Quit();
//     return 0;
// }