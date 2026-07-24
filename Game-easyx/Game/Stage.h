#pragma once
#define NOMINMAX
#include <graphics.h>
#include <vector>
#include "Vec2.h"

// 自定义 Rect 替代 SDL_Rect
struct Rect {
	int x, y, w, h;
};

// 尖刺
struct Spike {
	int x, y, w, h;
	bool inverted = false;  // true=倒置(尖端朝下)
};

// 得分平台
struct SafeBlock {
	int x, y, w, h;
};

class Stage {
public:
	Stage();
	void regenerate();              // 随机生成尖刺+平台
	void regeneratePlatformAt(int idx); // 刷新指定平台
	const std::vector<Spike>&     spikes()    const { return spikes_; }
	const std::vector<SafeBlock>& platforms() const { return plats_; }

	bool checkSpikeCollision(Vec2 pos, int charW, int charH) const;
	bool checkPlatformTop(Vec2 pos, int charW, int charH, double velY, double& outStandY) const;
	int  checkPlatformTouch(Vec2 pos, int charW, int charH) const;
	void draw() const;

private:
	std::vector<Spike>     spikes_;
	std::vector<SafeBlock> plats_;

	// 辅助
	static void fillCircle(int cx, int cy, int r, COLORREF c);
	static bool intersectRect(int ax, int ay, int aw, int ah,
	                          int bx, int by, int bw, int bh);
};
