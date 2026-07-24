#include "Game.h"
#include <stdio.h>
#include <math.h>
#include <windows.h>

// ============================================================
// 3x5 点阵字体
// ============================================================
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

void Game::drawText(int x, int y, const char* s, int dot) {
	int ox = x;
	for (int i = 0; s[i]; i++) {
		char c = s[i];
		if (c == ' ') { ox += 3*(dot+1); continue; }
		if (c == '-') { solidrectangle(ox, y+2*(dot+1), ox+3*dot, y+2*(dot+1)+dot); ox += 3*(dot+1); continue; }
		int idx = -1;
		if (c >= 'A' && c <= 'Z') idx = c - 'A';
		else if (c >= 'a' && c <= 'z') idx = c - 'a';
		if (idx < 0) { ox += 3*(dot+1); continue; }
		for (int row=0; row<5; row++) for (int col=0; col<3; col++)
			if (ALPHA[idx][row][col])
				solidrectangle(ox+col*(dot+1), y+row*(dot+1), ox+col*(dot+1)+dot, y+row*(dot+1)+dot);
		ox += 3*(dot+1) + 1;
	}
}

void Game::drawNum(int x, int y, int val, int dot) {
	if (val == 0) {
		for (int row=0; row<5; row++) for (int col=0; col<3; col++)
			if (DIGITS[0][row][col])
				solidrectangle(x+col*(dot+1), y+row*(dot+1), x+col*(dot+1)+dot, y+row*(dot+1)+dot);
		return;
	}
	char buf[12]; int len=0, n=val;
	while (n>0) { buf[len++]='0'+(n%10); n/=10; }
	for (int i=len-1; i>=0; i--) {
		int dg = buf[i]-'0';
		for (int row=0; row<5; row++) for (int col=0; col<3; col++)
			if (DIGITS[dg][row][col])
				solidrectangle(x+col*(dot+1), y+row*(dot+1), x+col*(dot+1)+dot, y+row*(dot+1)+dot);
		x += 3*(dot+1) + 2;
	}
}

// ============================================================
// 初始化
// ============================================================
// 工具: 宽字符文本转 GBK 窄字符串并通过 outtextxy 输出
static void outCN(int x, int y, const wchar_t* wstr) {
	char buf[256] = {0};
	WideCharToMultiByte(CP_ACP, 0, wstr, -1, buf, sizeof(buf), NULL, NULL);
	outtextxy(x, y, buf);
}

// 工具: alpha 混合绘制 IMAGE 到屏幕 (支持透明 PNG)
static void putimageAlpha(int dstX, int dstY, int dstW, int dstH,
	const IMAGE* pSrc, int srcX, int srcY) {
	int sw = pSrc->getwidth(), sh = pSrc->getheight();
	IMAGE dst;
	getimage(&dst, dstX, dstY, dstW, dstH);
	DWORD* dstBuf = GetImageBuffer(&dst);
	const DWORD* srcBuf = GetImageBuffer(pSrc);
	for (int y = 0; y < dstH; y++) {
		int sy = srcY + y * sh / dstH;
		if (sy >= sh) sy = sh - 1;
		for (int x = 0; x < dstW; x++) {
			int sx = srcX + x * sw / dstW;
			if (sx >= sw) sx = sw - 1;
			DWORD sc = srcBuf[sy * sw + sx];
			int sa = (sc >> 24) & 0xFF;
			if (sa == 0) continue;
			if (sa == 255) { dstBuf[y * dstW + x] = sc; continue; }
			DWORD dc = dstBuf[y * dstW + x];
			int sr = GetRValue(sc), sg = GetGValue(sc), sb = GetBValue(sc);
			int dr = GetRValue(dc), dg = GetGValue(dc), db = GetBValue(dc);
			int r = dr + (sr - dr) * sa / 255;
			int g = dg + (sg - dg) * sa / 255;
			int b = db + (sb - db) * sa / 255;
			dstBuf[y * dstW + x] = RGB(r, g, b);
		}
	}
	putimage(dstX, dstY, &dst);
}

