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
