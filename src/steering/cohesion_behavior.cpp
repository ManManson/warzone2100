// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project
*/

/**
 * @file cohesion_behavior.cpp
 */

#include "cohesion_behavior.h"
#include "flocking_constants.h"
#include "flocking_neighbors.h"

#include "lib/framework/trig.h"
#include "lib/framework/vector.h"

#include "src/droid.h"

#include <algorithm>

namespace steering
{

bool CohesionBehavior::isEnabled(const SteeringContext& ctx) const
{
	if (!ctx.droid)
	{
		return false;
	}
	return ctx.maxSpeed != 0;
}

SteeringForce CohesionBehavior::calculate(const SteeringContext& ctx)
{
	if (!isEnabled(ctx) || ctx.desiredSpeedForAvoidance <= 0)
	{
		return SteeringForce(Vector2i(0, 0), 0);
	}

	DROID* neighbors[MAX_FLOCK_NEIGHBORS_COLLECTED];
	const unsigned n = collectFlockNeighbors(ctx, neighbors, MAX_FLOCK_NEIGHBORS_COLLECTED);
	if (n < MIN_FLOCK_NEIGHBORS)
	{
		return SteeringForce(Vector2i(0, 0), 0);
	}

	int64_t sumX = 0;
	int64_t sumY = 0;
	for (unsigned i = 0; i < n; ++i)
	{
		const Vector2i p = neighbors[i]->pos.xy();
		sumX += p.x;
		sumY += p.y;
	}

	const Vector2i centroid(static_cast<int32_t>(sumX / static_cast<int>(n)),
	                        static_cast<int32_t>(sumY / static_cast<int>(n)));

	Vector2i steer = centroid - ctx.currentPos;
	if (steer.x == 0 && steer.y == 0)
	{
		return SteeringForce(Vector2i(0, 0), 0);
	}

	const int32_t maxMag = ctx.desiredSpeedForAvoidance * COHESION_MAX_SPEED_FRAC_NUM / COHESION_MAX_SPEED_FRAC_DEN;
	const int32_t h = iHypot(steer);
	if (h > maxMag && maxMag > 0)
	{
		steer = Vector2i(static_cast<int32_t>(static_cast<int64_t>(steer.x) * maxMag / h),
		                 static_cast<int32_t>(static_cast<int64_t>(steer.y) * maxMag / h));
	}

	return SteeringForce(steer, COHESION_BLEND_WEIGHT);
}

} // namespace steering