// 工具: 将 IMAGE 中近黑色像素设为透明
static void removeBlackBg(IMAGE& img, int threshold = 30) {
	DWORD* buf = GetImageBuffer(&img);
	int w = img.getwidth(), h = img.getheight();
	for (int i = 0; i < w * h; i++) {
		DWORD c = buf[i];
		int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
		if (r < threshold && g < threshold && b < threshold)
			buf[i] = 0;  // alpha=0, 全透明
	}
}

// 工具: 将 IMAGE 缩放到指定尺寸 (最近邻采样)
static void resizeImage(IMAGE& dst, const IMAGE& src, int newW, int newH) {
	DWORD* dstBuf = GetImageBuffer(&dst);
	const DWORD* srcBuf = GetImageBuffer(&src);
	int sw = src.getwidth(), sh = src.getheight();
	for (int y = 0; y < newH; y++) {
		int sy = y * sh / newH;
		if (sy >= sh) sy = sh - 1;
		for (int x = 0; x < newW; x++) {
			int sx = x * sw / newW;
			if (sx >= sw) sx = sw - 1;
			dstBuf[y * newW + x] = srcBuf[sy * sw + sx];
		}
	}
}

bool Game::init() {
	initgraph(1000, 940, EW_SHOWCONSOLE);
	BeginBatchDraw();
	setbkcolor(RGB(15, 15, 25));
	cleardevice();

	// 加载背景图
	loadimage(&imgBg_, _T("assets/back.png"), 1000, 940, true);

	// 加载机器人: 去黑底 → 预缩放到 40×40
	IMAGE rawRobot;
	if (loadimage(&rawRobot, _T("assets/robot.png")) == 0) {
		removeBlackBg(rawRobot);
		// 预缩放: 1024x1024 → 40x40
		IMAGE scaled(40, 40);
		resizeImage(scaled, rawRobot, 40, 40);
		imgRobot_ = scaled;
		robotLoaded_ = true;
	} else {
		printf("[WARN] robot.png load failed\n");
	}

	stage_.regenerate();
	return true;
}

