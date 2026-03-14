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

#ifdef DEBUG

// Ensure GLM swizzle extensions are enabled for this translation unit
#ifndef GLM_FORCE_SWIZZLE
#define GLM_FORCE_SWIZZLE
#endif
#ifndef GLM_FORCE_SILENT_WARNINGS
#define GLM_FORCE_SILENT_WARNINGS
#endif

#include "debug_overlay.h"
#include "steering_debug_info.h"
#include "utils.h" // PRECISION

#include "../objmem.h"
#include "../baseobject.h"
#include "lib/ivis_opengl/piepalette.h"
#include "lib/ivis_opengl/piematrix.h"
#include "lib/ivis_opengl/textdraw.h"
#include "lib/ivis_opengl/piefunc.h"
#include "../map.h"
#include "../display3d.h"
#include "lib/gamelib/gtime.h"

namespace steering
{

namespace
{

// Tunables (in tiles / world units)
constexpr float DEBUG_ARROW_SCALE_TILES = 2.0f;           // tiles per unit force
constexpr float DEBUG_ARROW_WIDTH_TILES = 0.06f;          // visual thickness
constexpr int32_t DEBUG_ELEVATION       = TILE_UNITS / 4; // above terrain / droid body

PIELIGHT colourFor(SteeringDebugColor c)
{
	switch (c)
	{
		case SteeringDebugColor::RED:    return WZCOL_RED;
		case SteeringDebugColor::GREEN:  return WZCOL_GREEN;
		case SteeringDebugColor::BLUE:   return pal_RGBA(64, 164, 255, 255);
		case SteeringDebugColor::PURPLE: return pal_RGBA(200, 64, 200, 255);
		default:                         return WZCOL_WHITE;
	}
}

Vector3f makeBasePos(const DROID *psDroid)
{
	// Match renderer mapping
	// x = worldX, y = worldZ, z = -worldY
	Spacetime st = interpolateObjectSpacetime(psDroid, graphicsTime);
	return Vector3f(st.pos.x, st.pos.z + DEBUG_ELEVATION, -st.pos.y);
}

void drawArrowHead(const Vector3f &headBase,
	const Vector3f &tipPos,
	PIELIGHT col,
	const glm::mat4 &viewMatrix)
{
	Vector3f dir = tipPos - headBase;
	float len = glm::length(dir);
	if (len < 1e-3f)
	{
		return;
	}
	Vector3f dirN = dir / len;

	const float widthFrac    = 0.25f;
	const float minHeadWidth = 0.15f * TILE_UNITS;

	const float headWidth = std::max(minHeadWidth, len * widthFrac);

	Vector3f side = glm::normalize(Vector3f(-dirN.z, 0.0f, dirN.x));
	Vector3f sideOffset = side * (headWidth * 0.5f);

	Vector3f baseL = headBase + sideOffset;
	Vector3f baseR = headBase - sideOffset;

	pie_TransColouredTriangle({ baseL, tipPos, baseR }, col, viewMatrix);
}

void drawArrowBodyAndHead(const Vector3f &basePos,
	const Vector2i &force,
	PIELIGHT col,
	float arrowScaleWorld,
	float halfWidthWorld,
	const glm::mat4 &viewMatrix)
{
	// Convert fixed-point force to world-space delta in the renderer's XZ plane.
	const float dxWorld = static_cast<float>(force.x) * arrowScaleWorld;
	const float dzWorld = -static_cast<float>(force.y) * arrowScaleWorld; // note minus

	Vector3f tipPos(basePos.x + dxWorld, basePos.y, basePos.z + dzWorld);

	Vector3f dir = tipPos - basePos;
	float len = glm::length(dir);
	if (len < 1e-3f)
	{
		return;
	}
	Vector3f dirN = dir / len;

	// Arrowhead shape (relative to arrow length, with minimums in world units)
	constexpr float HEAD_FRAC    = 0.3f;
	constexpr float MIN_HEAD_LEN = 0.1f * TILE_UNITS;

	const float headLen    = std::max(MIN_HEAD_LEN, len * HEAD_FRAC);

	// Compute head base, then use it as the end of the body
	Vector3f headBase = tipPos - dirN * headLen;

	// Side vector in XZ plane for ribbon width
	Vector3f side = glm::normalize(Vector3f(-dirN.z, 0.0f, dirN.x));
	Vector3f sideOffset = side * halfWidthWorld;

	// Ribbon quad
	Vector3f bodyBaseL = basePos + sideOffset;
	Vector3f bodyBaseR = basePos - sideOffset;
	Vector3f bodyTipL  = headBase + sideOffset;
	Vector3f bodyTipR  = headBase - sideOffset;

	pie_TransColouredTriangle({ bodyBaseL, bodyTipL, bodyTipR }, col, viewMatrix);
	pie_TransColouredTriangle({ bodyBaseL, bodyTipR, bodyBaseR }, col, viewMatrix);

	drawArrowHead(headBase, tipPos, col, viewMatrix);
}

} // anonymous namespace

void renderSteeringOverlay(const glm::mat4& viewMatrix)
{
	// Scale: fixed-point force (PRECISION) -> world units (TILE_UNITS)
	constexpr float arrowScaleWorld =
		DEBUG_ARROW_SCALE_TILES * TILE_UNITS / static_cast<float>(PRECISION);
	constexpr float halfWidthWorld =
		0.5f * (DEBUG_ARROW_WIDTH_TILES * TILE_UNITS);

	// For each player list
	for (unsigned player = 0; player < MAX_PLAYERS; ++player)
	{
		for (DROID* psDroid : apsDroidLists[player])
		{
			if (!psDroid)
			{
				continue;
			}

			if (psDroid->died != 0 && psDroid->died < graphicsTime)
			{
				continue;
			}

			// Cull quickly using tile-based clipping
			if (!clipXY(psDroid->pos.x, psDroid->pos.y))
			{
				continue;
			}

			// Snapshot from steering
			const SteeringDebugInfo &dbg = psDroid->steeringDebug;
			if (dbg.frameNum == 0 || dbg.frameNum < frameGetFrameNumber() - 5)
			{
				continue; // never initialised
			}

			Vector3f basePos = makeBasePos(psDroid);

			// Draw each behaviour force
			for (const auto &f : dbg.forces)
			{
				if (f.force.x == 0 && f.force.y == 0)
				{
					continue;
				}

				drawArrowBodyAndHead(basePos, f.force * f.weight / PRECISION, colourFor(f.color),
					arrowScaleWorld, halfWidthWorld,
					viewMatrix);
			}

			// Resultant arrow (thicker / white)
			if (dbg.resultant.x != 0 || dbg.resultant.y != 0)
			{
				drawArrowBodyAndHead(basePos, dbg.resultant, WZCOL_WHITE,
					arrowScaleWorld, halfWidthWorld * 1.4f,
					viewMatrix);
			}
		}
	}
}

} // namespace steering

#endif // DEBUG
