#pragma once

#include "map.h"
#include "objmem.h"
#include "src/basedef.h"

#include <memory>
#include <string>

#include <stdint.h>

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