bool Game::intro() {
	// 故事文本 (按行拆分)
	const wchar_t* lines[] = {
		L"\u4f60\u662f\u4e00\u4e2a\u63a2\u7d22\u673a\u5668\u4eba\uff0c\u5728\u4e00\u6b21\u8fdc\u53e4\u9057\u8ff9\u63a2\u7d22\u4efb\u52a1\u4e2d\uff0c",
		L"\u4f60\u906d\u9047\u4e86\u81f4\u547d\u7684\u7a0b\u5e8f\u6545\u969c\uff0c\u4e0d\u614e\u89e6\u53d1\u4e86\u6c89\u7761\u5343\u5e74\u7684\u795e\u79d8\u673a\u5173\u3002",
		L"",
		L"\u5982\u4eca\uff0c\u4f60\u88ab\u56f0\u5728\u8fd9\u5145\u6ee1\u81f4\u547d\u9677\u9631\u7684\u6df7\u6c8c\u7a7a\u95f4\u3002",
		L"\u552f\u4e00\u7684\u673a\u4f1a\uff0c\u662f\u6536\u96c6\u6563\u843d\u7684\u80fd\u91cf\u7403\u2014\u2014",
		L"\u5f53\u80fd\u91cf\u79ef\u84c4\u5230\u4e34\u754c\u70b9\u65f6\uff0c\u4f20\u9001\u6838\u5fc3\u5c06\u88ab\u6fc0\u6d3b\u3002",
		L"",
		L"\u90a3\u662f\u4f60\u9003\u79bb\u8fd9\u91cc\u7684\u552f\u4e00\u4e4b\u8def\u3002",
		L"",
		L"\u6d3b\u4e0b\u53bb\uff0c\u6536\u96c6\u80fd\u91cf\uff0c\u6fc0\u6d3b\u4f20\u9001\uff0c\u9003\u79bb\u8fd9\u4e2a\u5730\u65b9\uff01\uff01",
		L"@###h\u68cd\u65a4\u62f7kg&%??",
	};
	const int lineCount = sizeof(lines) / sizeof(lines[0]);

	const int fontH = 24;
	const int charMs = 70;
	const int lineGapMs = 400;
	const int startX = 80;
	const int startY = 240;
	const int lineSpacing = 38;

	settextstyle(fontH, 0, _T("微软雅黑"));
	setbkmode(TRANSPARENT);

	DWORD startTime = GetTickCount();

	// 预计算每行起始的 "字符预算" 偏移
	int lineBudgetStart[20];
	int totalBudget = 0;
	{
		int b = 0;
		for (int i = 0; i < lineCount; i++) {
			lineBudgetStart[i] = b;
			if (wcslen(lines[i]) == 0)
				b += lineGapMs / charMs;
			else
				b += (int)wcslen(lines[i]);
		}
		totalBudget = b;
	}

	bool started = false;
	bool skipped = false;
	bool skipWasDown = false;
	while (!started) {
		DWORD elapsed = GetTickCount() - startTime;
		int shownBudget = skipped ? 999999 : (int)(elapsed / charMs);

		cleardevice();
		setbkcolor(RGB(5, 5, 12));
		cleardevice();

		// 标题
		settextstyle(36, 0, _T("微软雅黑"));
		settextcolor(RGB(60, 200, 255));
		{
			char buf[128];
			wchar_t w[] = L"// \u7cfb\u7edf\u542f\u52a8\u4e2d...";
			WideCharToMultiByte(CP_ACP, 0, w, -1, buf, sizeof(buf), NULL, NULL);
			outtextxy(startX, startY - 90, buf);
		}

		// 分隔线
		setlinecolor(RGB(30, 80, 120));
		line(startX, startY - 40, 920, startY - 40);

		// 故事正文
		settextstyle(fontH, 0, _T("\u5fae\u8f6f\u96c5\u9ed1"));
		int cursorY = startY;
		for (int i = 0; i < lineCount; i++) {
			int len = (int)wcslen(lines[i]);
			int budgetOff = shownBudget - lineBudgetStart[i];
			if (budgetOff <= 0) { cursorY += lineSpacing; continue; }
			int visible = (budgetOff >= len) ? len : budgetOff;
			if (len == 0) { cursorY += lineSpacing; continue; }
			wchar_t wbuf[128] = {0};
			wcsncpy_s(wbuf, lines[i], visible);
			char buf[256] = {0};
			WideCharToMultiByte(CP_ACP, 0, wbuf, -1, buf, sizeof(buf), NULL, NULL);
			settextcolor(RGB(170, 195, 215));
			outtextxy(startX, cursorY, buf);
			cursorY += lineSpacing;
		}

		// 光标闪烁
		if ((GetTickCount() / 400) % 2 == 0 && shownBudget < totalBudget) {
			int lastLine = -1, lastVisible = 0;
			for (int i = lineCount - 1; i >= 0; i--) {
				int budgetOff = shownBudget - lineBudgetStart[i];
				int len = (int)wcslen(lines[i]);
				if (len > 0 && budgetOff > 0) {
					lastLine = i;
					lastVisible = (budgetOff >= len) ? len : budgetOff;
					break;
				}
			}
			if (lastLine >= 0) {
				int cx = startX + lastVisible * fontH;
				int cy = startY + lastLine * lineSpacing;
				settextcolor(RGB(60, 200, 255));
				outtextxy(cx, cy, "_");
			}
		}

		// 文字全部显示后提示
		if (shownBudget >= totalBudget) {
			if ((GetTickCount() / 600) % 2 == 0) {
				settextstyle(22, 0, _T("微软雅黑"));
				settextcolor(RGB(80, 220, 160));
				char buf[128];
				wchar_t w[] = L"[ \u6309 Space \u5f00\u59cb\u6e38\u620f ]";
				WideCharToMultiByte(CP_ACP, 0, w, -1, buf, sizeof(buf), NULL, NULL);
				outtextxy(340, startY + lineCount * lineSpacing + 50, buf);
			}
		}

		// 空格跳过动画 / 开始游戏
		bool spNow = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
		if (spNow && !skipWasDown) {
			if (!skipped && shownBudget < totalBudget)
				skipped = true;
			else
				started = true;
		}
		skipWasDown = spNow;

		if (GetHWnd() == NULL) return false;
		FlushBatchDraw();
		Sleep(16);
	}

	setbkcolor(RGB(15, 15, 25));
	cleardevice();
	spaceWasDown_ = true;
	return true;
}

