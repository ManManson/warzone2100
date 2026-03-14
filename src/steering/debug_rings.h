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

#pragma once

#ifdef DEBUG

#include <vector>

#include "lib/framework/vector.h"

#include <glm/vec4.hpp>

namespace steering
{

// Per-droid steering debug ring descriptor used by terrain SDF rendering.
struct SteeringRingDebug
{
	Vector2i center = {0, 0};  // world XY in map units
	int32_t radius = 0;        // world units (outer radius)
	int32_t thickness = 0;     // ring width in world units
	glm::vec4 color = {0, 0, 0, 0}; // RGBA
};

void buildSteeringRings(std::vector<SteeringRingDebug>& outRings);

} // namespace steering

#endif // debug
