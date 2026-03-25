#include "game_world.h"

#include <stdlib.h>

bool GameWorld::isLoaded() const
{
	return map.tiles != nullptr && map.width > 0 && map.height > 0;
}

void GameWorld::reset()
{
	map.tiles.reset();
	map.width = 0;
	map.height = 0;

	for (auto& slot : map.blockMap)
	{
		slot.reset();
	}
	for (auto& slot : map.auxMap)
	{
		slot.reset();
	}
	for (auto* gateway : map.gateways)
	{
		free(gateway);
	}
	map.gateways.clear();

	map.scroll = {};
	objects = {};
	debugName.clear();
}
