// ============================================================
// Chaos Pendulum — EasyX 
// ============================================================
#include "Game.h"

int main() {
	Game game;
	if (!game.init()) return 1;
	if (!game.intro()) { game.shutdown(); return 0; }
	game.run();
	game.shutdown();
	return 0;
}