void Game::shutdown() {
	EndBatchDraw();
	closegraph();
}

// ============================================================
// 主循环
// ============================================================
void Game::run() {
	running_ = true;
	lastTick_ = GetTickCount64();

	while (running_) {
		handleInput();

		DWORD now = GetTickCount64();
		double dt = (now - lastTick_) / 1000.0;
		lastTick_ = now;
		if (dt > 0.05) dt = 0.05;

		update(dt);
		render(now);
	}
}

// ============================================================
// 输入处理 (peekmessage + GetAsyncKeyState 混合)
// ============================================================
void Game::handleInput() {
	ExMessage msg;

	// EasyX 消息 (鼠标 + 键盘事件)
	while (peekmessage(&msg, EM_KEY | EM_MOUSE)) {
		switch (msg.message) {
		case WM_KEYDOWN:
			switch (msg.vkcode) {
			case VK_ESCAPE: running_ = false; break;
			case 'R': pendulum_.reset(); player_.reset_Player(); player_.resetScore(); stage_.regenerate(); survivalTime_ = 0.0; victory_ = false; break;
			}
			break;
		case WM_MOUSEMOVE:
			mouseWorld_.x = (double)msg.x;
			mouseWorld_.y = (double)msg.y;
			break;
		case WM_LBUTTONDOWN:
			player_.hook(mouseWorld_, pendulum_, pivot_);
			break;
		}
	}

	// 持续按键 + 边沿检测 (防自动重复)
	bool aDown = (GetAsyncKeyState('A') & 0x8000) != 0;
	bool dDown = (GetAsyncKeyState('D') & 0x8000) != 0;
	bool wDown = (GetAsyncKeyState('W') & 0x8000) != 0;
	bool spDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
	bool fDown  = (GetAsyncKeyState('F') & 0x8000) != 0;
	bool qDown  = (GetAsyncKeyState('Q') & 0x8000) != 0;
	bool eDown  = (GetAsyncKeyState('E') & 0x8000) != 0;

	// A/D 持续移动
	if (aDown) player_.moveLeft();
	if (dDown) player_.moveRight();

	// W - 跳跃 (边沿触发)
	if (wDown && !wWasDown_) player_.jump(pendulum_, pivot_);
	wWasDown_ = wDown;

	// Space - 暂停 (边沿触发)
	if (spDown && !spaceWasDown_) paused_ = !paused_;
	spaceWasDown_ = spDown;

	// F - 球捕获切换 (边沿触发)
	if (fDown && !fWasDown_) {
		player_.toggleBallCapture();
	}
	fWasDown_ = fDown;

	// Q/E - 重力倍率调节 (边沿触发)
	if (qDown && !qWasDown_) player_.adjGravity(-0.1);
	qWasDown_ = qDown;
	if (eDown && !eWasDown_) player_.adjGravity(+0.1);
	eWasDown_ = eDown;

	// 1~5 预设角度
	static bool p1=false,p2=false,p3=false,p4=false,p5=false;
	auto chk = [](bool& prev, bool cur, int preset, Pendulum& pen) {
		if (cur && !prev) pen.setPreset(preset);
		prev = cur;
	};
	chk(p1, (GetAsyncKeyState('1')&0x8000)!=0, 0, pendulum_);
	chk(p2, (GetAsyncKeyState('2')&0x8000)!=0, 1, pendulum_);
	chk(p3, (GetAsyncKeyState('3')&0x8000)!=0, 2, pendulum_);
	chk(p4, (GetAsyncKeyState('4')&0x8000)!=0, 3, pendulum_);
	chk(p5, (GetAsyncKeyState('5')&0x8000)!=0, 4, pendulum_);
}

