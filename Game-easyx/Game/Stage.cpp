#include "Stage.h"
#include <cstdlib>
#include <ctime>
#include <cmath>

Stage::Stage() {
	static bool seeded = false;
	if (!seeded) { srand((unsigned)time(nullptr)); seeded = true; }
}

// AABB 相交检测
bool Stage::intersectRect(int ax, int ay, int aw, int ah,
                          int bx, int by, int bw, int bh) {
	return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

bool Stage::checkSpikeCollision(Vec2 pos, int charW, int charH) const {
	int cx = (int)pos.x - charW / 2;
	int cy = (int)pos.y - charH / 2;
	for (const auto& s : spikes_) {
		if (intersectRect(cx, cy, charW, charH, s.x, s.y, s.w, s.h))
			return true;
	}
	return false;
}

bool Stage::checkPlatformTop(Vec2 pos, int charW, int charH, double velY, double& outStandY) const {
	if (plats_.empty() || velY <= 0.0) return false;

	int playerBottom = (int)pos.y + charH / 2;
	int playerPrevBottom = playerBottom - (int)(velY / 60.0);
	int cx = (int)pos.x - charW / 2;
	int cy = (int)pos.y - charH / 2;

	for (const auto& pb : plats_) {
		if (!intersectRect(cx, cy, charW, charH, pb.x, pb.y, pb.w, pb.h)) continue;
		int platTop = pb.y;
		if (playerPrevBottom <= platTop && playerBottom >= platTop) {
			outStandY = (double)(platTop - charH / 2);
			return true;
		}
	}
	return false;
}

int Stage::checkPlatformTouch(Vec2 pos, int charW, int charH) const {
	if (plats_.empty()) return -1;
	int cx = (int)pos.x - charW / 2;
	int cy = (int)pos.y - charH / 2;
	for (size_t i = 0; i < plats_.size(); i++) {
		if (intersectRect(cx, cy, charW, charH, plats_[i].x, plats_[i].y, plats_[i].w, plats_[i].h))
			return (int)i;
	}
	return -1;
}

void Stage::regenerate() {
	spikes_.clear();
	plats_.clear();

	// 尖刺 (2块)
	if (rand() % 100 < 30) {
		// 30%: 1顶部倒置 + 1下部正置
		Spike spTop;
		spTop.x = 50 + rand() % 850;
		spTop.y = 10 + rand() % 110;
		spTop.w = 30 + rand() % 50;
		spTop.h = 12 + rand() % 16;
		spTop.inverted = true;
		spikes_.push_back(spTop);

		Spike spBot;
		spBot.x = 50 + rand() % 850;
		spBot.y = 550 + rand() % 320;
		spBot.w = 30 + rand() % 50;
		spBot.h = 12 + rand() % 16;
		spBot.inverted = false;
		spikes_.push_back(spBot);
	} else {
		// 70%: 2块下部正置
		for (int i = 0; i < 2; i++) {
			Spike sp;
			sp.x = 50 + rand() % 850;
			sp.y = 550 + rand() % 320;
			sp.w = 30 + rand() % 50;
			sp.h = 12 + rand() % 16;
			sp.inverted = false;
			spikes_.push_back(sp);
		}
	}

	// 得分平台 (2个)
	{
		SafeBlock pb;
		pb.x = 100 + rand() % 700;
		pb.y = 120 + rand() % 250;
		pb.w = 30 + rand() % 30;
		pb.h = 8 + rand() % 5;
		plats_.push_back(pb);
	}
	{
		SafeBlock pb;
		pb.x = 100 + rand() % 700;
		pb.y = 370 + rand() % 180;
		pb.w = 30 + rand() % 30;
		pb.h = 8 + rand() % 5;
		plats_.push_back(pb);
	}
}

void Stage::regeneratePlatformAt(int idx) {
	if (idx < 0 || idx >= (int)plats_.size()) return;
	plats_[idx].x = 100 + rand() % 700;
	plats_[idx].w = 30 + rand() % 30;
	plats_[idx].h = 8 + rand() % 5;
	if (idx == 0)
		plats_[idx].y = 120 + rand() % 250;
	else
		plats_[idx].y = 370 + rand() % 180;
}

// 填充圆辅助函数
void Stage::fillCircle(int cx, int cy, int r, COLORREF c) {
	setfillcolor(c);
	for (int y = -r; y <= r; y++) {
		int w = (int)std::sqrt((double)(r * r - y * y));
		solidrectangle(cx - w, cy + y, cx + w, cy + y);
	}
}

void Stage::draw() const {
	float t = (float)GetTickCount();

	// ═══ 尖刺 ═══
	for (const auto& s : spikes_) {
		// 基座
		setfillcolor(RGB(80, 10, 10));
		solidrectangle(s.x, s.y, s.x + s.w, s.y + s.h);

		// 顶部亮边
		setlinecolor(RGB(150, 20, 20));
		line(s.x, s.y, s.x + s.w, s.y);

		// 三角锥阵列
		int step = 14;
		int count = s.w / step;
		if (count < 1) count = 1;
		for (int i = 0; i < count; i++) {
			int bx = s.x + i * step;
			int tipX = bx + step / 2;
			if (!s.inverted) {
				// 正置: 尖端向上
				int tipY = s.y - (s.h + 8);
				for (int y = s.y; y > tipY; y--) {
					float ratio = (float)(s.y - y) / (float)(s.y - tipY);
					int cr = (int)(150 + ratio * 105);
					int cg = (int)(20 + ratio * 40);
					int hw = (int)((1.0f - ratio) * step / 2);
					setlinecolor(RGB(cr, cg, 20));
					line(tipX - hw, y, tipX + hw, y);
				}
			} else {
				// 倒置: 尖端向下
				int baseBottom = s.y + s.h;
				int tipY = baseBottom + (s.h + 8);
				for (int y = baseBottom; y < tipY; y++) {
					float ratio = (float)(y - baseBottom) / (float)(tipY - baseBottom);
					int cr = (int)(150 + ratio * 105);
					int cg = (int)(20 + ratio * 40);
					int hw = (int)((1.0f - ratio) * step / 2);
					setlinecolor(RGB(cr, cg, 20));
					line(tipX - hw, y, tipX + hw, y);
				}
			}
		}
	}

	// ═══ 得分平台: 发光悬浮球 (同心圆模拟光晕) ═══
	for (size_t pi = 0; pi < plats_.size(); pi++) {
		const auto& p = plats_[pi];
		int cx = p.x + p.w / 2;
		int baseY = p.y + p.h / 2;
		int radius = (p.w > p.h ? p.w : p.h) / 4 + 4;
		float phase = (float)pi * 1.5f;
		float floatOff = sinf(t * 0.003f + phase) * 4.0f;
		int cy = baseY + (int)floatOff;

		// 外层光晕 (用深色同心圆模拟，非alpha)
		fillCircle(cx, cy, radius + 8, RGB(0, 40, 60));
		fillCircle(cx, cy, radius + 5, RGB(0, 80, 100));

		// 球体主体 (底部深蓝→顶部亮青 扫描线)
		for (int y = -radius; y <= radius; y++) {
			int hw = (int)std::sqrt((double)(radius * radius - y * y));
			if (hw < 1) continue;
			float rowT = (float)(y + radius) / (float)(2 * radius); // 0顶→1底
			int cr = (int)(100 - rowT * 80);
			int cg = (int)(255 - rowT * 135);
			int cb = (int)(220 - rowT * 40);
			setlinecolor(RGB(cr, cg, cb));
			line(cx - hw, cy + y, cx + hw, cy + y);
		}

		// 高光点
		setfillcolor(RGB(180, 255, 255));
		solidrectangle(cx - 3, cy - radius/3 - 1, cx + 3, cy - radius/3 + 2);
	}
}
