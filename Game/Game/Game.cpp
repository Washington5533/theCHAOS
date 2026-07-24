#include "Game.h"
#include <string>
#include <vector>

#include <stdio.h>
#include <math.h>

// 3x5 点阵数字绘制 (复用)
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
// 3x5 点阵字母字体 (A-Z, 仅用到部分)
static const bool ALPHA[26][5][3] = {
    {{0,1,0},{1,0,1},{1,1,1},{1,0,1},{1,0,1}}, // A
    {{1,1,0},{1,0,1},{1,1,0},{1,0,1},{1,1,0}}, // B
    {{0,1,1},{1,0,0},{1,0,0},{1,0,0},{0,1,1}}, // C
    {{1,1,0},{1,0,1},{1,0,1},{1,0,1},{1,1,0}}, // D
    {{1,1,1},{1,0,0},{1,1,0},{1,0,0},{1,1,1}}, // E
    {{1,1,1},{1,0,0},{1,1,0},{1,0,0},{1,0,0}}, // F
    {{0,1,1},{1,0,0},{1,0,1},{1,0,1},{0,1,1}}, // G
    {{1,0,1},{1,0,1},{1,1,1},{1,0,1},{1,0,1}}, // H
    {{0,1,0},{0,1,0},{0,1,0},{0,1,0},{0,1,0}}, // I
    {{0,0,1},{0,0,1},{0,0,1},{1,0,1},{0,1,0}}, // J
    {{1,0,1},{1,1,0},{1,0,0},{1,1,0},{1,0,1}}, // K
    {{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,1,1}}, // L
    {{1,0,1},{1,1,1},{1,0,1},{1,0,1},{1,0,1}}, // M
    {{1,0,1},{1,1,1},{1,1,1},{1,0,1},{1,0,1}}, // N
    {{0,1,0},{1,0,1},{1,0,1},{1,0,1},{0,1,0}}, // O
    {{1,1,0},{1,0,1},{1,1,0},{1,0,0},{1,0,0}}, // P
    {{0,1,0},{1,0,1},{1,0,1},{1,1,1},{0,1,1}}, // Q
    {{1,1,0},{1,0,1},{1,1,0},{1,0,1},{1,0,1}}, // R
    {{0,1,1},{1,0,0},{0,1,0},{0,0,1},{1,1,0}}, // S
    {{1,1,1},{0,1,0},{0,1,0},{0,1,0},{0,1,0}}, // T
    {{1,0,1},{1,0,1},{1,0,1},{1,0,1},{0,1,0}}, // U
    {{1,0,1},{1,0,1},{1,0,1},{0,1,0},{0,1,0}}, // V
    {{1,0,1},{1,0,1},{1,1,1},{1,1,1},{1,0,1}}, // W
    {{1,0,1},{1,0,1},{0,1,0},{1,0,1},{1,0,1}}, // X
    {{1,0,1},{1,0,1},{0,1,0},{0,1,0},{0,1,0}}, // Y
    {{1,1,1},{0,0,1},{0,1,0},{1,0,0},{1,1,1}}, // Z
};
static void drawText(SDL_Renderer* r, int x, int y, const char* s, int dot=2) {
    int ox = x;
    for (int i = 0; s[i]; i++) {
        char c = s[i];
        if (c == ' ') { ox += 3*(dot+1); continue; }
        if (c == '-') { // 横线
            SDL_Rect d = {ox, y + 2*(dot+1), 3*dot, dot};
            SDL_RenderFillRect(r, &d);
            ox += 3*(dot+1); continue;
        }
        int idx = -1;
        if (c >= 'A' && c <= 'Z') idx = c - 'A';
        else if (c >= 'a' && c <= 'z') idx = c - 'a';
        if (idx < 0) { ox += 3*(dot+1); continue; }
        for (int row=0; row<5; row++) for (int col=0; col<3; col++)
            if (ALPHA[idx][row][col]) {
                SDL_Rect d = {ox + col*(dot+1), y + row*(dot+1), dot, dot};
                SDL_RenderFillRect(r, &d);
            }
        ox += 3*(dot+1) + 1;
    }
}

