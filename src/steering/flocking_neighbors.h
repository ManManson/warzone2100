// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project
*/

/**
 * @file flocking_neighbors.h
 * Collect qualifying friendly droids for flock steering modifiers.
 */

#pragma once

#include <cstddef>

#include "flocking_constants.h"

struct DROID;

namespace steering
{

struct SteeringContext;

/// Same-player flock mates: alive droids, VTOL plane matches ours, not transporters, not self.
bool isValidFlockMate(const DROID* neighbor, const DROID* ourDroid);

/// Fill \p out with up to \p maxOut pointers (cap with MAX_FLOCK_NEIGHBORS_COLLECTED). Returns count written.
unsigned collectFlockNeighbors(const SteeringContext& ctx, DROID* out[], unsigned maxOut);

} // namespace steering
