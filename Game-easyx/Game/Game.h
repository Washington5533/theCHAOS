#pragma once
#define NOMINMAX
#include <graphics.h>
#include "Player.h"
#include "Physics.h"
#include "Pendulum.h"
#include "Stage.h"

class Game {
public:
	Game()  {}
	~Game() {}

	bool init();
	bool intro();  // 返回 false 表示窗口已关闭
	void run();
	void shutdown();

	// 图片资源 (public 供 Player 访问)
	IMAGE imgBg_;
	IMAGE imgRobot_;
	bool  robotLoaded_ = false;

private:
	Pendulum pendulum_;
	Player   player_;
	Stage    stage_;
	Vec2     pivot_ = {500, 300};
	bool     paused_ = false;
	bool     running_ = false;
	bool     victory_ = false;
	DWORD    lastTick_ = 0;
	Vec2     mouseWorld_ = {0, 0};
	double   survivalTime_ = 0.0;

	// 按键防重复
	bool     wWasDown_ = false;
	bool     spaceWasDown_ = false;
	bool     fWasDown_ = false;
	bool     qWasDown_ = false;
	bool     eWasDown_ = false;

	void handleInput();
	void update(double dt);
	void render(DWORD now);

	// 像素字体
	static void drawText(int x, int y, const char* s, int dot = 2);
	static void drawNum(int x, int y, int val, int dot = 3);
};
