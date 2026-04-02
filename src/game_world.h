#pragma once

#include "world_map_state.h"
#include "world_object_state.h"
#include "src/basedef.h"

#include <memory>
#include <string>

#include <stdint.h>

struct GameWorld
{
	WorldMapState map;
	WorldObjectState objects;
	std::string debugName;

	bool isLoaded() const;
	void reset();
};
