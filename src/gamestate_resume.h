// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project (https://github.com/Warzone2100)

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
/** \file
 *  GameState resume pipeline: reconstruct the world from a snapshot, restart
 *  process-lifetime workers, re-seed the sync CRC, then restore game queues.
 *
 *  Replay rewind and the in-game round-trip call resumeFromSnapshot(). Cold-load
 *  uses the same three phases at the points level-load already splits.
 */

#pragma once

#include "gamestate_serialize.h"

#include <functional>
#include <nlohmann/json_fwd.hpp>

namespace gamestate
{

struct ReconstructOptions
{
	ScriptScope scriptScope = ScriptScope::AllInstances;
	bool deferMessages = false;
};

/// Phase 1: teardown process-lifetime workers (inside gameStateFromJson) and apply the document.
void reconstructWorldFromSnapshot(const nlohmann::ordered_json &j,
                                  ReconstructOptions options = {});

/// Phase 2: start workers the way a fresh process would. No-op mapInit/gridReset when no tiles
/// (headless self-test). Returns false if fpath.initialise fails.
bool restartSimWorkers();

/// Phase 3: drop reconstruct hashes, re-seed the accumulator, floor GAME_GAME_TIME CRC checks.
/// Call after any caller-owned reconstruct events (cold-load: after TRIGGER_GAME_LOADED).
void seedResumeSyncDebug();

/// Live-engine one-shot: reconstruct + workers + CRC + queues last.
/// Empty restoreQueues leaves game queues untouched (round-trip).
void resumeFromSnapshot(const nlohmann::ordered_json &j,
                        const std::function<void()> &restoreQueues,
                        ScriptScope scriptScope = ScriptScope::AllInstances);

} // namespace gamestate
