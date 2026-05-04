// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project
*/

/**
 * @file flocking_constants.h
 * Parameters for flock modifier behaviors (separation / alignment / cohesion).
 */

#pragma once

#include "lib/wzmaplib/include/wzmaplib/map.h"

#include "utils.h"

namespace steering
{

/// Minimum number of qualifying flock mates (excluding self) before modifiers contribute.
constexpr unsigned MIN_FLOCK_NEIGHBORS = 2;

/// Spatial query radius for flock neighbors (world units).
constexpr uint32_t FLOCK_SCAN_RADIUS = TILE_UNITS * 2;

/// Upper bound on neighbors collected per tick (stack buffer size).
constexpr unsigned MAX_FLOCK_NEIGHBORS_COLLECTED = 24;

/// Separation steering capped as a fraction of locomotion speed (`desiredSpeedForAvoidance`).
constexpr int32_t SEPARATION_MAX_SPEED_FRAC_NUM = 1;
constexpr int32_t SEPARATION_MAX_SPEED_FRAC_DEN = 4;

/// Blend weight vs locomotion in `combineForcesImpl` (keep modest vs collision).
constexpr int32_t SEPARATION_BLEND_WEIGHT = PRECISION / 4;

/// Squared distance floor when weighting separation (avoids huge impulses when overlapping).
constexpr int32_t SEPARATION_DIST_SQ_FLOOR = (TILE_UNITS / 8) * (TILE_UNITS / 8);

/// Cohesion: steer toward centroid of flock mates, capped like separation.
constexpr int32_t COHESION_MAX_SPEED_FRAC_NUM = 1;
constexpr int32_t COHESION_MAX_SPEED_FRAC_DEN = 4;
constexpr int32_t COHESION_BLEND_WEIGHT = PRECISION / 4;

} // namespace steering
