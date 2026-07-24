#include "Stage.h"
#include <cstdlib>
#include <ctime>
#include <cmath>

Stage::Stage()
{
    static bool seeded = false;
    if (!seeded) { srand((unsigned)time(nullptr)); seeded = true; }
}

void Stage::regenerate()
{
    spikes_.clear();
    plats_.clear();
    platScored_.clear();

    // 生成尖刺 (始终保持 2 块)
    if (rand() % 100 < 30) {
        // 30% 概率: 1 块顶部倒置 + 1 块下部正置
        Spike spTop;
        spTop.rect.x = 50 + rand() % 850;
        spTop.rect.y = 10 + rand() % 110;    // y: 10~120
        spTop.rect.w = 30 + rand() % 50;
        spTop.rect.h = 12 + rand() % 16;
        spTop.inverted = true;
        spikes_.push_back(spTop);

        Spike spBot;
        spBot.rect.x = 50 + rand() % 850;
        spBot.rect.y = 550 + rand() % 320;   // y: 550~870
        spBot.rect.w = 30 + rand() % 50;
        spBot.rect.h = 12 + rand() % 16;
        spBot.inverted = false;
        spikes_.push_back(spBot);
    } else {
        // 70% 概率: 2 块下部正置
        for (int i = 0; i < 2; i++) {
            Spike sp;
            sp.rect.x = 50 + rand() % 850;
            sp.rect.y = 550 + rand() % 320;   // y: 550~870
            sp.rect.w = 30 + rand() % 50;
            sp.rect.h = 12 + rand() % 16;
            sp.inverted = false;
            spikes_.push_back(sp);
        }
    }

    // 得分点 A: 上部区域 (y: 120~370)
    {
        SafeBlock pb;
        pb.rect.x = 100 + rand() % 700;
        pb.rect.y = 120 + rand() % 250;   // y: 120~370
        pb.rect.w = 30 + rand() % 30;
        pb.rect.h = 8 + rand() % 5;
        plats_.push_back(pb);
        platScored_.push_back(false);
    }
    // 得分点 B: 中部区域 (y: 370~550)
    {
        SafeBlock pb;
        pb.rect.x = 100 + rand() % 700;
        pb.rect.y = 370 + rand() % 180;   // y: 370~550
        pb.rect.w = 30 + rand() % 30;
        pb.rect.h = 8 + rand() % 5;
        plats_.push_back(pb);
        platScored_.push_back(false);
    }
}

void Stage::regeneratePlatform()
{
    // 刷新第一个未得分的平台 (兼容旧调用)
    for (size_t i = 0; i < plats_.size(); i++) {
        if (!platScored_[i]) { regeneratePlatformAt((int)i); return; }
    }
    // 全部已得分则刷新第一个
    regeneratePlatformAt(0);
}

void Stage::regeneratePlatformAt(int idx)
{
    if (idx < 0 || idx >= (int)plats_.size()) return;
    plats_[idx].rect.x = 100 + rand() % 700;
    plats_[idx].rect.w = 30 + rand() % 30;
    plats_[idx].rect.h = 8 + rand() % 5;
    // idx=0 上部区域, idx=1 中部区域
    if (idx == 0) {
        plats_[idx].rect.y = 120 + rand() % 250;   // y: 120~370
    } else {
        plats_[idx].rect.y = 370 + rand() % 180;   // y: 370~550
    }
    platScored_[idx] = false;
}

bool Stage::checkSpikeCollision(Vec2 pos, int charW, int charH) const
{
    SDL_Rect cr;
    cr.x = (int)pos.x - charW / 2;
    cr.y = (int)pos.y - charH / 2;
    cr.w = charW;
    cr.h = charH;
    for (const auto& s : spikes_) {
        SDL_Rect tmp;
        if (SDL_IntersectRect(&cr, &s.rect, &tmp))
            return true;
    }
    return false;
}

