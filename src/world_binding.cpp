#include "world_binding.h"

#include "game_world.h"

#include "lib/framework/frame.h" // for ASSERT

static GameWorld* gActiveWorld = nullptr;

GameWorld& activeGameWorld()
{
	ASSERT(gActiveWorld != nullptr, "No active world bound");
	return *gActiveWorld;
}

const GameWorld& activeGameWorldConst()
{
	ASSERT(gActiveWorld != nullptr, "No active world bound");
	return *gActiveWorld;
}

void bindActiveWorld(GameWorld& world)
{
	gActiveWorld = &world;
}

bool hasActiveWorld()
{
	return gActiveWorld != nullptr;
}
