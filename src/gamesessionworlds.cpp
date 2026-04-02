#include "gamesessionworlds.h"

#include "game_world.h"

#include "lib/framework/frame.h" // for ASSERT

GameSessionWorlds &GameSessionWorlds::instance()
{
	static GameSessionWorlds s;
	return s;
}

void GameSessionWorlds::setActiveToPrimary()
{
	ASSERT(primary != nullptr, "setActiveToPrimary: no primary");
	active = primary.get();
}

void GameSessionWorlds::setActiveToOffworld()
{
	ASSERT(mode == WorldSessionMode::CampaignDual, "setActiveToOffworld: not dual");
	ASSERT(offworld != nullptr, "setActiveToOffworld: no offworld");
	active = offworld.get();
}

void GameSessionWorlds::flipActiveWorld()
{
	ASSERT(mode == WorldSessionMode::CampaignDual, "flipActiveWorld: not dual");
	ASSERT(primary != nullptr, "flipActiveWorld: no primary");
	ASSERT(offworld != nullptr, "flipActiveWorld: no offworld");
	active = active == primary.get() ? offworld.get() : primary.get(); // toggle between primary and offworld
}

void GameSessionWorlds::reset()
{
	primary.reset();
	offworld.reset();
	active = nullptr;
	mode = WorldSessionMode::Solo;
}

GameWorld &activeGameWorld()
{
	GameSessionWorlds &s = GameSessionWorlds::instance();
	ASSERT(s.active != nullptr, "No active world bound");
	return *s.active;
}

const GameWorld &activeGameWorldConst()
{
	return activeGameWorld();
}

WorldObjectState &activeObjects()
{
	return activeGameWorld().objects;
}

const WorldObjectState &activeObjectsConst()
{
	return activeGameWorldConst().objects;
}

void bindActiveWorld(GameWorld &world)
{
	GameSessionWorlds &s = GameSessionWorlds::instance();
	ASSERT(&world == s.primary.get() || &world == s.offworld.get(),
	       "bindActiveWorld: world not owned by session");
	s.active = &world;
}

bool hasActiveWorld()
{
	return GameSessionWorlds::instance().active != nullptr;
}

GameWorld &missionParkedHomeWorld()
{
	GameSessionWorlds &s = GameSessionWorlds::instance();
	if (!s.primary)
	{
		s.primary = std::make_unique<GameWorld>();
		s.primary->debugName = "primary";
	}
	return *s.primary;
}

void ensureOffworldWorld()
{
	GameSessionWorlds &s = GameSessionWorlds::instance();
	if (!s.offworld)
	{
		s.offworld = std::make_unique<GameWorld>();
		s.offworld->debugName = "offworld";
	}
}

GameWorld &offworldWorld()
{
	ensureOffworldWorld();
	return *GameSessionWorlds::instance().offworld;
}
