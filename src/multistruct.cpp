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
/*
 * Multistruct.c
 *
 * Alex Lee 98, Pumpkin Studios.
 *
 * files to cope with multiplayer structure related stuff..
 */

#include <string.h>

#include "lib/framework/frame.h"
#include "lib/framework/math_ext.h"

#include "design.h"
#include "template.h"
#include "droid.h"
#include "droiddef.h"
#include "basedef.h"
#include "power.h"
#include "geometry.h"								// for gettilestructure
#include "stats.h"
#include "map.h"
#include "console.h"
#include "action.h"
#include "order.h"
#include "projectile.h"
#include "lib/netplay/netplay.h"								// the netplay library.
#include "multiplay.h"
#include "multigifts.h"
#include "multirecv.h"
#include "lib/sound/audio_id.h"
#include "lib/sound/audio.h"
#include "research.h"
#include "qtscript.h"
#include "combat.h"

// ////////////////////////////////////////////////////////////////////////////
// structures

// ////////////////////////////////////////////////////////////////////////////
// INFORM others that a building has been completed.
bool SendBuildFinished(STRUCTURE *psStruct)
{
	uint8_t player = psStruct->player;
	ASSERT_OR_RETURN(false, player < MAX_PLAYERS, "invalid player %u", player);

	NETbeginEncode(NETgameQueue(selectedPlayer), GAME_DEBUG_ADD_STRUCTURE);
	NETuint32_t(&psStruct->id);		// ID of building

	// Along with enough info to build it (if needed)
	NETuint32_t(&psStruct->pStructureType->ref);
	NETPosition(&psStruct->pos);
	NETuint8_t(&player);
	return NETend();
}

// ////////////////////////////////////////////////////////////////////////////
bool recvBuildFinished(NETQUEUE queue)
{
	uint32_t	structId;
	STRUCTURE	*psStruct;
	Position        pos;
	uint32_t	type, typeindex;
	uint8_t		player;

	{
		auto r = NETbeginDecode(queue, GAME_DEBUG_ADD_STRUCTURE);
		r.NETuint32_t(&structId);	// get the struct id.
		r.NETuint32_t(&type); 	// Kind of building.
		r.NETPosition(&pos);      // pos
		r.NETuint8_t(&player);
	}

	ASSERT_OR_RETURN(false, player < MAX_PLAYERS, "invalid player %u", player);

	const DebugInputManager& dbgInputManager = gInputManager.debugManager();
	if (!dbgInputManager.debugMappingsAllowed() && bMultiPlayer)
	{
		debug(LOG_WARNING, "Failed to add structure for player %u.", NetPlay.players[queue.index].position);
		return false;
	}

	psStruct = IdToStruct(structId, ANYPLAYER);

	if (psStruct)
	{
		// make it complete.
		psStruct->currentBuildPts = structureBuildPointsToCompletion(*psStruct) + 1;

		if (psStruct->status != SS_BUILT)
		{
			debug(LOG_SYNC, "Synch error, structure %u was not complete, and should have been.", structId);
			psStruct->status = SS_BUILT;
			buildingComplete(psStruct);
		}
		debug(LOG_SYNC, "Created normal building %u for player %u", psStruct->id, player);
		return true;
	}

	// The building wasn't started, so we'll have to just plonk it down in the map.

	// Find the structures stats
	for (typeindex = 0; typeindex < numStructureStats && asStructureStats[typeindex].ref != type; typeindex++) {}	// Find structure target

	// Build the structure
	psStruct = buildStructureDir(&(asStructureStats[typeindex]), pos.x, pos.y, 0, player, true, structId);
	if (psStruct)
	{
		psStruct->status	= SS_BUILT;
		buildingComplete(psStruct);
		debug(LOG_SYNC, "Huge synch error, forced to create building %u for player %u", psStruct->id, player);
#if defined (DEBUG)
		NETlogEntry("had to plonk down a building", SYNC_FLAG, player);
#endif
		triggerEventStructBuilt(psStruct, nullptr);
		checkPlayerBuiltHQ(psStruct);
	}
	else
	{
		debug(LOG_SYNC, "Gigantic synch error, unable to create building for player %u", player);
		NETlogEntry("had to plonk down a building, BUT FAILED!", SYNC_FLAG, player);
	}

	return false;
}


// ////////////////////////////////////////////////////////////////////////////
// Inform others that a structure has been destroyed
bool SendDestroyStructure(STRUCTURE *s)
{
	NETbeginEncode(NETgameQueue(selectedPlayer), GAME_DEBUG_REMOVE_STRUCTURE);

	// Struct to destroy
	NETuint32_t(&s->id);

	return NETend();
}

