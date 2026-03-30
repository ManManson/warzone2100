#pragma once

#include "gateway.h"
#include "map.h"
#include "objmem.h"
#include "src/basedef.h"

#include <memory>
#include <string>

#include <stdint.h>

struct WorldScrollLimits
{
	int32_t minX = 0;
	int32_t minY = 0;
	int32_t maxX = 0;
	int32_t maxY = 0;
};

struct WorldMapState
{
	std::unique_ptr<MAPTILE[]> tiles;
	int32_t width = 0;
	int32_t height = 0;
	std::unique_ptr<uint8_t[]> blockMap[AUX_MAX];
	std::unique_ptr<uint8_t[]> auxMap[MAX_PLAYERS + AUX_MAX];
	WorldScrollLimits scroll;
	GATEWAY_LIST gateways;
};

struct WorldObjectState
{
	PerPlayerDroidLists droids;
	PerPlayerStructureLists structures;
	PerPlayerFeatureLists features;
	PerPlayerFlagPositionLists flags;
	PerPlayerExtractorLists extractors;
	GlobalSensorList sensors;
	GlobalOilList oils;
	DestroyedObjectsList destroyedObjects;
};

struct GameWorld
{
	WorldMapState map;
	WorldObjectState objects;
	std::string debugName;

	bool isLoaded() const;
	void reset();
};

// World-parameterized map accessors (GAME_WORLD_REFACTORING_V2_IMPL §5.1–5.2). Same indexing as global mapTile / auxTile / blockTile in map.h.
inline MAPTILE *mapTile(GameWorld &world, int32_t x, int32_t y)
{
	return mapTile(world.map, x, y);
}

inline uint8_t auxTile(WorldMapState &m, int x, int y, int player)
{
	ASSERT_OR_RETURN(AUXBITS_ALL, player >= 0 && player < MAX_PLAYERS + AUX_MAX, "invalid player: %d", player);
	return m.auxMap[player][x + y * m.width];
}

inline uint8_t blockTile(WorldMapState &m, int x, int y, int slot)
{
	return m.blockMap[slot][x + y * m.width];
}

inline uint8_t auxTile(GameWorld &world, int x, int y, int player)
{
	return auxTile(world.map, x, y, player);
}

inline uint8_t blockTile(GameWorld &world, int x, int y, int slot)
{
	return blockTile(world.map, x, y, slot);
}