// ============================================================
// 更新 (逻辑与原版完全一致)
// ============================================================
void Game::update(double dt) {
	if (paused_ || victory_) return;

	static int prevState = -1;
	static double survMark = 0.0;
	static int lastTouchedPlat = -1;

	pendulum_.step(dt, 32);
	player_.update(dt, pendulum_, pivot_);

	int curState = (int)player_.state();

	// 存活计时
	if (curState != 3) {
		survivalTime_ += dt;
		if (survivalTime_ - survMark >= 15.0) {
			survMark += 15.0;
			player_.addScore(1);
			pendulum_.triggerScore();
			player_.emitScoreSparks();
		}
	}

	// 死亡自动重置
	if (prevState == 3 && curState != 3) {
		player_.addScore(-1);
		stage_.regenerate();
		survivalTime_ = 0.0; survMark = 0.0;
	}
	prevState = curState;

	// 尖刺碰撞
	if (stage_.checkSpikeCollision(player_.position(), 10, 10))
		player_.die();

	// 平台站立
	double standY;
	if (stage_.checkPlatformTop(player_.position(), 10, 10, player_.velY(), standY))
		player_.landOnPlatform(standY);

	// 平台触碰
	int touchIdx = stage_.checkPlatformTouch(player_.position(), 10, 10);
	if (touchIdx >= 0 && touchIdx != lastTouchedPlat) {
		player_.addScore(1);
		pendulum_.triggerScore();
		player_.emitScoreSparks();
		stage_.regeneratePlatformAt(touchIdx);
	}
	lastTouchedPlat = touchIdx;

	// 胜利检测
	if (player_.score() >= 15) {
		victory_ = true;
	}
}