// ////////////////////////////////////////////////////////////////////////////
// acknowledge the destruction of a structure, from another player.
bool recvDestroyStructure(NETQUEUE queue)
{
	uint32_t structID;
	STRUCTURE *psStruct;

	{
		auto r = NETbeginDecode(queue, GAME_DEBUG_REMOVE_STRUCTURE);
		r.NETuint32_t(&structID);
	}

	const DebugInputManager& dbgInputManager = gInputManager.debugManager();
	if (!dbgInputManager.debugMappingsAllowed() && bMultiPlayer)
	{
		debug(LOG_WARNING, "Failed to remove structure for player %u.", NetPlay.players[queue.index].position);
		return false;
	}

	// Struct to destroy
	psStruct = IdToStruct(structID, ANYPLAYER);

	if (psStruct)
	{
		turnOffMultiMsg(true);
		// Remove the struct from remote players machine
		destroyStruct(psStruct, gameTime - deltaGameTime + 1);  // deltaGameTime is actually 0 here, since we're between updates. However, the value of gameTime - deltaGameTime + 1 will not change when we start the next tick.
		turnOffMultiMsg(false);
	}

	return true;
}

// ////////////////////////////////////////////////////////////////////////////
//lassat is firing

bool sendLasSat(UBYTE player, STRUCTURE *psStruct, BASE_OBJECT *psObj)
{
	NETbeginEncode(NETgameQueue(selectedPlayer), GAME_LASSAT);

	NETuint8_t(&player);
	NETuint32_t(&psStruct->id);
	NETuint32_t(&psObj->id);	// Target
	NETuint8_t(&psObj->player);	// Target player

	return NETend();
}

// recv lassat info on the receiving end.
bool recvLasSat(NETQUEUE queue)
{
	BASE_OBJECT	*psObj;
	UBYTE		player, targetplayer;
	STRUCTURE	*psStruct;
	uint32_t	id, targetid;

	{
		auto r = NETbeginDecode(queue, GAME_LASSAT);
		r.NETuint8_t(&player);
		r.NETuint32_t(&id);
		r.NETuint32_t(&targetid);
		r.NETuint8_t(&targetplayer);
	}

	psStruct = IdToStruct(id, player);
	psObj	 = IdToPointer(targetid, targetplayer);
	if (psStruct && !canGiveOrdersFor(queue.index, psStruct->player))
	{
		syncDebug("Wrong player.");
		return false;
	}

	if (psStruct && psObj && isLasSat(psStruct->pStructureType))
	{
		// Lassats have just one weapon
		unsigned firePause = weaponFirePause(*psStruct->getWeaponStats(0), player);
		unsigned damLevel = PERCENT(psStruct->body, psStruct->structureBody());

		if (damLevel < HEAVY_DAMAGE_LEVEL)
		{
			firePause += firePause;
		}

		if (isHumanPlayer(player) && gameTime - psStruct->asWeaps[0].lastFired <= firePause)
		{
			/* Too soon to fire again */
			return true ^ false;  // Return value meaningless and ignored.
		}

		// Give enemy no quarter, unleash the lasat
		proj_SendProjectile(&psStruct->asWeaps[0], nullptr, player, psObj->pos, psObj, true, 0);
		psStruct->asWeaps[0].lastFired = gameTime;
		psStruct->asWeaps[0].ammo = 1; // abducting this field for keeping track of triggers

		// Play 5 second countdown message
		audio_QueueTrackPos(ID_SOUND_LAS_SAT_COUNTDOWN, psObj->pos.x, psObj->pos.y, psObj->pos.z);
	}

	return true;
}

void sendStructureInfo(STRUCTURE *psStruct, STRUCTURE_INFO structureInfo_, DROID_TEMPLATE *pT)
{
	uint8_t  player = psStruct->player;
	uint32_t structId = psStruct->id;
	uint8_t  structureInfo = structureInfo_;

	auto w = NETbeginEncode(NETgameQueue(selectedPlayer), GAME_STRUCTUREINFO);
	w.NETuint8_t(&player);
	w.NETuint32_t(&structId);
	w.NETuint8_t(&structureInfo);
	if (structureInfo_ == STRUCTUREINFO_MANUFACTURE)
	{
		int32_t droidType = pT->droidType;
		WzString name = pT->name;
		w.NETwzstring(name);
		w.NETuint32_t(&pT->multiPlayerID);
		w.NETint32_t(&droidType);
		w.NETuint8_t(&pT->asParts[COMP_BODY]);
		w.NETuint8_t(&pT->asParts[COMP_BRAIN]);
		w.NETuint8_t(&pT->asParts[COMP_PROPULSION]);
		w.NETuint8_t(&pT->asParts[COMP_REPAIRUNIT]);
		w.NETuint8_t(&pT->asParts[COMP_ECM]);
		w.NETuint8_t(&pT->asParts[COMP_SENSOR]);
		w.NETuint8_t(&pT->asParts[COMP_CONSTRUCT]);
		w.NETint8_t(&pT->numWeaps);
		for (int i = 0; i < pT->numWeaps; i++)
		{
			w.NETuint32_t(&pT->asWeaps[i]);
		}
	}
}

