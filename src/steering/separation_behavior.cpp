// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project
*/

/**
 * @file separation_behavior.cpp
 */

#include "separation_behavior.h"
#include "flocking_constants.h"
#include "flocking_neighbors.h"
#include "utils.h"

#include "lib/framework/trig.h"
#include "lib/framework/vector.h"

#include "src/droid.h"

#include <algorithm>

namespace steering
{

bool SeparationBehavior::isEnabled(const SteeringContext& ctx) const
{
	if (!ctx.droid)
	{
		return false;
	}
	return ctx.maxSpeed != 0;
}

SteeringForce SeparationBehavior::calculate(const SteeringContext& ctx)
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

	int64_t accX = 0;
	int64_t accY = 0;

	for (unsigned i = 0; i < n; ++i)
	{
		const DROID* mate = neighbors[i];
		const Vector2i diff = ctx.currentPos - mate->pos.xy();
		const int64_t distSq = std::max<int64_t>(dot(diff, diff), SEPARATION_DIST_SQ_FLOOR);

		accX += static_cast<int64_t>(diff.x) * PRECISION / distSq;
		accY += static_cast<int64_t>(diff.y) * PRECISION / distSq;
	}

	Vector2i steer(static_cast<int32_t>(accX), static_cast<int32_t>(accY));

	const int32_t maxMag = ctx.desiredSpeedForAvoidance * SEPARATION_MAX_SPEED_FRAC_NUM / SEPARATION_MAX_SPEED_FRAC_DEN;
	const int32_t h = iHypot(steer);
	if (h > maxMag && maxMag > 0)
	{
		steer = Vector2i(static_cast<int32_t>(static_cast<int64_t>(steer.x) * maxMag / h),
		                 static_cast<int32_t>(static_cast<int64_t>(steer.y) * maxMag / h));
	}

	return SteeringForce(steer, SEPARATION_BLEND_WEIGHT);
}

} // namespace steering
