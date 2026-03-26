#include "world_binding.h"

#include "game_world.h"

#include "lib/framework/frame.h" // for ASSERT

static GameWorld* gActiveWorld = nullptr;
static GameWorld gPrimaryWorld;

GameWorld& primaryGameWorld()
{
	return gPrimaryWorld;
}

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

void unbindActiveWorld()
{
	gActiveWorld = nullptr;
}

bool hasActiveWorld()
{
	return gActiveWorld != nullptr;
}
bool isActiveWorld(const GameWorld& world)
{
	return gActiveWorld == &world;
}
bool isActiveWorld(const GameWorld* world)
{
	return gActiveWorld == world;
}

std::unique_ptr<MAPTILE[]>& activeMapTiles()
{
	return activeGameWorld().map.tiles;
}

const std::unique_ptr<MAPTILE[]>& activeMapTilesConst()
{
	return activeGameWorldConst().map.tiles;
}

int32_t& activeMapWidth()
{
	return activeGameWorld().map.width;
}

const int32_t& activeMapWidthConst()
{
	return activeGameWorldConst().map.width;
}

int32_t& activeMapHeight()
{
	return activeGameWorld().map.height;
}

const int32_t& activeMapHeightConst()
{
	return activeGameWorldConst().map.height;
}

std::unique_ptr<uint8_t[]>& activeBlockMap(int slot)
{
	ASSERT(slot >= 0 && slot < AUX_MAX, "Invalid block map slot: %d", slot);
	return activeGameWorld().map.blockMap[slot];
}

const std::unique_ptr<uint8_t[]>& activeBlockMapConst(int slot)
{
	ASSERT(slot >= 0 && slot < AUX_MAX, "Invalid block map slot: %d", slot);
	return activeGameWorldConst().map.blockMap[slot];
}

std::unique_ptr<uint8_t[]>& activeAuxMap(int slot)
{
	ASSERT(slot >= 0 && slot < MAX_PLAYERS + AUX_MAX, "Invalid aux map slot: %d", slot);
	return activeGameWorld().map.auxMap[slot];
}

const std::unique_ptr<uint8_t[]>& activeAuxMapConst(int slot)
{
	ASSERT(slot >= 0 && slot < MAX_PLAYERS + AUX_MAX, "Invalid aux map slot: %d", slot);
	return activeGameWorldConst().map.auxMap[slot];
}

WorldScrollLimits& activeScrollLimits()
{
	return activeGameWorld().map.scroll;
}

const WorldScrollLimits& activeScrollLimitsConst()
{
	return activeGameWorldConst().map.scroll;
}
