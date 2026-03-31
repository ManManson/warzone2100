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

#ifndef __INCLUDED_SRC_GEOMETRY_H__
#define __INCLUDED_SRC_GEOMETRY_H__

#include "map.h"
#include "game_world.h"

struct QUAD
{
	Vector2i coords[4] = {{0,0}, {0,0}, {0,0}, {0,0}};
};

uint16_t calcDirection(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
bool inQuad(const Vector2i *pt, const QUAD *quad);
Vector2i positionInQuad(Vector2i const &pt, QUAD const &quad);
DROID *getNearestDroid(UDWORD x, UDWORD y, bool bSelected);
bool objectOnScreen(const BASE_OBJECT *object, SDWORD tolerance);

STRUCTURE *getTileStructure(GameWorld &world, UDWORD x, UDWORD y);
FEATURE *getTileFeature(GameWorld &world, UDWORD x, UDWORD y);
BASE_OBJECT *getTileOccupier(GameWorld &world, UDWORD x, UDWORD y);

#endif // __INCLUDED_SRC_GEOMETRY_H__
