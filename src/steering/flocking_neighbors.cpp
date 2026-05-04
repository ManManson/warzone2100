// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project
*/

/**
 * @file flocking_neighbors.cpp
 */

#include "flocking_neighbors.h"
#include "steering.h"

#include "src/droid.h"
#include "src/mapgrid.h"
#include "src/objects.h"

#include <algorithm>

namespace steering
{

bool isValidFlockMate(const DROID* neighbor, const DROID* ourDroid)
{
	if (!neighbor || !ourDroid || neighbor == ourDroid)
	{
		return false;
	}
	if (neighbor->died != 0)
	{
		return false;
	}
	if (neighbor->player != ourDroid->player)
	{
		return false;
	}
	if (ourDroid->isVtol() != neighbor->isVtol())
	{
		return false;
	}
	if (neighbor->isTransporter())
	{
		return false;
	}
	return true;
}

unsigned collectFlockNeighbors(const SteeringContext& ctx, DROID* out[], unsigned maxOut)
{
	const DROID* our = ctx.droid;
	if (!our || maxOut == 0)
	{
		return 0;
	}

	const unsigned cap = std::min(maxOut, MAX_FLOCK_NEIGHBORS_COLLECTED);
	unsigned count = 0;

	const GridList& gridList = gridStartIterateDroidsByPlayer(ctx.currentPos.x, ctx.currentPos.y, FLOCK_SCAN_RADIUS, static_cast<int>(our->player));
	for (GridIterator gi = gridList.begin(); gi != gridList.end(); ++gi)
	{
		BASE_OBJECT* obj = *gi;
		DROID* neighbor = castDroid(obj);
		if (!isValidFlockMate(neighbor, our))
		{
			continue;
		}
		out[count++] = neighbor;
		if (count >= cap)
		{
			break;
		}
	}
	return count;
}

} // namespace steering
