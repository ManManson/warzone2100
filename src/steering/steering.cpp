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
 * @file steering.cpp
 * Steering manager implementation.
 */

#include "steering.h"
#include "utils.h" // for PRECISION

#include "lib/framework/trig.h"
#include "lib/framework/vector.h"

namespace steering
{

namespace
{

static SteeringForce fallbackLocomotion(const SteeringContext& ctx)
{
	Vector2i toTarget = ctx.targetPos - ctx.currentPos;
	if (iHypot(toTarget) == 0 || ctx.cruiseSpeed <= 0)
	{
		return SteeringForce(Vector2i(0, 0), 0);
	}
	const uint16_t dir = iAtan2(toTarget);
	return SteeringForce(iSinCosR(dir, ctx.cruiseSpeed), PRECISION);
}

} // namespace

SteeringManager::~SteeringManager() = default;

uint16_t SteeringMoveIntent::direction(const SteeringContext& ctx) const
{
	if (velocity.x != 0 || velocity.y != 0)
	{
		return iAtan2(velocity);
	}
	const Vector2i toTarget = ctx.targetPos - ctx.currentPos;
	return iAtan2(toTarget);
}

int32_t SteeringMoveIntent::speedScalar() const
{
	return iHypot(velocity);
}

void SteeringManager::addBehavior(std::unique_ptr<ISteeringBehavior> behavior)
{
	_behaviors.emplace_back(std::move(behavior));
	_forces.reserve(_behaviors.size());
}

SteeringMoveIntent SteeringManager::calculateMoveIntent(SteeringContext& ctx)
{
	ctx.desiredSpeedForAvoidance = 0;
	_forces.clear();

	SteeringForce locomotion;
	bool gotLocomotion = false;
	for (const auto& behavior : _behaviors)
	{
		if (behavior->category() != SteeringBehaviorCategory::Locomotion)
		{
			continue;
		}
		if (!behavior->isEnabled(ctx))
		{
			continue;
		}
		locomotion = behavior->calculate(ctx);
		gotLocomotion = true;
		break;
	}
	if (!gotLocomotion || locomotion.isZero())
	{
		locomotion = fallbackLocomotion(ctx);
	}
	if (!locomotion.isZero())
	{
		ctx.desiredSpeedForAvoidance = iHypot(locomotion.force);
		_forces.push_back(locomotion);
	}

	for (const auto& behavior : _behaviors)
	{
		if (behavior->category() != SteeringBehaviorCategory::Modifier)
		{
			continue;
		}
		if (!behavior->isEnabled(ctx))
		{
			continue;
		}
		SteeringForce force = behavior->calculate(ctx);
		if (!force.isZero())
		{
			_forces.push_back(force);
		}
	}

	return SteeringMoveIntent{combineForcesImpl(_forces)};
}

Vector2i SteeringManager::calculateSteering(const SteeringContext& ctx)
{
	SteeringContext ctxMut = ctx;
	return calculateMoveIntent(ctxMut).velocity;
}

uint16_t SteeringManager::calculateSteeringDirection(const SteeringContext& ctx)
{
	SteeringContext ctxMut = ctx;
	const SteeringMoveIntent intent = calculateMoveIntent(ctxMut);
	return intent.direction(ctxMut);
}

Vector2i SteeringManager::combineForcesImpl(const std::vector<SteeringForce>& forces)
{
	if (forces.empty())
	{
		return Vector2i(0, 0);
	}

	int64_t totalX = 0;
	int64_t totalY = 0;
	int64_t totalWeight = 0;

	for (const auto& f : forces)
	{
		int64_t weightedX = static_cast<int64_t>(f.force.x) * f.weight / PRECISION;
		int64_t weightedY = static_cast<int64_t>(f.force.y) * f.weight / PRECISION;

		totalX += weightedX;
		totalY += weightedY;
		totalWeight += f.weight;
	}

	if (totalWeight == 0)
	{
		return Vector2i(0, 0);
	}

	int32_t resultX = static_cast<int64_t>(totalX) * PRECISION / totalWeight;
	int32_t resultY = static_cast<int64_t>(totalY) * PRECISION / totalWeight;

	return Vector2i(resultX, resultY);
}

} // namespace steering
