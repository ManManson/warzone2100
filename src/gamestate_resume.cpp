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

#include <nlohmann/json.hpp>

namespace gamestate
{

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