bool Stage::checkPlatformTop(Vec2 pos, int charW, int charH, double velY,
                              double& outStandY) const
{
    if (plats_.empty()) return false;
    if (velY <= 0.0) return false;

    int playerBottom = (int)pos.y + charH / 2;
    int playerPrevBottom = playerBottom - (int)(velY / 60.0);

    SDL_Rect cr;
    cr.x = (int)pos.x - charW / 2;
    cr.y = (int)pos.y - charH / 2;
    cr.w = charW;
    cr.h = charH;

    for (const auto& pb : plats_) {
        int platTop = pb.rect.y;
        SDL_Rect tmp;
        if (!SDL_IntersectRect(&cr, &pb.rect, &tmp)) continue;
        if (playerPrevBottom <= platTop && playerBottom >= platTop) {
            outStandY = (double)(platTop - charH / 2);
            return true;
        }
    }
    return false;
}

int Stage::checkPlatformTouch(Vec2 pos, int charW, int charH) const
{
    if (plats_.empty()) return -1;
    SDL_Rect cr;
    cr.x = (int)pos.x - charW / 2;
    cr.y = (int)pos.y - charH / 2;
    cr.w = charW;
    cr.h = charH;
    for (size_t i = 0; i < plats_.size(); i++) {
        SDL_Rect tmp;
        if (SDL_IntersectRect(&cr, &plats_[i].rect, &tmp))
            return (int)i;
    }
    return -1;
}

void Stage::draw(SDL_Renderer* r) const
{
    float time = (float)SDL_GetTicks();

    // ═══ 尖刺: 锐利三角锥阵列 ═══
    for (size_t si = 0; si < spikes_.size(); si++) {
        const auto& s = spikes_[si];
        // B1. 底部暗色基座
        SDL_Rect base = {s.rect.x, s.rect.y, s.rect.w, s.rect.h};
        SDL_SetRenderDrawColor(r, 80, 10, 10, 200);
        SDL_RenderFillRect(r, &base);
        // 基座顶部亮边
        SDL_SetRenderDrawColor(r, 150, 20, 20, 220);
        SDL_RenderDrawLine(r, s.rect.x, s.rect.y, s.rect.x + s.rect.w, s.rect.y);

        // B2. 三角锥阵列
        int step = 14;
        int count = s.rect.w / step;
        if (count < 1) count = 1;
        for (int i = 0; i < count; i++) {
            int bx = s.rect.x + i * step;
            int tipX = bx + step / 2;

            if (!s.inverted) {
                // 正置: 尖端向上
                int tipY = s.rect.y - (s.rect.h + 8);
                for (int y = s.rect.y; y > tipY; y--) {
                    float t = (float)(s.rect.y - y) / (float)(s.rect.y - tipY);
                    Uint8 cr = (Uint8)(150 + t * 105);
                    Uint8 cg = (Uint8)(20  + t * 40);
                    Uint8 cb = (Uint8)(20);
                    int hw = (int)((1.0f - t) * step / 2);
                    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
                    SDL_RenderDrawLine(r, tipX - hw, y, tipX + hw, y);
                }
                // 尖端脉冲闪烁
                float pulse = (sinf(time * 0.006f + (float)i * 1.2f + (float)si * 2.0f) + 1.0f) * 0.5f;
                if (pulse > 0.6f) {
                    int a = (int)((pulse - 0.6f) / 0.4f * 220);
                    SDL_SetRenderDrawColor(r, 255, 200, 180, (Uint8)a);
                    SDL_Rect dot = {tipX - 1, tipY - 1, 3, 3};
                    SDL_RenderFillRect(r, &dot);
                }
            } else {
                // 倒置: 尖端向下 (从基座底部延伸)
                int baseBottom = s.rect.y + s.rect.h;
                int tipY = baseBottom + (s.rect.h + 8);
                for (int y = baseBottom; y < tipY; y++) {
                    float t = (float)(y - baseBottom) / (float)(tipY - baseBottom);
                    Uint8 cr = (Uint8)(150 + t * 105);
                    Uint8 cg = (Uint8)(20  + t * 40);
                    Uint8 cb = (Uint8)(20);
                    int hw = (int)((1.0f - t) * step / 2);
                    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
                    SDL_RenderDrawLine(r, tipX - hw, y, tipX + hw, y);
                }
                // 尖端脉冲闪烁
                float pulse = (sinf(time * 0.006f + (float)i * 1.2f + (float)si * 2.0f) + 1.0f) * 0.5f;
                if (pulse > 0.6f) {
                    int a = (int)((pulse - 0.6f) / 0.4f * 220);
                    SDL_SetRenderDrawColor(r, 255, 200, 180, (Uint8)a);
                    SDL_Rect dot = {tipX - 1, tipY - 1, 3, 3};
                    SDL_RenderFillRect(r, &dot);
                }
            }
        }
    }

    // ═══ 得分点: 发光悬浮球 (多平台) ═══
    for (size_t pi = 0; pi < plats_.size(); pi++) {
        const auto& p = plats_[pi].rect;
        int cx = p.x + p.w / 2;
        int baseY = p.y + p.h / 2;
        int radius = (p.w > p.h ? p.w : p.h) / 4 + 4;  // 11~18px

        // A3. 浮动动画 (每个平台相位不同)
        float phase = (float)pi * 1.5f;
        float floatOff = sinf(time * 0.003f + phase) * 4.0f;
        int cy = baseY + (int)floatOff;

        float pulse = (sinf(time * 0.004f + phase) + 1.0f) * 0.5f;  // 0~1

        // A2-外层光晕
        int glowR = radius + 6;
        int glowA = (int)(20 + 30 * pulse);
        drawFillCircle(r, cx, cy, glowR, {0, 220, 200, (Uint8)glowA});

        // A2-球体主体 (逐行扫描线, 底部深蓝→顶部亮青)
        for (int y = -radius; y <= radius; y++) {
            int hw = (int)std::sqrt((double)(radius * radius - y * y));
            if (hw < 1) continue;
            float t = (float)(y + radius) / (float)(2 * radius);  // 0(top)→1(bottom)
            Uint8 cr = (Uint8)(100 - t * 80);
            Uint8 cg = (Uint8)(255 - t * 135);
            Uint8 cb = (Uint8)(220 - t * 40);
            int a = (int)(180 + 60 * pulse);
            if (a > 255) a = 255;
            SDL_SetRenderDrawColor(r, cr, cg, cb, (Uint8)a);
            SDL_RenderDrawLine(r, cx - hw, cy + y, cx + hw, cy + y);
        }

        // A2-高光点
        int hx = cx - radius / 4;
        int hy = cy - radius / 3;
        int hr = radius / 5;
        if (hr < 1) hr = 1;
        drawFillCircle(r, hx, hy, hr, {255, 255, 255, 150});
    }
}