static void drawNum(SDL_Renderer* r, int x, int y, int val, int dot=3) {
    if (val == 0) {
        for (int row=0; row<5; row++) for (int col=0; col<3; col++)
            if (DIGITS[0][row][col]) {
                SDL_Rect d={x+col*(dot+1), y+row*(dot+1), dot, dot};
                SDL_RenderFillRect(r, &d);
            }
        return;
    }
    char buf[12]; int len=0, n=val;
    while (n>0) { buf[len++]='0'+(n%10); n/=10; }
    for (int i=len-1; i>=0; i--) {
        int dg=buf[i]-'0';
        for (int row=0; row<5; row++) for (int col=0; col<3; col++)
            if (DIGITS[dg][row][col]) {
                SDL_Rect d={x+col*(dot+1), y+row*(dot+1), dot, dot};
                SDL_RenderFillRect(r, &d);
            }
        x += 3*(dot+1) + 2;
    }
}

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow("chaos - Checkpoint 5",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1000, 940, 0);
    if (!window_) {
        printf("Window failed: %s\n", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1,
        SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
        printf("Renderer failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    // TTF 字体初始化
    if (TTF_Init() < 0) {
        printf("TTF_Init failed: %s\n", TTF_GetError());
        return false;
    }
    font_ = TTF_OpenFont("C:\\Windows\\Fonts\\consola.ttf", 14);
    if (!font_) {
        printf("TTF_OpenFont failed: %s\n", TTF_GetError());
        return false;
    }
    // 中文字体 (用于开场介绍)
    // 优先尝试 simhei.ttf (单字体文件, 无索引问题)
    fontCN_ = TTF_OpenFont("C:\\Windows\\Fonts\\simhei.ttf", 22);
    if (!fontCN_) {
        printf("[Warn] simhei.ttf not found, trying msyh.ttc index 0\n");
        fontCN_ = TTF_OpenFontIndex("C:\\Windows\\Fonts\\msyh.ttc", 22, 0);
    }
    if (!fontCN_) {
        printf("[Warn] msyh.ttc index 0 failed, trying msyh.ttc index 1\n");
        fontCN_ = TTF_OpenFontIndex("C:\\Windows\\Fonts\\msyh.ttc", 22, 1);
    }
    if (!fontCN_) {
        printf("[Warn] No Chinese font available, intro text may not render\n");
    } else {
        printf("[Info] Chinese font loaded OK\n");
    }

    pendulum_.initTextures(renderer_);  // CP8: 预计算球体纹理

    stage_.regenerate();  // CP7: 初始场景
    return true;
}

void Game::shutdown() {
    pendulum_.destroyTextures();  // CP8: 释放球体纹理
    if (font_) TTF_CloseFont(font_);
    if (fontCN_) TTF_CloseFont(fontCN_);
    TTF_Quit();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_)   SDL_DestroyWindow(window_);
    SDL_Quit();
}
void Game::run() {
    running_ = true;
    lastTick_ = SDL_GetTicks64();

    while (running_) {
        handleInput();

        // 时间
        Uint64 now = SDL_GetTicks64();
        double dt = (now - lastTick_) / 1000.0;
        lastTick_ = now;
        if (dt > 0.05) dt = 0.05;

        // ── 开场介绍阶段逻辑 ──
        if (introActive_) {
            introTimer_ += dt;
            // 阶段1: 前1.5秒仅淡入黑色蒙版
            if (introTimer_ < 1.5) {
                introFadeAlpha_ = (introTimer_ / 1.5) * 240.0;
                if (introFadeAlpha_ > 240.0) introFadeAlpha_ = 240.0;
            } else {
                introFadeAlpha_ = 240.0;
                // 阶段2: 打字机效果 (每0.06秒显示一个字符)
                double typeTime = introTimer_ - 1.5;
                introCharIndex_ = (int)(typeTime / 0.06);
                // 总字符数 (6行, 每行约14个UTF-8中文字符)
                int totalChars = 6 * 16;  // 每行预留16字符位
                if (introCharIndex_ >= totalChars) {
                    introCharIndex_ = totalChars;
                    introTextDone_ = true;
                }
            }
        }

        update(dt);
        render(now);
    }
}

void Game::handleInput() {
    static bool aHeld = false, dHeld = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            running_ = false;
        } else if (e.type == SDL_KEYDOWN) {
            // ── 开场阶段: 空格跳过 ──
            if (introActive_) {
                if (e.key.keysym.sym == SDLK_SPACE) {
                    introActive_ = false;  // 正式开始游戏
                }
                continue;  // 开场阶段忽略其他所有输入
            }
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE: running_ = false; break;
            case SDLK_r:
                pendulum_.reset(); player_.reset_Player(); player_.resetScore();
                stage_.regenerate(); survivalTime_ = 0.0; victory_ = false;
                victoryTimer_ = 0.0;
                break;
            case SDLK_SPACE:  paused_ = !paused_; break;
            case SDLK_w:      if (!e.key.repeat) player_.jump(pendulum_, pivot_); break;
            case SDLK_a:      aHeld = true;  break;
            case SDLK_d:      dHeld = true;  break;
            case SDLK_1: case SDLK_2: case SDLK_3:
            case SDLK_4: case SDLK_5:
                pendulum_.setPreset(e.key.keysym.sym - SDLK_1);
                break;
            case SDLK_f:
                if (!e.key.repeat) {
                    player_.toggleBallCapture();
                }
                break;
            case SDLK_q:
                if (!e.key.repeat) player_.adjGravity(-0.1);
                break;
            case SDLK_e:
                if (!e.key.repeat) player_.adjGravity(+0.1);
                break;
            }
        } else if (e.type == SDL_KEYUP) {
            switch (e.key.keysym.sym) {
            case SDLK_a: aHeld = false; break;
            case SDLK_d: dHeld = false; break;
            }
        } else if (e.type == SDL_MOUSEMOTION) {
            // CP5: 跟踪鼠标世界坐标
            mouseWorld_.x = (double)e.motion.x;
            mouseWorld_.y = (double)e.motion.y;
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            // CP5: 左键发射钩锁
            if (e.button.button == SDL_BUTTON_LEFT)
                player_.hook(mouseWorld_, pendulum_, pivot_);
        }
    }

    if (aHeld) player_.moveLeft();
    if (dHeld) player_.moveRight();
}