// ============================================================
// 渲染
// ============================================================
void Game::render(DWORD now) {
	cleardevice();

	// ── 背景图 ──
	putimage(0, 0, &imgBg_);

	// ── 场景 + 摆锤 + 玩家 ──
	stage_.draw();
	pendulum_.draw(pivot_, now);
	player_.draw();

	// ── 机器人图片覆盖角色位置 ──
	if (robotLoaded_) {
		Vec2 pp = player_.position();
		int drawW = 40, drawH = 40;
		int dx = (int)pp.x - drawW/2;
		int dy = (int)pp.y - 6 - drawH/2;  // 以角色视觉中心(pos.y-6)为机器人中心
		// 边界保护
		if (dx >= 0 && dy >= 0 && dx + drawW <= 1000 && dy + drawH <= 940)
			putimageAlpha(dx, dy, drawW, drawH, &imgRobot_, 0, 0);
	}

	// ── 左侧速度信息 ──
	const auto& st = pendulum_.state();
	const auto& pr = pendulum_.params();
	int surf1 = (int)(fabs(st.w1) * pr.R1);
	int surf2 = (int)(fabs(st.w2) * pr.R2);
	int pspd  = (int)player_.speed();

	setfillcolor(RGB(255, 160, 60));
	drawNum(10, 90, surf1);
	setfillcolor(RGB(100, 200, 255));
	drawNum(10, 112, surf2);
	double sp = pspd / 1000.0; if (sp > 1.0) sp = 1.0;
	int sr = (int)(255*sp), sg = (int)(255*(1.0-sp));
	setfillcolor(RGB(sr, sg, 60));
	drawNum(10, 140, pspd);

	// ── 暂停菜单 (霓虹风格) ──
	if (paused_) {
		// 深色遮罩
		setfillcolor(RGB(3, 3, 10));
		solidrectangle(0, 0, 1000, 940);

		// CRT 扫描线 (水平线, 静态不闪)
		for (int yy = 0; yy < 940; yy += 3) {
			setlinecolor(RGB(6, 6, 18));
			line(0, yy, 999, yy);
		}

		int cx = 500, cy = 430;
		int bw = 340, bh = 290;
		int bx1 = cx-bw/2, by1 = cy-bh/2, bx2 = cx+bw/2, by2 = cy+bh/2;

		// 外层辉光环 (固定亮度, 不闪)
		int glowColors[3][3] = {
			{18, 8, 8},   // 最外
			{30, 14, 14},
			{45, 22, 22},
		};
		for (int g = 0; g < 3; g++) {
			int expand = (3 - g) * 3;
			setfillcolor(RGB(glowColors[g][0], glowColors[g][1], glowColors[g][2]));
			// 上
			solidrectangle(bx1-expand, by1-expand-2, bx2+expand, by1-expand);
			// 下
			solidrectangle(bx1-expand, by2+expand, bx2+expand, by2+expand+2);
			// 左
			solidrectangle(bx1-expand-2, by1-expand, bx1-expand, by2+expand);
			// 右
			solidrectangle(bx2+expand, by1-expand, bx2+expand+2, by2+expand);
		}

		// 主面板 (深色)
		setfillcolor(RGB(10, 10, 22));
		solidrectangle(bx1, by1, bx2, by2);

		// 内层霓虹边框 (亮红)
		setlinecolor(RGB(220, 50, 50));
		rectangle(bx1, by1, bx2, by2);
		// 外层霓虹边框 (暗红)
		setlinecolor(RGB(140, 25, 25));
		rectangle(bx1-2, by1-2, bx2+2, by2+2);

		// 角落装饰点 (4个亮色方块, 居中于角点)
		setfillcolor(RGB(255, 80, 80));
		solidrectangle(bx1-2, by1-2, bx1+2, by1+2);
		solidrectangle(bx2-2, by1-2, bx2+2, by1+2);
		solidrectangle(bx1-2, by2-2, bx1+2, by2+2);
		solidrectangle(bx2-2, by2-2, bx2+2, by2+2);

		// 已暂停 标题 (霓虹红 + 辉光底)
		setfillcolor(RGB(80, 15, 15));
		solidrectangle(cx - 50, cy - bh/2 + 14, cx + 50, cy - bh/2 + 40);
		settextstyle(24, 0, _T("微软雅黑"));
		setbkmode(TRANSPARENT);
		settextcolor(RGB(255, 80, 80));
		outCN(cx - 36, cy - bh/2 + 18, L"\u5df2\u6682\u505c");

		// 分数 (霓虹红)
		settextstyle(18, 0, _T("微软雅黑"));
		settextcolor(RGB(255, 100, 100));
		outCN(cx - 50, cy - 12, L"\u5206\u6570");
		setfillcolor(RGB(255, 100, 100));
		drawNum(cx + 10, cy - 10, player_.score(), 2);

		// 时间 (霓虹蓝)
		settextcolor(RGB(80, 180, 255));
		outCN(cx - 50, cy + 14, L"\u65f6\u95f4");
		int sec = (int)survivalTime_;
		char timebuf[32];
		sprintf_s(timebuf, "%d:%02d", sec/60, sec%60);
		setfillcolor(RGB(80, 180, 255));
		drawText(cx + 10, cy + 16, timebuf, 2);

		// 分隔线
		setlinecolor(RGB(40, 40, 80));
		line(cx - bw/2 + 20, cy + 46, cx + bw/2 - 20, cy + 46);

		// 键位提示 (中文)
		int ky = cy + 48;
		const char* keys[] = {"W", "A/D", "LMB", "R", "SPACE"};
		const wchar_t* acts[] = {
			L"\u8df3\u8dc3",       // 跳跃
			L"\u79fb\u52a8",       // 移动
			L"\u94a9\u9501",       // 钩锁
			L"\u91cd\u7f6e",       // 重置
			L"\u7ee7\u7eed",       // 继续
		};
		for (int i = 0; i < 5; i++) {
			setfillcolor(RGB(100, 255, 200));
			drawText(cx - 120, ky + i*20, keys[i], 2);
			setfillcolor(RGB(50, 50, 80));
			drawText(cx - 60, ky + i*20, "-", 2);
			settextcolor(RGB(90, 100, 130));
			outCN(cx - 44, ky + i*20 - 2, acts[i]);
		}
	}

	// ── 胜利画面 (青绿霓虹风格) ──
	if (victory_) {
		setfillcolor(RGB(3, 3, 10));
		solidrectangle(0, 0, 1000, 940);
		for (int yy = 0; yy < 940; yy += 3) {
			setlinecolor(RGB(6, 6, 18));
			line(0, yy, 999, yy);
		}

		int cx = 500, cy = 430;
		int bw = 400, bh = 280;
		int bx1 = cx-bw/2, by1 = cy-bh/2, bx2 = cx+bw/2, by2 = cy+bh/2;

		int glowColors[3][3] = {{8,18,8},{14,30,14},{22,45,22}};
		for (int g = 0; g < 3; g++) {
			int expand = (3 - g) * 3;
			setfillcolor(RGB(glowColors[g][0], glowColors[g][1], glowColors[g][2]));
			solidrectangle(bx1-expand, by1-expand-2, bx2+expand, by1-expand);
			solidrectangle(bx1-expand, by2+expand, bx2+expand, by2+expand+2);
			solidrectangle(bx1-expand-2, by1-expand, bx1-expand, by2+expand);
			solidrectangle(bx2+expand, by1-expand, bx2+expand+2, by2+expand);
		}

		setfillcolor(RGB(10, 22, 10));
		solidrectangle(bx1, by1, bx2, by2);
		setlinecolor(RGB(50, 220, 100));
		rectangle(bx1, by1, bx2, by2);
		setlinecolor(RGB(25, 140, 60));
		rectangle(bx1-2, by1-2, bx2+2, by2+2);

		setfillcolor(RGB(80, 255, 120));
		solidrectangle(bx1-1, by1-1, bx1+3, by1+3);
		solidrectangle(bx2-3, by1-1, bx2+1, by1+3);
		solidrectangle(bx1-1, by2-3, bx1+3, by2+1);
		solidrectangle(bx2-3, by2-3, bx2+1, by2+1);

		// 通关成功
		setfillcolor(RGB(15, 60, 30));
		solidrectangle(cx - 70, cy - bh/2 + 14, cx + 70, cy - bh/2 + 46);
		settextstyle(28, 0, _T("微软雅黑"));
		setbkmode(TRANSPARENT);
		settextcolor(RGB(80, 255, 120));
		outCN(cx - 56, cy - bh/2 + 20, L"\u901a\u5173\u6210\u529f");

		// 分数
		settextstyle(20, 0, _T("微软雅黑"));
		settextcolor(RGB(100, 255, 140));
		outCN(cx - 60, cy - 14, L"\u5206\u6570");
		setfillcolor(RGB(100, 255, 140));
		drawNum(cx + 10, cy - 12, player_.score(), 2);

		// 分隔线
		setlinecolor(RGB(40, 80, 40));
		line(cx - bw/2 + 20, cy + 20, cx + bw/2 - 20, cy + 20);

		// 按 R 重新开始
		settextstyle(18, 0, _T("微软雅黑"));
		settextcolor(RGB(100, 255, 200));
		outCN(cx - 70, cy + 34, L"\u6309 R \u91cd\u65b0\u5f00\u59cb");

		// 传送核心已激活 (闪烁)
		if ((GetTickCount() / 500) % 2 == 0) {
			settextcolor(RGB(60, 220, 160));
			outCN(cx - 80, cy + 64, L"\u4f20\u9001\u6838\u5fc3\u5df2\u6fc0\u6d3b");
		}
	}

	FlushBatchDraw();
	Sleep(1);
}