// ============================================================
// 绘图辅助函数
// ============================================================

void Stage::drawFillCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int y = -radius; y <= radius; y++) {
        int w = (int)std::sqrt((double)(radius * radius - y * y));
        SDL_RenderDrawLine(r, cx - w, cy + y, cx + w, cy + y);
    }
}

void Stage::drawFillTriangle(SDL_Renderer* r, int x1, int y1, int x2, int y2,
                              int x3, int y3, SDL_Color c)
{
    // 简化: 扫描线填充三角形
    int minY = y1, maxY = y1;
    if (y2 < minY) minY = y2; if (y2 > maxY) maxY = y2;
    if (y3 < minY) minY = y3; if (y3 > maxY) maxY = y3;

    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int y = minY; y <= maxY; y++) {
        // 求扫描线与三条边的交点
        int pts[3] = {-1, -1, -1};
        int cnt = 0;
        // 边 (x1,y1)-(x2,y2)
        if ((y1 <= y && y2 >= y) || (y2 <= y && y1 >= y)) {
            if (y1 != y2) { pts[cnt++] = x1 + (y - y1) * (x2 - x1) / (y2 - y1); }
        }
        // 边 (x2,y2)-(x3,y3)
        if ((y2 <= y && y3 >= y) || (y3 <= y && y2 >= y)) {
            if (y2 != y3) { pts[cnt++] = x2 + (y - y2) * (x3 - x2) / (y3 - y2); }
        }
        // 边 (x3,y3)-(x1,y1)
        if ((y3 <= y && y1 >= y) || (y1 <= y && y3 >= y)) {
            if (y3 != y1) { pts[cnt++] = x3 + (y - y3) * (x1 - x3) / (y1 - y3); }
        }
        if (cnt >= 2) {
            int lx = pts[0], rx = pts[1];
            if (lx > rx) { lx = pts[1]; rx = pts[0]; }
            SDL_RenderDrawLine(r, lx, y, rx, y);
        }
    }
}