void Game::update(double dt) {
    // ── 胜利后仅推进动画计时, 不更新物理 ──
    if (victory_) {
        victoryTimer_ += dt;
        return;
    }

    if (paused_) return;

    static int prevState = -1;
    static double survMark = 0.0;  // 存活计时标记

    pendulum_.step(dt, 32);
    player_.update(dt, pendulum_, pivot_);

    int curState = (int)player_.state();

    // ── 存活计时 (非死亡状态) ──
    if (curState != 3) {  // 3 = DEAD
        survivalTime_ += dt;
        if (survivalTime_ - survMark >= 15.0) {
            survMark += 15.0;
            player_.addScore(1);
            pendulum_.triggerScore();   // CP8: 得分脉冲反馈
            player_.emitScoreSparks();  // CP8: 角色离子迸溅
        }
    }

    // ── 死亡自动重置 → 扣分 + 刷新场景 ──
    if (prevState == 3 && curState != 3) {
        player_.addScore(-1);
        stage_.regenerate();
        survivalTime_ = 0.0;
        survMark = 0.0;
    }
    prevState = curState;

    // ── 尖刺碰撞 ──
    if (stage_.checkSpikeCollision(player_.position(), 10, 10))
        player_.die();

    // ── 平台站立 ──
    double standY;
    if (stage_.checkPlatformTop(player_.position(), 10, 10,
                                 player_.velY(), standY))
        player_.landOnPlatform(standY);

    // ── 平台触碰 = 胜利 ──
    static int lastTouchedPlat = -1;
    int touchIdx = stage_.checkPlatformTouch(player_.position(), 10, 10);
    if (touchIdx >= 0 && touchIdx != lastTouchedPlat) {
        player_.addScore(1);
        pendulum_.triggerScore();   // CP8: 得分脉冲反馈
        player_.emitScoreSparks();  // CP8: 角色离子迸溅
        stage_.regeneratePlatformAt(touchIdx);  // 仅刷新被触碰的平台
    }
    lastTouchedPlat = touchIdx;

    // ── 胜利检测: 分数达到15 ──
    if (player_.score() >= 15 && !victory_) {
        victory_ = true;
        victoryTimer_ = 0.0;
        player_.emitScoreSparks();  // 触发粒子效果
        pendulum_.triggerScore();
    }
}

