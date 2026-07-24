#pragma once
#include <SDL.h>
#include <vector>
#include "Vec2.h"

struct Spike {
    SDL_Rect rect;
    bool inverted = false;  // true = 倒置(尖端朝下, 顶部尖刺)
};

struct SafeBlock {
    SDL_Rect rect;   // 可站立的实体 (不致死)
};

class Stage {
public:
    Stage();
    void regenerate();             // 随机生成尖刺 + 安全平台
    void regeneratePlatform();     // 仅刷新平台位置

    const std::vector<Spike>& spikes() const { return spikes_; }
    const std::vector<SafeBlock>& platforms() const { return plats_; }
    bool hasPlatform() const { return !plats_.empty(); }

    bool checkSpikeCollision(Vec2 pos, int charW, int charH) const;
    bool checkPlatformTop(Vec2 pos, int charW, int charH, double velY,
                          double& outStandY) const;
    // 返回触碰到的平台索引 (-1 = 无)
    int  checkPlatformTouch(Vec2 pos, int charW, int charH) const;
    void regeneratePlatformAt(int idx);  // 仅刷新指定平台位置
    void draw(SDL_Renderer* r) const;

private:
    std::vector<Spike> spikes_;
    std::vector<SafeBlock> plats_;
    std::vector<bool> platScored_;  // 标记已得分的平台

    // 绘图辅助
    static void drawFillCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c);
    static void drawFillTriangle(SDL_Renderer* r, int x1, int y1, int x2, int y2, int x3, int y3, SDL_Color c);
};
