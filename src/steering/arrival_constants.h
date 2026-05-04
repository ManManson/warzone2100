// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project
*/

/**
 * @file arrival_constants.h
 * Shared parameters for final-waypoint arrival slowdown (legacy `moveCheckFinalWaypoint` semantics).
 */

#pragma once

#include "lib/wzmaplib/include/wzmaplib/map.h"

namespace steering
{

/// Minimum speed at the destination under arrival slowdown (world speed units).
constexpr int32_t ARRIVAL_MIN_END_SPEED = 60;

/// Distance from final waypoint at which arrival slowdown begins (world units).
constexpr int32_t ARRIVAL_SLOWDOWN_RANGE = 3 * TILE_UNITS;

} // namespace steering
