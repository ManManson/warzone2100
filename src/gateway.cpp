/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2020  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/*
 * Gateway.c
 *
 * Routing gateway code.
 *
 */

#include "lib/framework/frame.h"

#include "map.h"
#include "wrappers.h"
#include "game_world.h"

#include "gateway.h"

static void gwFreeGateway(GATEWAY *psDel, WorldMapState *mapState);

static SDWORD gwMapWidth(WorldMapState &m)
{
	return (SDWORD)m.width;
}

static SDWORD gwMapHeight(WorldMapState &m)
{
	return (SDWORD)m.height;
}

static void gwSetGatewayFlag(WorldMapState &map, SDWORD x, SDWORD y)
{
	map.tiles[x + y * map.width].tileInfoBits |= BITS_GATEWAY;
}

static void gwClearGatewayFlag(WorldMapState &map, SDWORD x, SDWORD y)
{
	map.tiles[x + y * map.width].tileInfoBits &= ~BITS_GATEWAY;
}

bool gwInitialise(GameWorld &world)
{
	world.map.gateways.clear();
	return true;
}

void gwShutDown(GameWorld &world)
{
	for (auto psGateway : world.map.gateways)
	{
		gwFreeGateway(psGateway, &world.map);
	}
	world.map.gateways.clear();
}

bool gwNewGateway(GameWorld &world, int x1, int y1, int x2, int y2)
{
	WorldMapState &m = world.map;
	GATEWAY		*psNew;
	SDWORD		pos, temp;
	const SDWORD mw = gwMapWidth(m);
	const SDWORD mh = gwMapHeight(m);

	ASSERT_OR_RETURN(false, x1 >= 0 && x1 < mw && y1 >= 0 && y1 < mh
	                 && x2 >= 0 && x2 < mw && y2 >= 0 && y2 < mh
	                 && (x1 == x2 || y1 == y2), "Invalid gateway coordinates (%d, %d, %d, %d)",
	                 x1, y1, x2, y2);
	psNew = (GATEWAY *)malloc(sizeof(GATEWAY));

	// make sure the first coordinate is always the smallest
	if (x2 < x1)
	{
		// y is the same, swap x
		temp = x2;
		x2 = x1;
		x1 = temp;
	}
	else if (y2 < y1)
	{
		// x is the same, swap y
		temp = y2;
		y2 = y1;
		y1 = temp;
	}

	// Initialise the gateway, correct out-of-map gateways
	psNew->x1 = MAX(3, x1);
	psNew->y1 = MAX(3, y1);
	psNew->x2 = MIN(x2, mw - 4);
	psNew->y2 = MIN(y2, mh - 4);

	m.gateways.push_back(psNew);

	// set the map flags
	if (psNew->x1 == psNew->x2)
	{
		// vertical gateway
		for (pos = psNew->y1; pos <= psNew->y2; pos++)
		{
			gwSetGatewayFlag(m, psNew->x1, pos);
		}
	}
	else
	{
		// horizontal gateway
		for (pos = psNew->x1; pos <= psNew->x2; pos++)
		{
			gwSetGatewayFlag(m, pos, psNew->y1);
		}
	}

	return true;
}

size_t gwNumGateways(GameWorld &world)
{
	return world.map.gateways.size();
}

GATEWAY_LIST &gwGetGateways(GameWorld &world)
{
	return world.map.gateways;
}

static void gwFreeGateway(GATEWAY *psDel, WorldMapState *mapState)
{
	int pos;

	if (mapState == nullptr)
	{
		free(psDel);
		return;
	}
	if (!mapState->tiles)
	{
		free(psDel);
		return;
	}
	if (psDel->x1 == psDel->x2)
	{
		for (pos = psDel->y1; pos <= psDel->y2; pos++)
		{
			gwClearGatewayFlag(*mapState, psDel->x1, pos);
		}
	}
	else
	{
		for (pos = psDel->x1; pos <= psDel->x2; pos++)
		{
			gwClearGatewayFlag(*mapState, pos, psDel->y1);
		}
	}

	free(psDel);
}