void recvStructureInfo(NETQUEUE queue)
{
	uint8_t         player = 0;
	uint32_t        structId = 0;
	uint8_t         structureInfo;
	STRUCTURE      *psStruct;
	DROID_TEMPLATE t, *pT = &t;
	int32_t droidType;

	{
		auto r = NETbeginDecode(queue, GAME_STRUCTUREINFO);
		r.NETuint8_t(&player);
		r.NETuint32_t(&structId);
		r.NETuint8_t(&structureInfo);
		if (structureInfo == STRUCTUREINFO_MANUFACTURE)
		{
			WzString name;
			r.NETwzstring(name);
			pT->name = name;
			r.NETuint32_t(&pT->multiPlayerID);
			r.NETint32_t(&droidType);
			r.NETuint8_t(&pT->asParts[COMP_BODY]);
			r.NETuint8_t(&pT->asParts[COMP_BRAIN]);
			r.NETuint8_t(&pT->asParts[COMP_PROPULSION]);
			r.NETuint8_t(&pT->asParts[COMP_REPAIRUNIT]);
			r.NETuint8_t(&pT->asParts[COMP_ECM]);
			r.NETuint8_t(&pT->asParts[COMP_SENSOR]);
			r.NETuint8_t(&pT->asParts[COMP_CONSTRUCT]);
			r.NETint8_t(&pT->numWeaps);
			ASSERT_OR_RETURN(, pT->numWeaps >= 0 && pT->numWeaps <= ARRAY_SIZE(pT->asWeaps), "Bad numWeaps %d", pT->numWeaps);
			for (int i = 0; i < pT->numWeaps; i++)
			{
				r.NETuint32_t(&pT->asWeaps[i]);
			}
			pT->droidType = (DROID_TYPE)droidType;
			pT = copyTemplate(player, pT);
		}
	}

	psStruct = IdToStruct(structId, player);

	syncDebug("player%d,structId%u%c,structureInfo%u", player, structId, psStruct == nullptr? '^' : '*', structureInfo);

	if (psStruct == nullptr)
	{
		debug(LOG_WARNING, "Could not find structure %u to change production for", structId);
		return;
	}
	if (!canGiveOrdersFor(queue.index, psStruct->player))
	{
		syncDebug("Wrong player.");
		return;
	}

	CHECK_STRUCTURE(psStruct);

	if (structureInfo == STRUCTUREINFO_MANUFACTURE && !researchedTemplate(pT, player, true, true))
	{
		debug(LOG_ERROR, "Invalid droid received from player %d with name %s", (int)player, pT->name.toUtf8().c_str());
		return;
	}
	if (structureInfo == STRUCTUREINFO_MANUFACTURE && !intValidTemplate(pT, nullptr, true, player))
	{
		debug(LOG_ERROR, "Illegal droid received from player %d with name %s", (int)player, pT->name.toUtf8().c_str());
		return;
	}

	if (psStruct->isFactory())
	{
		popStatusPending(psStruct->pFunctionality->factory);
	}
	else if (psStruct->pStructureType->type == REF_RESEARCH)
	{
		popStatusPending(psStruct->pFunctionality->researchFacility);
	}

	syncDebugStructure(psStruct, '<');

	switch (structureInfo)
	{
	case STRUCTUREINFO_MANUFACTURE:       structSetManufacture(psStruct, pT, ModeImmediate); break;
	case STRUCTUREINFO_CANCELPRODUCTION:  cancelProduction(psStruct, ModeImmediate, false);       break;
	case STRUCTUREINFO_HOLDPRODUCTION:    holdProduction(psStruct, ModeImmediate);                break;
	case STRUCTUREINFO_RELEASEPRODUCTION: releaseProduction(psStruct, ModeImmediate);             break;
	case STRUCTUREINFO_HOLDRESEARCH:      holdResearch(psStruct, ModeImmediate);                  break;
	case STRUCTUREINFO_RELEASERESEARCH:   releaseResearch(psStruct, ModeImmediate);               break;
	default:
		debug(LOG_ERROR, "Invalid structureInfo %d", structureInfo);
	}

	syncDebugStructure(psStruct, '>');

	CHECK_STRUCTURE(psStruct);
}
