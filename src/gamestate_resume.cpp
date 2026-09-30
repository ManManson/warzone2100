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

#include "gamestate_resume.h"

#include "lib/framework/frame.h"
#include "lib/gamelib/gtime.h"
#include "lib/netplay/sync_debug.h"

#include "game_world.h"
#include "map.h"
#include "mapgrid.h"
#include "pathfinding_backend.h"
#include "fpath.h"
#include "mechanics.h"
#include "visibility.h"
#include "structure.h"
#include "message.h"
#include "corridor_gate.h"

#include <nlohmann/json.hpp>

namespace gamestate
{

void simSubsystemsReset()
{
	// Stop the SKIRMISH danger worker (if one is running) BEFORE the world is torn down and the aux/block
	// maps are reallocated by readMapTerrain - the worker writes auxMap[MAX_PLAYERS+AUX_DANGERMAP] /
	// reads blockMap[AUX_DANGERMAP], so reallocating those underneath it would race / use-after-free.
	// No-op on the disk cold-load path (mapInit has not started a worker yet) - the in-process round-trip
	// and in-place resume are the cases with a live worker here. The danger overlay is re-applied by the
	// dangerMaps post-pass and the worker is (re)started by the next mapInit.
	mapStopDangerThreadForReconstruct();

	// Drop queued path jobs and planner caches from the timeline we are leaving. In-place resume (replay
	// seek) otherwise leaves pre-restore jobs on the worker queues; snapshot pendingRoutes then sit
	// behind that backlog, first-tick MOVEWAITROUTE polls miss, and later searches reuse a stale
	// PathfindContext. Cold-load is a no-op (the pool is still empty). pendingRoutes are applied later,
	// after the world exists.
	fpathResetForWorldReconstruct();

	// Purge the pending destroyed-object list before teardown. Objects killed in the tick(s) before the
	// snapshot were moved off the world lists onto the global psDestroyedObj, so clearWorldObjects never
	// frees them. Left in place, the next objmemUpdate would fire triggerEventDestroyed / free them on the
	// resumed tick - the pre-restore timeline leaking into the restored one. (No-op when the list is empty:
	// cold-load starts fresh, the in-process round-trip usually has none pending.)
	mechanicsPurgeDestroyedObjects();

	// Tear down any live script spotters before the world/map is reset. They hold watchedTiles into the
	// active map - clearWorldObjects/readMapTerrain zero and reallocate the per-tile watcher/sensor counts,
	// so a surviving spotter's destructor would later underflow those counts. The snapshot's own spotters
	// are rebuilt by the "spotters" section after the world is restored.
	removeSpotters();

	clearAllRecycledExperience(); // repopulated by the recycledExperience section
	initFactoryNumFlag();         // rebuilt by resetFactoryNumFlag after the world
	releaseAllProxDisp();         // proximity displays reference messages/objects
	corridorGateResetDebugLogs(); // g_logClass/g_logSpeed (debug-only, deterministic anyway)
}

void reconstructWorldFromSnapshot(const nlohmann::ordered_json &j, ReconstructOptions options)
{
	gameStateFromJson(j, options.scriptScope, options.deferMessages);
}

bool restartSimWorkers()
{
	if (!fpathActiveBackend().initialise())
	{
		return false;
	}
	if (!gameWorld.map.tiles)
	{
		return true;
	}
	mapInit(gameWorld);
	gridReset(gameWorld);
	return true;
}

void seedResumeSyncDebug()
{
	resetSyncDebug();
	applyResumeSyncDebugCrc();
	setSyncCheckFloorTime(gameTime);
	// No-op unless --gamestate-crc-detail-on-save. The next 20 sendPlayerGameTime
	// calls append full buckets to <trace>.detail.txt. 20 covers the observed
	// first failures (about 5 and 9 ticks after the 60s keyframe).
	debug(LOG_INFO, "sync check floor gameTime=%u", gameTime);
	syncCrcDetailArmOnSaveOrLoad();
}

void resumeFromSnapshot(const nlohmann::ordered_json &j,
                        const std::function<void()> &restoreQueues,
                        ScriptScope scriptScope)
{
	reconstructWorldFromSnapshot(j, ReconstructOptions{scriptScope, false});
	if (!restartSimWorkers())
	{
		throw StateError("failed to restart sim workers after snapshot reconstruct");
	}
	seedResumeSyncDebug();
	if (restoreQueues)
	{
		restoreQueues();
	}
}

} // namespace gamestate
