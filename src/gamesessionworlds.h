#pragma once

#include <memory>

enum class WorldSessionMode
{
	Solo,
	CampaignDual,
};

struct GameWorld;

class GameSessionWorlds
{
public:
	static GameSessionWorlds &instance();

	WorldSessionMode mode = WorldSessionMode::Solo;

	std::unique_ptr<GameWorld> primary;
	std::unique_ptr<GameWorld> offworld;

	GameWorld *active = nullptr;

	void setMode(WorldSessionMode m) { mode = m; }

	void setActiveToPrimary();
	void setActiveToOffworld();

	void reset();

private:
	GameSessionWorlds() = default;
};

GameWorld &activeGameWorld();
const GameWorld &activeGameWorldConst();
void bindActiveWorld(GameWorld &world);
bool hasActiveWorld();
