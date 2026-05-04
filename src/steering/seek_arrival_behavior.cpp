// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project

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
/**
 * @file seek_arrival_behavior.cpp
 */

#include "seek_arrival_behavior.h"

#include "lib/framework/trig.h"
#include "lib/framework/vector.h"
#include "lib/wzmaplib/include/wzmaplib/map.h"

#include "src/actiondef.h"
#include "src/droid.h"
#include "src/movedef.h"
#include "src/statsdef.h"

#include <algorithm>

namespace steering
{

namespace
{

// Match move.cpp moveCheckFinalWaypoint / MIN_END_SPEED / END_SPEED_RANGE.
constexpr int32_t MIN_END_SPEED = 60;
constexpr int32_t END_SPEED_RANGE = 3 * TILE_UNITS;

static bool isPersonOrCyborg(const DROID* droid)
{
	switch (droid->droidType)
	{
	case DROID_PERSON:
	case DROID_CYBORG:
	case DROID_CYBORG_CONSTRUCT:
	case DROID_CYBORG_REPAIR:
	case DROID_CYBORG_SUPER:
		return true;
	default:
		return false;
	}
}

static bool arrivalSlowdownActive(const SteeringContext& ctx)
{
	const DROID* droid = ctx.droid;
	if (!droid)
	{
		return false;
	}
	if (isPersonOrCyborg(droid))
	{
		return false;
	}
	if (droid->isVtol() && droid->action == DACTION_VTOLATTACK)
	{
		return false;
	}
	if (droid->sMove.Status == MOVESHUFFLE)
	{
		return false;
	}
	if (droid->sMove.pathIndex != static_cast<int>(droid->sMove.asPath.size()))
	{
		return false;
	}
	return true;
}

} // namespace

SteeringForce SeekArrivalBehavior::calculate(const SteeringContext& ctx)
{
	Vector2i toTarget = ctx.targetPos - ctx.currentPos;
	const int32_t dist = iHypot(toTarget);
	if (dist == 0 || ctx.cruiseSpeed <= 0)
	{
		return SteeringForce(Vector2i(0, 0), 0);
	}

	int32_t speedMag = ctx.cruiseSpeed;

	if (arrivalSlowdownActive(ctx))
	{
		const int32_t distSq = dot(toTarget, toTarget);
		const int64_t rangeSq = static_cast<int64_t>(END_SPEED_RANGE) * END_SPEED_RANGE;
		if (distSq < rangeSq)
		{
			const int32_t minEndSpeed = std::min((speedMag + 2) / 3, MIN_END_SPEED);
			speedMag = static_cast<int32_t>((static_cast<int64_t>(speedMag - minEndSpeed) * distSq) / rangeSq + minEndSpeed);
		}
	}

	const uint16_t dir = iAtan2(toTarget);
	const Vector2i vel = iSinCosR(dir, speedMag);
	return SteeringForce(vel, WEIGHT);
}

} // namespace steering
