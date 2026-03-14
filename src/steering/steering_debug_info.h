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
 * @file Steering debug types and helpers.
 *
 * NOTE: The colour mapping below hashes the pointer value of the
 * behaviour name using `std::hash<const char*>`. This means the
 * behaviour `name()` must return a pointer with stable/static
 * lifetime (typically a string literal or static storage). The
 * mapping intentionally does NOT hash the string contents.
*/

#pragma once

#include "lib/framework/wzglobal.h"

#ifdef DEBUG

#include "lib/framework/vector.h"
#include <cstdint>
#include <functional>
#include <vector>

namespace steering
{

enum class SteeringDebugColor : uint8_t
{
	RED = 0,
	GREEN = 1,
	BLUE = 2,
	PURPLE = 3,
};

struct SteeringDebugInfo
{
	uint32_t frameNum = 0;                 // game frame snapshot taken
	Vector2i resultant = {0, 0};      // combined steering force (PRECISION)
	Vector2i toTarget = {0, 0};       // vector to target (world units)
	int32_t radius = 0;                    // the same as SteeringContext::radius

	struct ForceEntry
	{
		Vector2i force = {0, 0};
		int32_t weight = 0;
		const char* name = nullptr; // pointer to behaviour name (static lifetime)
		SteeringDebugColor color = SteeringDebugColor::RED;
	};

	std::vector<ForceEntry> forces;
};

inline SteeringDebugColor mapBehaviourColor(const char *name)
{
	if (!name)
	{
		return SteeringDebugColor::RED;
	}
	// Hash based on pointer value only. Behaviour `name()` MUST return
	// a stable pointer (e.g. string literal or static storage).
	size_t hv = std::hash<const char*>()(name);
	return static_cast<SteeringDebugColor>(hv % 4);
}

// Global toggle for overlay rendering.
bool isDebugOverlayEnabled();
void setDebugOverlayEnabled(bool v);

} // namespace steering

#endif // DEBUG
