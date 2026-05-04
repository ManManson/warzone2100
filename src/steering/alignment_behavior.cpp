// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project
*/

/**
 * @file alignment_behavior.cpp
 */

#include "alignment_behavior.h"
#include "flocking_constants.h"
#include "flocking_neighbors.h"

#include "lib/framework/trig.h"
#include "lib/framework/vector.h"

#include "src/droid.h"

namespace steering
{

bool AlignmentBehavior::isEnabled(const SteeringContext& ctx) const
{
	if (!ctx.droid)
	{
		return false;
	}
	return ctx.maxSpeed != 0;
}

SteeringForce AlignmentBehavior::calculate(const SteeringContext& ctx)
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

	int64_t sumVx = 0;
	int64_t sumVy = 0;
	for (unsigned i = 0; i < n; ++i)
	{
		const DROID* mate = neighbors[i];
		const Vector2i v = iSinCosR(mate->sMove.moveDir, mate->sMove.speed);
		sumVx += v.x;
		sumVy += v.y;
	}

	const int32_t avgVx = static_cast<int32_t>(sumVx / static_cast<int>(n));
	const int32_t avgVy = static_cast<int32_t>(sumVy / static_cast<int>(n));

	Vector2i steer(avgVx - ctx.velocity.x, avgVy - ctx.velocity.y);
	if (steer.x == 0 && steer.y == 0)
	{
		return SteeringForce(Vector2i(0, 0), 0);
	}

	const int32_t maxMag = ctx.desiredSpeedForAvoidance * ALIGNMENT_MAX_SPEED_FRAC_NUM / ALIGNMENT_MAX_SPEED_FRAC_DEN;
	const int32_t h = iHypot(steer);
	if (h > maxMag && maxMag > 0)
	{
		steer = Vector2i(static_cast<int32_t>(static_cast<int64_t>(steer.x) * maxMag / h),
		                 static_cast<int32_t>(static_cast<int64_t>(steer.y) * maxMag / h));
	}

	return SteeringForce(steer, ALIGNMENT_BLEND_WEIGHT);
}

} // namespace steering
