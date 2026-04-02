#pragma once

#include "game_world.h"

#include <memory>

enum class WorldSessionMode
{
	Solo,
	CampaignDual,
};

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
	void flipActiveWorld();

	void reset();

private:
	GameSessionWorlds() = default;
};

GameWorld &activeGameWorld();
const GameWorld &activeGameWorldConst();

/** Object lists for the currently simulated world (same storage as activeGameWorld().objects). */
WorldObjectState &activeObjects();
const WorldObjectState &activeObjectsConst();

void bindActiveWorld(GameWorld &world);
bool hasActiveWorld();

/** Parked campaign home map/objects while the player is on an off-world mission (stored in GameSessionWorlds::primary). */
GameWorld &missionParkedHomeWorld();

/** Ensure offworld GameWorld exists (campaign mission live map vs parked primary). */
void ensureOffworldWorld();

/** Live off-world map GameWorld (same as GameSessionWorlds::offworld). */
GameWorld &offworldWorld();