// TTF 文本绘制辅助 (支持 UTF-8 中文)
static void drawTTF(SDL_Renderer* r, TTF_Font* f, const char* text,
                    int x, int y, SDL_Color c) {
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, text, c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void drawTextBox(SDL_Renderer* r, TTF_Font* f,
                        int bx, int by, int bw, int bh,
                        const char* title, const char** lines, int lineCount,
                        SDL_Color titleC, SDL_Color textC,
                        SDL_Color bgC, SDL_Color borderC) {
    // 背景
    SDL_Rect bg = {bx, by, bw, bh};
    SDL_SetRenderDrawColor(r, bgC.r, bgC.g, bgC.b, bgC.a);
    SDL_RenderFillRect(r, &bg);
    // 边框
    SDL_SetRenderDrawColor(r, borderC.r, borderC.g, borderC.b, borderC.a);
    SDL_RenderDrawRect(r, &bg);
    // 标题
    int ty = by + 8;
    drawTTF(r, f, title, bx + 12, ty, titleC);
    ty += 24;
    // 分隔线
    SDL_SetRenderDrawColor(r, borderC.r, borderC.g, borderC.b, (Uint8)(borderC.a / 2));
    SDL_RenderDrawLine(r, bx + 8, ty, bx + bw - 8, ty);
    ty += 6;
    // 内容行
    for (int i = 0; i < lineCount; i++) {
        drawTTF(r, f, lines[i], bx + 12, ty, textC);
        ty += 20;
    }
}

void Game::render(uint64_t now) {
    SDL_SetRenderDrawColor(renderer_, 15, 15, 25, 255);
    SDL_RenderClear(renderer_);

    // ══════════════════════════════════════════════
    //  开场介绍画面
    // ══════════════════════════════════════════════
    if (introActive_) {
        // 黑色蒙版
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, (Uint8)introFadeAlpha_);
        SDL_Rect fullScreen = {0, 0, 1000, 940};
        SDL_RenderFillRect(renderer_, &fullScreen);

        if (fontCN_ && introTimer_ >= 1.5) {
            // 开场文案 (润色版)
            const char* introLines[] = {
                "你是一艘探索无人飞船，",
                "如今被困于一个高维物理空间——",
                "或许是某种高维生物的实验场？",
                "无论如何，请务必存活并收集能量。",
                "当能量充足时，激活传送核心，",
                "逃离此地，将关键情报带回！"
            };

            int centerX = 500;
            int startY = 280;
            int lineHeight = 48;
            int charsPerLine = 16;

            // 标题
            double titleAlpha = (introTimer_ - 1.5) / 0.8;
            if (titleAlpha > 1.0) titleAlpha = 1.0;
            Uint8 ta = (Uint8)(titleAlpha * 255);
            drawTTF(renderer_, fontCN_, "【 高 维 困 局 】",
                    centerX - 130, startY - 70, {100, 200, 255, ta});

            // 逐行打字机效果
            for (int i = 0; i < 6; i++) {
                int lineStartChar = i * charsPerLine;
                int charsToShow = introCharIndex_ - lineStartChar;
                if (charsToShow <= 0) continue;

                // 获取当前行的 UTF-8 字符
                std::string lineStr(introLines[i]);
                // 按 UTF-8 字符切割 (中文每字3字节)
                int utf8Chars = 0;
                size_t bytePos = 0;
                while (bytePos < lineStr.size() && utf8Chars < charsToShow) {
                    unsigned char ch = lineStr[bytePos];
                    if (ch < 0x80) bytePos += 1;
                    else if ((ch & 0xE0) == 0xC0) bytePos += 2;
                    else if ((ch & 0xF0) == 0xE0) bytePos += 3;
                    else bytePos += 4;
                    utf8Chars++;
                }
                std::string partial = lineStr.substr(0, bytePos);

                // 文字颜色: 柔和的蓝白色
                SDL_Color textC = {190, 215, 240, 230};
                int textW = 0, textH = 0;
                TTF_SizeUTF8(fontCN_, partial.c_str(), &textW, &textH);
                drawTTF(renderer_, fontCN_, partial.c_str(),
                        centerX - textW / 2, startY + i * lineHeight, textC);

                // 当前正在打字的行末尾显示闪烁光标
                if (!introTextDone_ && charsToShow < (int)lineStr.size() / 3) {
                    int cursorAlpha = ((int)(now / 400) % 2) ? 220 : 60;
                    SDL_SetRenderDrawColor(renderer_, 100, 200, 255, (Uint8)cursorAlpha);
                    SDL_Rect cursor = {centerX + textW / 2 + 2,
                                       startY + i * lineHeight + 4,
                                       3, textH - 4};
                    SDL_RenderFillRect(renderer_, &cursor);
                }
            }

            // 文字全部显示后, 显示"按空格开始"提示 (呼吸闪烁)
            if (introTextDone_) {
                float breath = (sinf((float)now * 0.004f) + 1.0f) * 0.5f;
                Uint8 promptA = (Uint8)(120 + breath * 135);
                drawTTF(renderer_, fontCN_, "按 空 格 键 正 式 开 始",
                        centerX - 140, 620, {140, 220, 180, promptA});
            }
        }

        SDL_RenderPresent(renderer_);
        SDL_Delay(1);
        return;  // 开场阶段不渲染游戏内容
    }

    // ══════════════════════════════════════════════
    //  正常游戏渲染
    // ══════════════════════════════════════════════
    stage_.draw(renderer_);            // CP7: 尖刺 ( pendulum 之前)
    pendulum_.draw(renderer_, pivot_, now);
    player_.draw(renderer_);

    // ── 左侧: 速度信息 ──
    const auto& st = pendulum_.state();
    const auto& pr = pendulum_.params();
    int surf1 = (int)(fabs(st.w1) * pr.R1);   // M1 表面线速度
    int surf2 = (int)(fabs(st.w2) * pr.R2);   // M2 表面线速度
    int pspd  = (int)player_.speed();          // 角色实际速度

    // M1 表面速度: 橙色
    SDL_SetRenderDrawColor(renderer_, 255, 160, 60, 255);
    drawNum(renderer_, 10, 90, surf1);
    // M2 表面速度: 蓝色
    SDL_SetRenderDrawColor(renderer_, 100, 200, 255, 255);
    drawNum(renderer_, 10, 112, surf2);
    // 角色速度: 绿→红渐变 (0 → 1000 px/s)
    double t = pspd / 1000.0; if (t > 1.0) t = 1.0;
    int r = (int)(255 * t), g = (int)(255 * (1.0 - t));
    SDL_SetRenderDrawColor(renderer_, r, g, 60, 255);
    drawNum(renderer_, 10, 140, pspd);

    // ── 暂停菜单 ──
    if (paused_) {
        // 层1: 全屏暗色遮罩
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 180);
        SDL_Rect overlay = {0, 0, 1000, 940};
        SDL_RenderFillRect(renderer_, &overlay);

        int cx = 500, cy = 430;
        float breath = (sinf((float)now * 0.003f) + 1.0f) * 0.5f;
        int R = 28 + (int)(breath * 6);

        // 层2: 外层呼吸光晕
        int glowR = R + 12 + (int)(breath * 8);
        int glowA = (int)(20 + breath * 25);
        for (int y = -glowR; y <= glowR; y++) {
            int halfW = glowR - abs(y);
            if (halfW < 1) continue;
            int a = (int)((double)glowA * (1.0 - (double)abs(y) / glowR));
            if (a < 2) continue;
            SDL_SetRenderDrawColor(renderer_, 80, 160, 220, (Uint8)a);
            SDL_RenderDrawLine(renderer_, cx - halfW, cy + y, cx + halfW, cy + y);
        }

        // 层3: 菱形主体
        for (int y = -R; y <= R; y++) {
            int halfW = R - abs(y);
            double rowT = 1.0 - (double)abs(y) / R;
            Uint8 cr = (Uint8)(60 + rowT * 80);
            Uint8 cg = (Uint8)(120 + rowT * 100);
            Uint8 cb = (Uint8)(200 + rowT * 55);
            SDL_SetRenderDrawColor(renderer_, cr, cg, cb, 230);
            SDL_RenderDrawLine(renderer_, cx - halfW, cy + y, cx + halfW, cy + y);
        }

        // 层4: 内层高亮核心
        int iR = R / 3;
        for (int y = -iR; y <= iR; y++) {
            int halfW = iR - abs(y);
            double rowT = 1.0 - (double)abs(y) / iR;
            Uint8 v = (Uint8)(180 + rowT * 75);
            SDL_SetRenderDrawColor(renderer_, v, v, 255, (Uint8)(200 + (int)(rowT * 55)));
            SDL_RenderDrawLine(renderer_, cx - halfW, cy + y, cx + halfW, cy + y);
        }

        // 层5: 中心白色高光点
        int hlA = (int)(160 + breath * 80);
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, (Uint8)hlA);
        SDL_Rect hl = {cx - 2, cy - 2, 5, 5};
        SDL_RenderFillRect(renderer_, &hl);

        // 暂停标题 (TTF)
        drawTTF(renderer_, fontCN_, "游  戏  暂  停", cx - 110, cy - R - 52,
                {180, 220, 255, 240});

        // ── 左侧文本框: 下落逻辑 ──
        const char* fallLines[] = {
            "从上方落向平台：",
            "  接触平台顶部",
            "  velY > 0 即吸附",
            "",
            "Q/E：调节重力",
            "  0.1x ~ 3.0x",
        };
        drawTextBox(renderer_, fontCN_,
                    20, 160, 300, 180,
                    "下落逻辑", fallLines, 6,
                    {80, 200, 255, 255}, {170, 190, 210, 200},
                    {10, 15, 30, 200}, {60, 140, 200, 180});

        // ── 右侧文本框: 弹射逻辑 ──
        const char* ejectLines[] = {
            "表面速度 > 阈值",
            "  => 沿切线弹出",
            "",
            "W：主动跳离平台",
            "  （从平台顶部）",
        };
        drawTextBox(renderer_, fontCN_,
                    680, 120, 300, 150,
                    "弹射逻辑", ejectLines, 5,
                    {255, 160, 80, 255}, {200, 180, 160, 200},
                    {30, 15, 10, 200}, {200, 120, 60, 180});

        // ── 右侧文本框: 钩锁 ──
        const char* hookLines[] = {
            "2次充能，0.5秒冷却",
            "射程：320像素",
            "张力：绳索拉伸",
            "  0=松弛  1=断裂",
        };
        drawTextBox(renderer_, fontCN_,
                    680, 310, 300, 130,
                    "钩锁", hookLines, 4,
                    {180, 140, 255, 255}, {190, 180, 220, 200},
                    {20, 15, 35, 200}, {140, 100, 200, 180});

        // ── 左上角速度数值 (暂停时重绘, 避免被遮罩覆盖) ──
        // M1 表面速度: 橙色
        int numY1 = 90;
        SDL_SetRenderDrawColor(renderer_, 255, 160, 60, 255);
        drawNum(renderer_, 10, numY1, surf1);
        // M2 表面速度: 蓝色
        int numY2 = 112;
        SDL_SetRenderDrawColor(renderer_, 100, 200, 255, 255);
        drawNum(renderer_, 10, numY2, surf2);
        // 角色速度: 绿→红渐变
        int numY3 = 140;
        int spR = (int)(255 * t), spG = (int)(255 * (1.0 - t));
        SDL_SetRenderDrawColor(renderer_, spR, spG, 60, 255);
        drawNum(renderer_, 10, numY3, pspd);

        // ── 箭头标注: 指向左上角数值表 ──
        int labelX = 80;  // 标注文字起点 x
        // M1: 橙色 "M1 surface speed"
        SDL_SetRenderDrawColor(renderer_, 255, 160, 60, 180);
        SDL_RenderDrawLine(renderer_, labelX - 4, numY1 + 10, labelX, numY1 + 10);
        SDL_RenderDrawLine(renderer_, labelX - 2, numY1 + 7, labelX - 4, numY1 + 10);
        SDL_RenderDrawLine(renderer_, labelX - 2, numY1 + 13, labelX - 4, numY1 + 10);
        drawTTF(renderer_, fontCN_, "M1 表面速度", labelX + 4, numY1 - 2, {255, 160, 60, 220});
        // M2: 蓝色 "M2 surface speed"
        SDL_SetRenderDrawColor(renderer_, 100, 200, 255, 180);
        SDL_RenderDrawLine(renderer_, labelX - 4, numY2 + 10, labelX, numY2 + 10);
        SDL_RenderDrawLine(renderer_, labelX - 2, numY2 + 7, labelX - 4, numY2 + 10);
        SDL_RenderDrawLine(renderer_, labelX - 2, numY2 + 13, labelX - 4, numY2 + 10);
        drawTTF(renderer_, fontCN_, "M2 表面速度", labelX + 4, numY2 - 2, {100, 200, 255, 220});
        // 角色: 绿色 "Player speed"
        SDL_SetRenderDrawColor(renderer_, spR, spG, 60, 180);
        SDL_RenderDrawLine(renderer_, labelX - 4, numY3 + 10, labelX, numY3 + 10);
        SDL_RenderDrawLine(renderer_, labelX - 2, numY3 + 7, labelX - 4, numY3 + 10);
        SDL_RenderDrawLine(renderer_, labelX - 2, numY3 + 13, labelX - 4, numY3 + 10);
        drawTTF(renderer_, fontCN_, "玩家速度", labelX + 4, numY3 - 2, {180, 220, 140, 220});

        // ── 状态信息: 分数 + 时间 ──
        int infoY = cy + R + 28;
        char scoreBuf[48];
        sprintf_s(scoreBuf, "分数：%d", player_.score());
        drawTTF(renderer_, fontCN_, scoreBuf, cx - 90, infoY, {100, 200, 255, 200});

        int timeSec = (int)survivalTime_;
        char timeBuf[48];
        sprintf_s(timeBuf, "时间：%d:%02d", timeSec / 60, timeSec % 60);
        drawTTF(renderer_, fontCN_, timeBuf, cx - 90, infoY + 26, {100, 255, 160, 200});

        // ── 底部中央文本框: 计分规则 ──
        const char* scoreLines[] = {
            "接触平台：+1",
            "存活15秒：+1",
            "死亡：    -1",
        };
        drawTextBox(renderer_, fontCN_,
                    cx - 160, infoY + 56, 320, 110,
                    "计分规则", scoreLines, 3,
                    {100, 255, 180, 255}, {180, 210, 190, 200},
                    {15, 25, 15, 200}, {80, 180, 120, 180});

        // ── 底部键位提示 ──
        int keyY = 850;
        const char* keys[] = {"W-跳跃", "A/D-移动", "鼠标-钩锁", "R-重置", "Q/E-重力", "空格-继续"};
        int keySpacing = 160;
        int totalKeyW = 6 * keySpacing;
        int keyStartX = cx - totalKeyW / 2;
        for (int i = 0; i < 6; i++) {
            drawTTF(renderer_, fontCN_, keys[i], keyStartX + i * keySpacing, keyY - 4,
                    {120, 160, 200, 180});
        }
    }

    // ══════════════════════════════════════════════
    //  胜利画面
    // ══════════════════════════════════════════════
    if (victory_) {
        int ppx = (int)player_.position().x;
        int ppy = (int)player_.position().y - 6;  // 角色视觉中心

        // ── 阶段1: 菱形能量扩散 (0~2.5s) ──
        double vt = victoryTimer_;

        // 暗色遮罩渐变
        double overlayAlpha = vt / 2.0;
        if (overlayAlpha > 1.0) overlayAlpha = 1.0;
        SDL_SetRenderDrawColor(renderer_, 0, 2, 5, (Uint8)(overlayAlpha * 220));
        SDL_Rect full = {0, 0, 1000, 940};
        SDL_RenderFillRect(renderer_, &full);

        // 扩散菱形环 (最多10个活跃环, 每个持续~1.2s)
        double ringInterval = 0.22;
        int maxRings = 10;
        for (int ri = 0; ri < maxRings; ri++) {
            double spawnTime = ri * ringInterval;
            double age = vt - spawnTime;
            if (age < 0) continue;
            double life = 1.2;
            if (age > life) continue;

            double tLife = age / life;  // 0→1
            int radius = (int)(tLife * 350 + 20);  // 从小向大扩散
            int alpha = (int)((1.0 - tLife) * 200);
            int lineW = (int)((1.0 - tLife) * 4 + 1);
            if (alpha < 8) continue;

            // 青绿色菱形
            Uint8 cr = (Uint8)(40 + (int)((1.0 - tLife) * 100));
            Uint8 cg = (Uint8)(180 + (int)((1.0 - tLife) * 75));
            Uint8 cb = (Uint8)(120 + (int)((1.0 - tLife) * 100));

            for (int lw = 0; lw < lineW; lw++) {
                int r = radius + lw;
                int halfA = (int)(alpha * (1.0 - (double)lw / lineW * 0.6));
                SDL_SetRenderDrawColor(renderer_, cr, cg, cb, (Uint8)halfA);
                // 菱形4边
                SDL_RenderDrawLine(renderer_, ppx, ppy - r, ppx + r, ppy);      // 上→右
                SDL_RenderDrawLine(renderer_, ppx + r, ppy, ppx, ppy + r);      // 右→下
                SDL_RenderDrawLine(renderer_, ppx, ppy + r, ppx - r, ppy);      // 下→左
                SDL_RenderDrawLine(renderer_, ppx - r, ppy, ppx, ppy - r);      // 左→上
            }
        }

        // 核心高亮脉动
        double pulse = fabs(sinf(vt * 4.5));
        int coreAlpha = (int)(120 + pulse * 135);
        int coreR = 6 + (int)(pulse * 8);
        for (int y = -coreR; y <= coreR; y++) {
            int hw = coreR - abs(y);
            if (hw < 1) continue;
            double t = 1.0 - (double)abs(y) / coreR;
            Uint8 v = (Uint8)(140 + t * 115);
            SDL_SetRenderDrawColor(renderer_, v, 255, (Uint8)(180 + t * 75), (Uint8)coreAlpha);
            SDL_RenderDrawLine(renderer_, ppx - hw, ppy + y, ppx + hw, ppy + y);
        }
        // 中心白点
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, (Uint8)(180 + (int)(pulse * 75)));
        SDL_Rect dot = {ppx - 2, ppy - 2, 5, 5};
        SDL_RenderFillRect(renderer_, &dot);

        // ── 阶段2: 胜利面板 (2s后显示) ──
        if (vt >= 2.0) {
            double panelFade = (vt - 2.0) / 0.6;
            if (panelFade > 1.0) panelFade = 1.0;
            int pa = (int)(panelFade * 255);

            int pcx = 500, pcy = 430;
            int pbw = 380, pbh = 280;
            int pbx1 = pcx - pbw / 2, pby1 = pcy - pbh / 2;
            int pbx2 = pcx + pbw / 2, pby2 = pcy + pbh / 2;

            // 外层辉光
            int glowColors[3][3] = {{(int)(8*panelFade),(int)(18*panelFade),(int)(8*panelFade)},
                                    {(int)(14*panelFade),(int)(30*panelFade),(int)(14*panelFade)},
                                    {(int)(22*panelFade),(int)(45*panelFade),(int)(22*panelFade)}};
            for (int g = 0; g < 3; g++) {
                int expand = (3 - g) * 3;
                SDL_SetRenderDrawColor(renderer_, glowColors[g][0], glowColors[g][1], glowColors[g][2], (Uint8)pa);
                SDL_Rect gr = {pbx1 - expand, pby1 - expand - 2, pbx2 - pbx1 + 2 * expand, 2};
                SDL_RenderFillRect(renderer_, &gr);
                gr.y = pby2 + expand;
                SDL_RenderFillRect(renderer_, &gr);
                gr = {pbx1 - expand - 2, pby1 - expand, 2, pby2 - pby1 + 2 * expand};
                SDL_RenderFillRect(renderer_, &gr);
                gr.x = pbx2 + expand;
                SDL_RenderFillRect(renderer_, &gr);
            }

            // 主面板
            SDL_SetRenderDrawColor(renderer_, (int)(10*panelFade), (int)(22*panelFade), (int)(10*panelFade), (Uint8)(panelFade * 220));
            SDL_Rect panel = {pbx1, pby1, pbw, pbh};
            SDL_RenderFillRect(renderer_, &panel);
            SDL_SetRenderDrawColor(renderer_, (int)(50*panelFade), (int)(220*panelFade), (int)(100*panelFade), (Uint8)pa);
            SDL_RenderDrawRect(renderer_, &panel);
            SDL_SetRenderDrawColor(renderer_, (int)(25*panelFade), (int)(140*panelFade), (int)(60*panelFade), (Uint8)pa);
            SDL_Rect panel2 = {pbx1 - 2, pby1 - 2, pbw + 4, pbh + 4};
            SDL_RenderDrawRect(renderer_, &panel2);

            // 角落亮点
            SDL_SetRenderDrawColor(renderer_, (int)(80*panelFade), (int)(255*panelFade), (int)(120*panelFade), (Uint8)pa);
            SDL_Rect corner;
            corner = {pbx1 - 1, pby1 - 1, 4, 4}; SDL_RenderFillRect(renderer_, &corner);
            corner = {pbx2 - 3, pby1 - 1, 4, 4}; SDL_RenderFillRect(renderer_, &corner);
            corner = {pbx1 - 1, pby2 - 3, 4, 4}; SDL_RenderFillRect(renderer_, &corner);
            corner = {pbx2 - 3, pby2 - 3, 4, 4}; SDL_RenderFillRect(renderer_, &corner);

            // 标题: 通关成功
            SDL_SetRenderDrawColor(renderer_, (int)(15*panelFade), (int)(60*panelFade), (int)(30*panelFade), (Uint8)(panelFade * 180));
            SDL_Rect titleBg = {pcx - 70, pby1 + 14, 140, 36};
            SDL_RenderFillRect(renderer_, &titleBg);
            drawTTF(renderer_, fontCN_, "通  关  成  功",
                    pcx - 100, pby1 + 18, {(Uint8)(int)(80*panelFade), (Uint8)(int)(255*panelFade), (Uint8)(int)(120*panelFade), (Uint8)pa});

            // 分数
            char buf[64];
            sprintf_s(buf, "分数：%d", player_.score());
            drawTTF(renderer_, fontCN_, buf, pcx - 60, pcy - 14,
                    {(Uint8)(int)(100*panelFade), (Uint8)(int)(255*panelFade), (Uint8)(int)(140*panelFade), (Uint8)pa});

            // 分隔线
            SDL_SetRenderDrawColor(renderer_, (int)(40*panelFade), (int)(80*panelFade), (int)(40*panelFade), (Uint8)(panelFade * 150));
            SDL_RenderDrawLine(renderer_, pbx1 + 20, pcy + 20, pbx2 - 20, pcy + 20);

            // 按 R 重新开始
            drawTTF(renderer_, fontCN_, "按  R  重 新 开 始",
                    pcx - 110, pcy + 34, {(Uint8)(int)(100*panelFade), (Uint8)(int)(255*panelFade), (Uint8)(int)(200*panelFade), (Uint8)pa});

            // 传送核心已激活 (闪烁)
            if (((int)(now / 500) % 2) == 0) {
                drawTTF(renderer_, fontCN_, "传 送 核 心 已 激 活",
                        pcx - 120, pcy + 64, {(Uint8)(int)(60*panelFade), (Uint8)(int)(220*panelFade), (Uint8)(int)(160*panelFade), (Uint8)pa});
            }
        }
    }

    SDL_RenderPresent(renderer_);
    SDL_Delay(1);
}
