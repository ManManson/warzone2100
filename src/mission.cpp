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
/**
 * @file mission.c
 *
 * All the stuff relevant to a mission.
 */
#include <time.h>

#include "mission.h"

#include "lib/framework/frame.h"
#include "lib/framework/wzapp.h"
#include "lib/framework/math_ext.h"
#include "lib/framework/object_list_iteration.h"
#include "lib/framework/physfs_ext.h"
#include "lib/ivis_opengl/bitimage.h"
#include "lib/ivis_opengl/pieblitfunc.h"
#include "lib/ivis_opengl/screen.h"
#include "lib/gamelib/gtime.h"
#include "lib/sound/audio.h"
#include "lib/sound/audio_id.h"
#include "lib/sound/cdaudio.h"
#include "lib/widget/label.h"
#include "lib/widget/widget.h"

#include "game.h"
#include "challenge.h"
#include "projectile.h"
#include "power.h"
#include "structure.h"
#include "message.h"
#include "research.h"
#include "hci.h"
#include "move.h"
#include "order.h"
#include "action.h"
#include "display3d.h"
#include "effects.h"
#include "radar.h"
#include "transporter.h"
#include "group.h"
#include "frontend.h"		// for displaytextoption.
#include "intdisplay.h"
#include "main.h"
#include "display.h"
#include "loadsave.h"
#include "cmddroid.h"
#include "warcam.h"
#include "wrappers.h"
#include "console.h"
#include "droid.h"
#include "data.h"
#include "multiplay.h"
#include "loop.h"
#include "visibility.h"
#include "mapgrid.h"
#include "selection.h"
#include "scores.h"
#include "texture.h"
#include "warzoneconfig.h"
#include "combat.h"
#include "qtscript.h"
#include "activity.h"
#include "lib/framework/wztime.h"
#include "keybind.h"
#include "campaigninfo.h"
#include "wzapi.h"
#include "screens/guidescreen.h"

#define		IDMISSIONRES_TXT		11004
#define		IDMISSIONRES_LOAD		11005
#define		IDMISSIONRES_CONTINUE		11008
#define		IDMISSIONRES_BACKFORM		11013
#define		IDMISSIONRES_TITLE		11014

/* Mission timer label position */
#define		TIMER_LABELX			15
#define		TIMER_LABELY			0

/* Transporter Timer form position */
#define		TRAN_FORM_X			STAT_X
#define		TRAN_FORM_Y			TIMER_Y

/* Transporter Timer position */
#define		TRAN_TIMER_X			4
#define		TRAN_TIMER_Y			TIMER_LABELY
#define		TRAN_TIMER_WIDTH		25

#define		MISSION_1_X			5	// pos of text options in box.
#define		MISSION_1_Y			15
#define		MISSION_2_X			5
#define		MISSION_2_Y			35
#define		MISSION_3_X			5
#define		MISSION_3_Y			55

#define		MISSION_TEXT_W			MISSIONRES_W-10
#define		MISSION_TEXT_H			16

// These are used for mission countdown
#define TEN_MINUTES         (10 * 60 * GAME_TICKS_PER_SEC)
#define FIVE_MINUTES        (5 * 60 * GAME_TICKS_PER_SEC)
#define FOUR_MINUTES        (4 * 60 * GAME_TICKS_PER_SEC)
#define THREE_MINUTES       (3 * 60 * GAME_TICKS_PER_SEC)
#define TWO_MINUTES         (2 * 60 * GAME_TICKS_PER_SEC)
#define ONE_MINUTE          (1 * 60 * GAME_TICKS_PER_SEC)
#define NOT_PLAYED_ONE          0x01
#define NOT_PLAYED_TWO          0x02
#define NOT_PLAYED_THREE        0x04
#define NOT_PLAYED_FIVE         0x08
#define NOT_PLAYED_TEN          0x10
#define NOT_PLAYED_ACTIVATED    0x20

MISSION		mission;

bool		offWorldKeepLists;

/*lists of droids that are held separate over several missions. There should
only be selectedPlayer's droids but have possibility for MAX_PLAYERS -
also saves writing out list functions to cater for just one player*/
PerPlayerDroidLists apsLimboDroids;

//Where the Transporter lands for player 0 (sLandingZone[0]), and the rest are
//a list of areas that cannot be built on, used for landing the enemy transporters
static LANDING_ZONE		sLandingZone[MAX_NOGO_AREAS];

//flag to indicate when the droids in a Transporter are flown to safety and not the next mission
static bool             bDroidsToSafety;

static UBYTE   missionCountDown;
//flag to indicate whether the coded mission countdown is played
static UBYTE   bPlayCountDown;

//FUNCTIONS**************
static void addLandingLights(UDWORD x, UDWORD y);
static void resetHomeStructureObjects();
static bool startMissionOffClear(const GameLoadDetails& gameToLoad);
static bool startMissionOffKeep(const GameLoadDetails& gameToLoad);
static bool startMissionCampaignStart(const GameLoadDetails& gameToLoad);
static bool startMissionCampaignChange(const GameLoadDetails& gameToLoad);
static bool startMissionCampaignExpand(const GameLoadDetails& gameToLoad);
static bool startMissionCampaignExpandLimbo(const GameLoadDetails& gameToLoad);
static bool startMissionBetween();
static void endMissionCamChange();
static void endMissionOffClear();
static void endMissionOffKeep();
static void endMissionOffKeepLimbo();
static void endMissionExpandLimbo();

static void saveMissionData();
static void restoreMissionData();
static void saveCampaignData();
static void missionResetDroids();
static void saveMissionLimboData();
static void restoreMissionLimboData();
static void processMissionLimbo();

static void intUpdateMissionTimer(WIDGET *psWidget, const W_CONTEXT *psContext);
static bool intAddMissionTimer();
static void intUpdateTransporterTimer(WIDGET *psWidget, const W_CONTEXT *psContext);
static void adjustMissionPower();
static void saveMissionPower();
static UDWORD getHomeLandingX();
static UDWORD getHomeLandingY();
static void processPreviousCampDroids();
static bool intAddTransporterTimer();
static void clearCampaignUnits();
static void emptyTransporters(bool bOffWorld);

bool MissionResUp	= false;

static SDWORD		g_iReinforceTime = 0;


//Remove soon-to-be illegal references to objects for some structures before going offWorld.
static void resetHomeStructureObjects()
{
	for (unsigned int i = 0; i < MAX_PLAYERS; ++i)
	{
		for (STRUCTURE *psStruct : apsStructLists[i])
		{
			if (!psStruct->pFunctionality || !psStruct->pStructureType)
			{
				continue;
			}
			if (psStruct->pStructureType->type == REF_REPAIR_FACILITY)
			{
				REPAIR_FACILITY *psRepairFac = &psStruct->pFunctionality->repairFacility;
				if (psRepairFac->psObj)
				{
					psRepairFac->psObj = nullptr;
					psRepairFac->state = RepairState::Idle;
				}
			}
			else if (psStruct->pStructureType->type == REF_REARM_PAD)
			{
				REARM_PAD *psReArmPad = &psStruct->pFunctionality->rearmPad;
				if (psReArmPad->psObj)
				{
					psReArmPad->psObj = nullptr;
				}
			}
		}
	}
}

//returns true if on an off world mission
bool missionIsOffworld()
{
	return ((mission.type == LEVEL_TYPE::LDS_MKEEP)
	        || (mission.type == LEVEL_TYPE::LDS_MCLEAR)
	        || (mission.type == LEVEL_TYPE::LDS_MKEEP_LIMBO)
	       );
}

//returns true if the correct type of mission for reinforcements
bool missionForReInforcements()
{
	if (mission.type == LEVEL_TYPE::LDS_CAMSTART || missionIsOffworld() || mission.type == LEVEL_TYPE::LDS_CAMCHANGE)
	{
		return true;
	}
	else
	{
		return false;
	}
}

//returns true if the correct type of mission and a reinforcement time has been set
bool missionCanReEnforce()
{
	if (mission.ETA >= 0)
	{
		if (missionForReInforcements())
		{
			return true;
		}
	}
	return false;
}

//returns true if the mission is a Limbo Expand mission
bool missionLimboExpand()
{
	return (mission.type == LEVEL_TYPE::LDS_EXPAND_LIMBO);
}

// mission initialisation game code
void initMission()
{
	debug(LOG_SAVE, "*** Init Mission ***");
	mission.type = LEVEL_TYPE::LDS_NONE;
	for (int inc = 0; inc < MAX_PLAYERS; inc++)
	{
		mission.apsStructLists[inc].clear();
		mission.apsDroidLists[inc].clear();
		mission.apsFeatureLists[inc].clear();
		mission.apsFlagPosLists[inc].clear();
		mission.apsExtractorLists[inc].clear();
		apsLimboDroids[inc].clear();
	}
	mission.apsSensorList[0].clear();
	mission.apsOilList[0].clear();
	offWorldKeepLists = false;
	mission.time = -1;
	setMissionCountDown();

	mission.ETA = -1;
	mission.startTime = 0;
	mission.psGateways.clear(); // just in case
	mission.mapHeight = 0;
	mission.mapWidth = 0;
	for (auto &i : mission.psBlockMap)
	{
		i.reset();
	}
	for (auto &i : mission.psAuxMap)
	{
		i.reset();
	}

	//init all the landing zones
	initNoGoAreas();

	bDroidsToSafety = false;
	setPlayCountDown(true);

	//start as not cheating!
	mission.cheatTime = 0;
}

//this is called everytime the game is quit
void releaseMission()
{
	/* mission.apsDroidLists may contain some droids that have been transferred from one campaign to the next */
	freeAllMissionDroids();

	/* apsLimboDroids may contain some droids that have been saved at the end of one mission and not yet used */
	freeAllLimboDroids();
}

//called to shut down when mid-mission on an offWorld map
bool missionShutDown()
{
	debug(LOG_SAVE, "called, mission is %s", missionIsOffworld() ? "off-world" : "main map");
	if (missionIsOffworld())
	{
		//clear out the audio
		audio_StopAll();

		freeAllDroids();
		freeAllStructs();
		freeAllFeatures();
		freeAllFlagPositions();
		releaseAllProxDisp();
		gwShutDown();

		for (int inc = 0; inc < MAX_PLAYERS; inc++)
		{
			apsDroidLists[inc] = std::move(mission.apsDroidLists[inc]);
			mission.apsDroidLists[inc].clear();
			apsStructLists[inc] = std::move(mission.apsStructLists[inc]);
			mission.apsStructLists[inc].clear();
			apsFeatureLists[inc] = std::move(mission.apsFeatureLists[inc]);
			mission.apsFeatureLists[inc].clear();
			apsFlagPosLists[inc] = std::move(mission.apsFlagPosLists[inc]);
			mission.apsFlagPosLists[inc].clear();
			apsExtractorLists[inc] = std::move(mission.apsExtractorLists[inc]);
			mission.apsExtractorLists[inc].clear();
		}
		apsSensorList[0] = std::move(mission.apsSensorList[0]);
		apsOilList[0] = std::move(mission.apsOilList[0]);
		mission.apsSensorList[0].clear();
		mission.apsOilList[0].clear();

		psMapTiles = std::move(mission.psMapTiles);
		mapWidth = mission.mapWidth;
		mapHeight = mission.mapHeight;
		for (int i = 0; i < ARRAY_SIZE(mission.psBlockMap); ++i)
		{
			psBlockMap[i] = std::move(mission.psBlockMap[i]);
		}
		for (int i = 0; i < ARRAY_SIZE(mission.psAuxMap); ++i)
		{
			psAuxMap[i] = std::move(mission.psAuxMap[i]);
		}
		std::swap(mission.psGateways, gwGetGateways());
	}
	keybindShutdown();
	// sorry if this breaks something - but it looks like it's what should happen - John
	mission.type = LEVEL_TYPE::LDS_NONE;

	return true;
}


/*on the PC - sets the countdown played flag*/
void setMissionCountDown()
{
	SDWORD		timeRemaining;

	timeRemaining = mission.time - (gameTime - mission.startTime);
	if (timeRemaining < 0)
	{
		timeRemaining = 0;
	}

	// Need to init the countdown played each time the mission time is changed
	missionCountDown = NOT_PLAYED_ONE | NOT_PLAYED_TWO | NOT_PLAYED_THREE | NOT_PLAYED_FIVE | NOT_PLAYED_TEN | NOT_PLAYED_ACTIVATED;

	if (timeRemaining < TEN_MINUTES - 1)
	{
		missionCountDown &= ~NOT_PLAYED_TEN;
	}
	if (timeRemaining < FIVE_MINUTES - 1)
	{
		missionCountDown &= ~NOT_PLAYED_FIVE;
	}
	if (timeRemaining < THREE_MINUTES - 1)
	{
		missionCountDown &= ~NOT_PLAYED_THREE;
	}
	if (timeRemaining < TWO_MINUTES - 1)
	{
		missionCountDown &= ~NOT_PLAYED_TWO;
	}
	if (timeRemaining < ONE_MINUTE - 1)
	{
		missionCountDown &= ~NOT_PLAYED_ONE;
	}
}


bool startMission(LEVEL_TYPE missionType, const GameLoadDetails& gameDetails)
{
	bool	loaded = true;

	debug(LOG_SAVE, "type %d", (int)missionType);

	/* Player has (obviously) not failed at the start */
	setPlayerHasLost(false);
	setPlayerHasWon(false);

	/* and win/lose video is obviously not playing */
	setScriptWinLoseVideo(PLAY_NONE);

	// this inits the flag so that 'reinforcements have arrived' message isn't played for the first transporter load
	initFirstTransporterFlag();

	if (mission.type != LEVEL_TYPE::LDS_NONE)
	{
		/*mission type gets set to none when you have returned from a mission
		so don't want to go another mission when already on one! - so ignore*/
		debug(LOG_SAVE, "Already on a mission");
		return true;
	}

	initEffectsSystem();

	//load the game file for all types of mission except a Between Mission
	if (missionType != LEVEL_TYPE::LDS_BETWEEN)
	{
		loadGameInit(gameDetails);
	}

	//all proximity messages are removed between missions now
	releaseAllProxDisp();

	switch (missionType)
	{
	case LEVEL_TYPE::LDS_CAMSTART:
		if (!startMissionCampaignStart(gameDetails))
		{
			loaded = false;
		}
		break;
	case LEVEL_TYPE::LDS_MKEEP:
	case LEVEL_TYPE::LDS_MKEEP_LIMBO:
		if (!startMissionOffKeep(gameDetails))
		{
			loaded = false;
		}
		break;
	case LEVEL_TYPE::LDS_BETWEEN:
		//do anything?
		if (!startMissionBetween())
		{
			loaded = false;
		}
		break;
	case LEVEL_TYPE::LDS_CAMCHANGE:
		if (!startMissionCampaignChange(gameDetails))
		{
			loaded = false;
		}
		break;
	case LEVEL_TYPE::LDS_EXPAND:
		if (!startMissionCampaignExpand(gameDetails))
		{
			loaded = false;
		}
		break;
	case LEVEL_TYPE::LDS_EXPAND_LIMBO:
		if (!startMissionCampaignExpandLimbo(gameDetails))
		{
			loaded = false;
		}
		break;
	case LEVEL_TYPE::LDS_MCLEAR:
		if (!startMissionOffClear(gameDetails))
		{
			loaded = false;
		}
		break;
	default:
		//error!
		debug(LOG_ERROR, "Unknown Mission Type");

		loaded = false;
	}

	if (!loaded)
	{
		debug(LOG_ERROR, "Failed to start mission, missiontype = %d, game, %s", (int)missionType, gameDetails.filePath.c_str());
		return false;
	}

	mission.type = missionType;

	if (missionIsOffworld())
	{
		//add what power have got from the home base
		adjustMissionPower();
	}

	if (missionCanReEnforce())
	{
		//add mission timer - if necessary
		addMissionTimerInterface();

		//add Transporter Timer
		addTransporterTimerInterface();
	}

	scoreInitSystem();

	return true;
}


// initialise the mission stuff for a save game
bool startMissionSave(LEVEL_TYPE missionType)
{
	mission.type = missionType;

	return true;
}


/*checks the time has been set and then adds the timer if not already on
the display*/
void addMissionTimerInterface()
{
	//don't add if the timer hasn't been set
	if (mission.time < 0 && !challengeActive)
	{
		return;
	}

	//check timer is not already on the screen
	if (!widgGetFromID(psWScreen, IDTIMER_FORM))
	{
		intAddMissionTimer();
	}
}

/*checks that the timer has been set and that a Transporter exists before
adding the timer button*/
void addTransporterTimerInterface()
{
	DROID           *psTransporter = nullptr;
	bool            bAddInterface = false;
	W_CLICKFORM     *psForm;

	//check if reinforcements are allowed
	if (mission.ETA >= 0 && selectedPlayer < MAX_PLAYERS)
	{
		//check the player has at least one Transporter back at base
		for (DROID *psDroid : mission.apsDroidLists[selectedPlayer])
		{
			if (psDroid->isTransporter())
			{
				psTransporter = psDroid;
				break;
			}
		}
		if (psTransporter)
		{
			bAddInterface = true;

			// Check that neither the timer nor the launch button are on screen
			if (!widgGetFromID(psWScreen, IDTRANTIMER_BUTTON) && !widgGetFromID(psWScreen, IDTRANS_LAUNCH))
			{
				intAddTransporterTimer();
			}

			//set the data for the transporter timer
			widgSetUserData(psWScreen, IDTRANTIMER_DISPLAY, (void *)psTransporter);

			// lock the button if necessary
			if (transporterFlying(psTransporter))
			{
				// disable the form so can't add any more droids into the transporter
				psForm = (W_CLICKFORM *)widgGetFromID(psWScreen, IDTRANTIMER_BUTTON);
				if (psForm)
				{
					psForm->setState(WBUT_LOCK);
				}
			}
		}
	}
	// if criteria not met
	if (!bAddInterface)
	{
		// make sure its not there!
		intRemoveTransporterTimer();
	}
}

#define OFFSCREEN_HEIGHT	600
#define	EDGE_SIZE	1

/* fly in transporters at start of level */
void missionFlyTransportersIn(SDWORD iPlayer, bool bTrackTransporter)
{
	UWORD	iX, iY, iZ;
	SDWORD	iLandX, iLandY;

	ASSERT_OR_RETURN(, iPlayer < MAX_PLAYERS, "Flying nonexistent player %d's transporters in", iPlayer);

	iLandX = getLandingX(iPlayer);
	iLandY = getLandingY(iPlayer);
	missionGetTransporterEntry(iPlayer, &iX, &iY);
	iZ = (UWORD)(map_Height(iX, iY) + OFFSCREEN_HEIGHT);

	//get the droids for the mission
	mutating_list_iterate(mission.apsDroidLists[iPlayer], [iPlayer, bTrackTransporter, iX, iY, iZ, iLandX, iLandY](DROID* psTransporter)
	{
		SDWORD iDx, iDy;

		if (psTransporter->droidType == DROID_SUPERTRANSPORTER)
		{
			// Check that this transporter actually contains some droids
			if (psTransporter->psGroup && psTransporter->psGroup->refCount > 1)
			{
				// Remove map information from previous map
				psTransporter->watchedTiles.clear();

				// Remove out of stored list and add to current Droid list
				if (droidRemove(psTransporter, mission.apsDroidLists))
				{
					// Do not want to add it unless managed to remove it from the previous list
					addDroid(psTransporter, apsDroidLists);
				}

				/* set start position */
				psTransporter->pos.x = iX;
				psTransporter->pos.y = iY;
				psTransporter->pos.z = iZ;

				/* set start direction */
				iDx = iLandX - iX;
				iDy = iLandY - iY;

				psTransporter->rot.direction = iAtan2(iDx, iDy);

				// Camera track requested and it's the selected player.
				if ((bTrackTransporter == true) && (iPlayer == (SDWORD)selectedPlayer))
				{
					/* deselect all droids */
					selDroidDeselect(selectedPlayer);

					if (getWarCamStatus())
					{
						camToggleStatus();
					}

					/* select transporter */
					psTransporter->selected = true;
					camToggleStatus();
				}

				//little hack to ensure all Transporters are fully repaired by time enter world
				psTransporter->body = psTransporter->originalBody;

				/* set fly-in order */
				orderDroidLoc(psTransporter, DORDER_TRANSPORTIN, iLandX, iLandY, ModeImmediate);

				audio_PlayObjDynamicTrack(psTransporter, ID_SOUND_BLIMP_FLIGHT, moveCheckDroidMovingAndVisible);

				//only want to fly one transporter in at a time now - AB 14/01/99
				return IterationResult::BREAK_ITERATION;
			}
		}
		return IterationResult::CONTINUE_ITERATION;
	});
}

/* Saves the necessary data when moving from a home base Mission to an OffWorld mission */
static void saveMissionData()
{
	bool bRepairExists = false;
	bool bRearmPadExists = false;

	debug(LOG_SAVE, "called");

	ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " exceeds MAX_PLAYERS", selectedPlayer);

	//clear out the audio
	audio_StopAll();

	//set any structures currently being built to completed for the selected player
	mutating_list_iterate(apsStructLists[selectedPlayer], [&bRepairExists, &bRearmPadExists](STRUCTURE* psStruct)
	{
		STRUCTURE* psStructBeingBuilt;
		if (psStruct->status == SS_BEING_BUILT)
		{
			//find a droid working on it
			for (const DROID* psDroid : apsDroidLists[selectedPlayer])
			{
				if ((psStructBeingBuilt = (STRUCTURE*)orderStateObj(psDroid, DORDER_BUILD))
					&& psStructBeingBuilt == psStruct)
				{
					// just give it all its build points
					structureBuild(psStruct, nullptr, structureBuildPointsToCompletion(*psStruct));
					//don't bother looking for any other droids working on it
					break;
				}
			}
		}
		//check if have a completed repair facility or rearming pad on home world
		if (psStruct->status == SS_BUILT && psStruct->pStructureType)
		{
			if (psStruct->pStructureType->type == REF_REPAIR_FACILITY)
			{
				bRepairExists = true;
			}
			else if (psStruct->pStructureType->type == REF_REARM_PAD)
			{
				bRearmPadExists = true;
			}
		}
		return IterationResult::CONTINUE_ITERATION;
	});

	//repair and rearm all droids back at home base if have a repair facility or rearming pad
	if (bRepairExists || bRearmPadExists)
	{
		for (DROID* psDroid : apsDroidLists[selectedPlayer])
		{
			bool vtolAndPadsExist = (psDroid->isVtol() && bRearmPadExists);
			if ((bRepairExists || vtolAndPadsExist) && psDroid->isDamaged())
			{
				psDroid->body = psDroid->originalBody;
			}
			if (vtolAndPadsExist)
			{
				fillVtolDroid(psDroid);
			}
		}
	}

	//clear droid orders for all droids except constructors still building
	for (DROID* psDroid : apsDroidLists[selectedPlayer])
	{
		STRUCTURE* psStructBeingBuilt;
		if ((psStructBeingBuilt = (STRUCTURE *)orderStateObj(psDroid, DORDER_BUILD)))
		{
			if (psStructBeingBuilt->status == SS_BUILT)
			{
				orderDroid(psDroid, DORDER_STOP, ModeImmediate);
			}
		}
		else
		{
			orderDroid(psDroid, DORDER_STOP, ModeImmediate);
		}
		resetObjectAnimationState(psDroid);
	}

	resetHomeStructureObjects(); //get rid of soon-to-be illegal references of droids in repair facilities and rearming pads.

	//save the mission data
	mission.psMapTiles = std::move(psMapTiles);
	mission.mapWidth = mapWidth;
	mission.mapHeight = mapHeight;
	for (int i = 0; i < ARRAY_SIZE(mission.psBlockMap); ++i)
	{
		mission.psBlockMap[i] = std::move(psBlockMap[i]);
	}
	for (int i = 0; i < ARRAY_SIZE(mission.psAuxMap); ++i)
	{
		mission.psAuxMap[i] = std::move(psAuxMap[i]);
	}
	mission.scrollMinX = scrollMinX;
	mission.scrollMinY = scrollMinY;
	mission.scrollMaxX = scrollMaxX;
	mission.scrollMaxY = scrollMaxY;
	std::swap(mission.psGateways, gwGetGateways());
	// save the selectedPlayer's LZ
	mission.homeLZ_X = getLandingX(selectedPlayer);
	mission.homeLZ_Y = getLandingY(selectedPlayer);

	for (unsigned int inc = 0; inc < MAX_PLAYERS; ++inc)
	{
		mission.apsStructLists[inc] = apsStructLists[inc];
		mission.apsDroidLists[inc] = apsDroidLists[inc];
		mission.apsFeatureLists[inc] = apsFeatureLists[inc];
		mission.apsFlagPosLists[inc] = apsFlagPosLists[inc];
		mission.apsExtractorLists[inc] = apsExtractorLists[inc];
	}
	mission.apsSensorList[0] = apsSensorList[0];
	mission.apsOilList[0] = apsOilList[0];

	mission.playerX = playerPos.p.x;
	mission.playerY = playerPos.p.z;

	//save the power settings
	saveMissionPower();

	//init before loading in the new game
	initFactoryNumFlag();

	//clear all the effects from the map
	initEffectsSystem();

	resizeRadar();
}

/*
	This routine frees the memory for the offworld mission map (in the call to mapShutdown)

	- so when this routine is called we must still be set to the offworld map data
	i.e. We shoudn't have called SwapMissionPointers()

*/
void restoreMissionData()
{
	UDWORD		inc;

	debug(LOG_SAVE, "called");

	//clear out the audio
	audio_StopAll();

	//clear all the lists
	proj_FreeAllProjectiles();
	freeAllDroids();
	freeAllStructs();
	freeAllFeatures();
	freeAllFlagPositions();
	gwShutDown();
	if (game.type != LEVEL_TYPE::CAMPAIGN)
	{
		ASSERT(false, "game type isn't campaign, but we are in a campaign game!");
		game.type = LEVEL_TYPE::CAMPAIGN;	// fix the issue, since it is obviously a bug
	}
	//restore the game pointers
	for (inc = 0; inc < MAX_PLAYERS; inc++)
	{
		apsDroidLists[inc] = std::move(mission.apsDroidLists[inc]);
		mission.apsDroidLists[inc].clear();
		for (DROID* psObj : apsDroidLists[inc])
		{
			psObj->died = false;	//make sure the died flag is not set
		}

		apsStructLists[inc] = std::move(mission.apsStructLists[inc]);
		mission.apsStructLists[inc].clear();

		apsFeatureLists[inc] = std::move(mission.apsFeatureLists[inc]);
		mission.apsFeatureLists[inc].clear();

		apsFlagPosLists[inc] = std::move(mission.apsFlagPosLists[inc]);
		mission.apsFlagPosLists[inc].clear();

		apsExtractorLists[inc] = std::move(mission.apsExtractorLists[inc]);
		mission.apsExtractorLists[inc].clear();
	}
	apsSensorList[0] = std::move(mission.apsSensorList[0]);
	apsOilList[0] = std::move(mission.apsOilList[0]);
	mission.apsSensorList[0].clear();
	apsOilList[0].clear();
	//swap mission data over

	psMapTiles = std::move(mission.psMapTiles);

	mapWidth = mission.mapWidth;
	mapHeight = mission.mapHeight;
	for (int i = 0; i < ARRAY_SIZE(mission.psBlockMap); ++i)
	{
		psBlockMap[i] = std::move(mission.psBlockMap[i]);
	}
	for (int i = 0; i < ARRAY_SIZE(mission.psAuxMap); ++i)
	{
		psAuxMap[i] = std::move(mission.psAuxMap[i]);
	}
	scrollMinX = mission.scrollMinX;
	scrollMinY = mission.scrollMinY;
	scrollMaxX = mission.scrollMaxX;
	scrollMaxY = mission.scrollMaxY;
	std::swap(mission.psGateways, gwGetGateways());
	//and clear the mission pointers
	mission.psMapTiles	= nullptr;
	mission.mapWidth	= 0;
	mission.mapHeight	= 0;
	mission.scrollMinX	= 0;
	mission.scrollMinY	= 0;
	mission.scrollMaxX	= 0;
	mission.scrollMaxY	= 0;
	mission.psGateways.clear();

	//reset the current structure lists
	setCurrentStructQuantity(false);

	initFactoryNumFlag();
	resetFactoryNumFlag();

	offWorldKeepLists = false;

	resizeRadar();
}

/*Saves the necessary data when moving from one mission to a limbo expand Mission*/
void saveMissionLimboData()
{
	debug(LOG_SAVE, "called");

	ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " exceeds MAX_PLAYERS", selectedPlayer);

	//clear out the audio
	audio_StopAll();

	// before copy the pointers over check selectedPlayer's mission.droids since
	// there might be some from the previous camapign
	processPreviousCampDroids();

	// move droids properly - does all the clean up code
	mutating_list_iterate(apsDroidLists[selectedPlayer], [](DROID* psDroid)
	{
		if (droidRemove(psDroid, apsDroidLists))
		{
			addDroid(psDroid, mission.apsDroidLists);
		}
		return IterationResult::CONTINUE_ITERATION;
	});
	apsDroidLists[selectedPlayer].clear();

	// any selectedPlayer's factories/research need to be put on holdProduction/holdresearch
	for (STRUCTURE* psStruct : apsStructLists[selectedPlayer])
	{
		if (!psStruct)
		{
			continue;
		}
		if (psStruct->isFactory())
		{
			holdProduction(psStruct, ModeImmediate);
		}
		else if (psStruct->pStructureType->type == REF_RESEARCH)
		{
			holdResearch(psStruct, ModeImmediate);
		}
	}
}

//this is called via a script function to place the Limbo droids once the mission has started
void placeLimboDroids()
{
	debug(LOG_SAVE, "called");

	ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " exceeds MAX_PLAYERS", selectedPlayer);

	// Copy the droids across for the selected Player
	mutating_list_iterate(apsLimboDroids[selectedPlayer], [](DROID* psDroid)
	{
		UDWORD			droidX, droidY;
		PICKTILE		pickRes;

		if (droidRemove(psDroid, apsLimboDroids))
		{
			addDroid(psDroid, apsDroidLists);
			//KILL OFF TRANSPORTER - should never be one but....
			if (psDroid->isTransporter())
			{
				vanishDroid(psDroid);
				return IterationResult::CONTINUE_ITERATION;
			}
			//set up location for each of the droids
			droidX = map_coord(getLandingX(LIMBO_LANDING));
			droidY = map_coord(getLandingY(LIMBO_LANDING));
			pickRes = pickHalfATile(&droidX, &droidY, LOOK_FOR_EMPTY_TILE);
			if (pickRes == NO_FREE_TILE)
			{
				ASSERT(false, "placeLimboUnits: Unable to find a free location");
			}
			psDroid->pos.x = (UWORD)world_coord(droidX);
			psDroid->pos.y = (UWORD)world_coord(droidY);
			ASSERT(worldOnMap(psDroid->pos.x, psDroid->pos.y), "limbo droid is not on the map");
			psDroid->pos.z = map_Height(psDroid->pos.x, psDroid->pos.y);
			updateDroidOrientation(psDroid);
			psDroid->selected = false;
			//this is mainly for VTOLs
			setDroidBase(psDroid, nullptr);
			//initialise the movement data
			initDroidMovement(psDroid);
			//make sure the died flag is not set
			psDroid->died = false;
			//update visibility
			visTilesUpdate(psDroid);
		}
		else
		{
			ASSERT(false, "placeLimboUnits: Unable to remove unit from Limbo list");
		}
		return IterationResult::CONTINUE_ITERATION;
	});
}

/*restores the necessary data on completion of a Limbo Expand mission*/
void restoreMissionLimboData()
{
	debug(LOG_SAVE, "called");

	ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " exceeds MAX_PLAYERS", selectedPlayer);

	/*the droids stored in the mission droid list need to be added back
	into the current droid list*/
	mutating_list_iterate(mission.apsDroidLists[selectedPlayer], [](DROID* psDroid)
	{
		//remove out of stored list and add to current Droid list
		if (droidRemove(psDroid, mission.apsDroidLists))
		{
			addDroid(psDroid, apsDroidLists);
			//reset droid orders
			orderDroid(psDroid, DORDER_STOP, ModeImmediate);
			//the location of the droid should be valid!
			if (psDroid->pos.x != INVALID_XY && psDroid->pos.y != INVALID_XY)
			{
				visTilesUpdate(psDroid); //update visibility
			}
		}
		return IterationResult::CONTINUE_ITERATION;
	});
	ASSERT(mission.apsDroidLists[selectedPlayer].empty(), "list should be empty");
}

/*Saves the necessary data when moving from one campaign to the start of the
next - saves out the list of droids for the selected player*/
void saveCampaignData()
{
	debug(LOG_SAVE, "called");

	ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " exceeds MAX_PLAYERS", selectedPlayer);

	// If the droids have been moved to safety then get any Transporters that exist
	if (getDroidsToSafetyFlag())
	{
		// Move any Transporters into the mission list
		mutating_list_iterate(apsDroidLists[selectedPlayer], [](DROID* psDroid)
		{
			if (psDroid->isTransporter())
			{
				// Empty the transporter into the mission list
				ASSERT_OR_RETURN(IterationResult::CONTINUE_ITERATION, psDroid->psGroup != nullptr, "Transporter does not have a group");

				mutating_list_iterate(psDroid->psGroup->psList, [psDroid](DROID* psCurr)
				{
					if (psCurr == psDroid)
					{
						return IterationResult::BREAK_ITERATION;
					}
					// Remove it from the transporter group
					psDroid->psGroup->remove(psCurr);
					// Cam change add droid
					psCurr->pos.x = INVALID_XY;
					psCurr->pos.y = INVALID_XY;
					// Add it back into current droid lists
					addDroid(psCurr, mission.apsDroidLists);

					return IterationResult::CONTINUE_ITERATION;
				});
				// Remove the transporter from the current list
				if (droidRemove(psDroid, apsDroidLists))
				{
					//cam change add droid
					psDroid->pos.x = INVALID_XY;
					psDroid->pos.y = INVALID_XY;
					addDroid(psDroid, mission.apsDroidLists);
				}
			}
			return IterationResult::CONTINUE_ITERATION;
		});
	}
	else
	{
		// Reserve the droids for selected player for start of next campaign
		mission.apsDroidLists[selectedPlayer] = std::move(apsDroidLists[selectedPlayer]);
		apsDroidLists[selectedPlayer].clear();
		for (DROID* psDroid : mission.apsDroidLists[selectedPlayer])
		{
			//cam change add droid
			psDroid->pos.x = INVALID_XY;
			psDroid->pos.y = INVALID_XY;
		}
	}

	//if the droids have been moved to safety then get any Transporters that exist
	if (getDroidsToSafetyFlag())
	{
		/*now that every unit for the selected player has been moved into the
		mission list - reverse it and fill the transporter with the first ten units*/
		mission.apsDroidLists[selectedPlayer].reverse();

		//find the *first* transporter
		mutating_list_iterate(mission.apsDroidLists[selectedPlayer], [](DROID* psDroid)
		{
			if (psDroid->isTransporter())
			{
				//fill it with droids from the mission list
				mutating_list_iterate(mission.apsDroidLists[selectedPlayer], [psDroid](DROID* psSafeDroid)
				{
					if (psSafeDroid != psDroid)
					{
						//add to the Transporter, checking for when full
						if (checkTransporterSpace(psDroid, psSafeDroid))
						{
							if (droidRemove(psSafeDroid, mission.apsDroidLists))
							{
								psDroid->psGroup->add(psSafeDroid);
							}
						}
						else
						{
							return IterationResult::BREAK_ITERATION;
						}
					}
					return IterationResult::CONTINUE_ITERATION;
				});
				//only want to fill one transporter
				return IterationResult::BREAK_ITERATION;
			}
			return IterationResult::CONTINUE_ITERATION;
		});
	}

	//clear all other droids
	for (int inc = 0; inc < MAX_PLAYERS; inc++)
	{
		mutating_list_iterate(apsDroidLists[inc], [](DROID* d)
		{
			vanishDroid(d);
			return IterationResult::CONTINUE_ITERATION;
		});
	}

	//clear out the audio
	audio_StopAll();

	//clear all other memory
	freeAllStructs();
	freeAllFeatures();
}


//start an off world mission - clearing the object lists
bool startMissionOffClear(const GameLoadDetails& gameToLoad)
{
	debug(LOG_SAVE, "called for %s", gameToLoad.filePath.c_str());

	saveMissionData();

	//load in the new game clearing the lists
	if (!loadGame(gameToLoad, !KEEPOBJECTS, !FREEMEM))
	{
		return false;
	}

	offWorldKeepLists = false;

	// The message should have been played at the between stage
	missionCountDown &= ~NOT_PLAYED_ACTIVATED;

	return true;
}

//start an off world mission - keeping the object lists
bool startMissionOffKeep(const GameLoadDetails& gameToLoad)
{
	debug(LOG_SAVE, "called for %s", gameToLoad.filePath.c_str());
	saveMissionData();

	//load in the new game clearing the lists
	if (!loadGame(gameToLoad, !KEEPOBJECTS, !FREEMEM))
	{
		return false;
	}

	offWorldKeepLists = true;

	// The message should have been played at the between stage
	missionCountDown &= ~NOT_PLAYED_ACTIVATED;

	return true;
}

bool startMissionCampaignStart(const GameLoadDetails& gameToLoad)
{
	debug(LOG_SAVE, "called for %s", gameToLoad.filePath.c_str());

	// Clear out all intelligence screen messages
	freeMessages();

	// Check no units left with any settings that are invalid
	clearCampaignUnits();

	// Load in the new game details
	if (!loadGame(gameToLoad, !KEEPOBJECTS, FREEMEM))
	{
		return false;
	}

	offWorldKeepLists = false;

	return true;
}

bool startMissionCampaignChange(const GameLoadDetails& gameToLoad)
{
	// Clear out all intelligence screen messages
	freeMessages();

	// Cancel any research active in a lab
	CancelAllResearch(selectedPlayer);

	// Check no units left with any settings that are invalid
	clearCampaignUnits();

	// Clear out the production run between campaigns
	changeProductionPlayer((UBYTE)selectedPlayer);

	saveCampaignData();

	//load in the new game details
	if (!loadGame(gameToLoad, !KEEPOBJECTS, !FREEMEM))
	{
		return false;
	}

	offWorldKeepLists = false;

	return true;
}

bool startMissionCampaignExpand(const GameLoadDetails& gameToLoad)
{
	//load in the new game details
	if (!loadGame(gameToLoad, KEEPOBJECTS, !FREEMEM))
	{
		return false;
	}

	offWorldKeepLists = false;
	return true;
}

bool startMissionCampaignExpandLimbo(const GameLoadDetails& gameToLoad)
{
	saveMissionLimboData();

	//load in the new game details
	if (!loadGame(gameToLoad, KEEPOBJECTS, !FREEMEM))
	{
		return false;
	}

	offWorldKeepLists = false;

	return true;
}

static bool startMissionBetween()
{
	offWorldKeepLists = false;

	return true;
}

//check no units left with any settings that are invalid
static void clearCampaignUnits()
{
	if (selectedPlayer >= MAX_PLAYERS) { return; }

	for (DROID *psDroid : apsDroidLists[selectedPlayer])
	{
		orderDroid(psDroid, DORDER_STOP, ModeImmediate);
		setDroidBase(psDroid, nullptr);
		visRemoveVisibilityOffWorld((BASE_OBJECT *)psDroid);
		CHECK_DROID(psDroid);
	}
}

/*This deals with droids at the end of an offworld mission*/
static void processMission()
{
	ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " exceeds MAX_PLAYERS", selectedPlayer);

	//and the rest on the mission map  - for now?
	mutating_list_iterate(apsDroidLists[selectedPlayer], [](DROID* psDroid)
	{
		UDWORD			droidX, droidY;
		PICKTILE		pickRes;
		//reset order - do this to all the droids that are returning from offWorld
		orderDroid(psDroid, DORDER_STOP, ModeImmediate);
		// clean up visibility
		visRemoveVisibility((BASE_OBJECT*)psDroid);
		//remove out of stored list and add to current Droid list
		if (droidRemove(psDroid, apsDroidLists))
		{
			int	x, y;

			addDroid(psDroid, mission.apsDroidLists);
			droidX = getHomeLandingX();
			droidY = getHomeLandingY();
			// Swap the droid and map pointers
			swapMissionPointers();

			pickRes = pickHalfATile(&droidX, &droidY, LOOK_FOR_EMPTY_TILE);
			ASSERT(pickRes != NO_FREE_TILE, "processMission: Unable to find a free location");
			x = (UWORD)world_coord(droidX);
			y = (UWORD)world_coord(droidY);
			droidSetPosition(psDroid, x, y);
			ASSERT(worldOnMap(psDroid->pos.x, psDroid->pos.y), "the droid is not on the map");
			updateDroidOrientation(psDroid);
			// Swap the droid and map pointers back again
			swapMissionPointers();
			psDroid->selected = false;
			// This is mainly for VTOLs
			setDroidBase(psDroid, nullptr);
		}
		return IterationResult::CONTINUE_ITERATION;
	});
}


#define MAXLIMBODROIDS (999)

/*This deals with droids at the end of an offworld Limbo mission*/
void processMissionLimbo()
{
	UDWORD	numDroidsAddedToLimboList = 0;

	ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " exceeds MAX_PLAYERS", selectedPlayer);

	//all droids (for selectedPlayer only) are placed into the limbo list
	mutating_list_iterate(apsDroidLists[selectedPlayer], [&numDroidsAddedToLimboList](DROID* psDroid)
	{
		//KILL OFF TRANSPORTER - should never be one but....
		if (psDroid->isTransporter())
		{
			vanishDroid(psDroid);
		}
		else
		{
			if (numDroidsAddedToLimboList >= MAXLIMBODROIDS)		// any room in limbo list
			{
				vanishDroid(psDroid);
			}
			else
			{
				// Remove out of stored list and add to current Droid list
				if (droidRemove(psDroid, apsDroidLists))
				{
					// Limbo list invalidate XY
					psDroid->pos.x = INVALID_XY;
					psDroid->pos.y = INVALID_XY;
					addDroid(psDroid, apsLimboDroids);
					// This is mainly for VTOLs
					setDroidBase(psDroid, nullptr);
					orderDroid(psDroid, DORDER_STOP, ModeImmediate);
					numDroidsAddedToLimboList++;
				}
			}
		}
		return IterationResult::CONTINUE_ITERATION;
	});
}

/*switch the pointers for the map and droid lists so that droid placement
 and orientation can occur on the map they will appear on*/
// NOTE: This is one huge hack for campaign games!
// Pay special attention on what is getting swapped!
void swapMissionPointers()
{
	debug(LOG_SAVE, "called");

	std::swap(psMapTiles, mission.psMapTiles);
	std::swap(mapWidth,   mission.mapWidth);
	std::swap(mapHeight,  mission.mapHeight);
	for (int i = 0; i < ARRAY_SIZE(mission.psBlockMap); ++i)
	{
		std::swap(psBlockMap[i], mission.psBlockMap[i]);
	}
	for (int i = 0; i < ARRAY_SIZE(mission.psAuxMap); ++i)
	{
		std::swap(psAuxMap[i],   mission.psAuxMap[i]);
	}
	//swap gateway zones
	std::swap(mission.psGateways, gwGetGateways());
	std::swap(scrollMinX, mission.scrollMinX);
	std::swap(scrollMinY, mission.scrollMinY);
	std::swap(scrollMaxX, mission.scrollMaxX);
	std::swap(scrollMaxY, mission.scrollMaxY);
	for (unsigned inc = 0; inc < MAX_PLAYERS; inc++)
	{
		std::swap(apsDroidLists[inc],     mission.apsDroidLists[inc]);
		std::swap(apsStructLists[inc],    mission.apsStructLists[inc]);
		std::swap(apsFeatureLists[inc],   mission.apsFeatureLists[inc]);
		std::swap(apsFlagPosLists[inc],   mission.apsFlagPosLists[inc]);
		std::swap(apsExtractorLists[inc], mission.apsExtractorLists[inc]);
	}
	std::swap(apsSensorList[0], mission.apsSensorList[0]);
	std::swap(apsOilList[0],    mission.apsOilList[0]);
}

void endMission()
{
	if (mission.type != LEVEL_TYPE::LDS_BETWEEN)
	{
		releaseAllFlicMessages(apsMessages); //Needed to remove mission objectives from offworld missions
		releaseObjectives = true;
	}
	else
	{
		releaseObjectives = false;
	}

	if (mission.type == LEVEL_TYPE::LDS_NONE)
	{
		//can't go back any further!!
		debug(LOG_SAVE, "Already returned from mission");
		return;
	}

	switch (mission.type)
	{
	case LEVEL_TYPE::LDS_CAMSTART:
		//any transporters that are flying in need to be emptied
		emptyTransporters(false);
		//when loading in a save game mid cam2a or cam3a it is loaded as a camstart
		endMissionCamChange();
		/*
			This is called at the end of every campaign mission
		*/
		break;
	case LEVEL_TYPE::LDS_MKEEP:
		//any transporters that are flying in need to be emptied
		emptyTransporters(true);
		endMissionOffKeep();
		break;
	case LEVEL_TYPE::LDS_EXPAND:
	case LEVEL_TYPE::LDS_BETWEEN:
		/*
			This is called at the end of every campaign mission
		*/

		break;
	case LEVEL_TYPE::LDS_CAMCHANGE:
		//any transporters that are flying in need to be emptied
		emptyTransporters(false);
		endMissionCamChange();
		break;
	/* left in so can skip the mission for testing...*/
	case LEVEL_TYPE::LDS_EXPAND_LIMBO:
		//shouldn't be any transporters on this mission but...who knows?
		endMissionExpandLimbo();
		break;
	case LEVEL_TYPE::LDS_MCLEAR:
		//any transporters that are flying in need to be emptied
		emptyTransporters(true);
		endMissionOffClear();
		break;
	case LEVEL_TYPE::LDS_MKEEP_LIMBO:
		//any transporters that are flying in need to be emptied
		emptyTransporters(true);
		endMissionOffKeepLimbo();
		break;
	default:
		//error!
		debug(LOG_FATAL, "Unknown Mission Type");
		abort();
	}

	intRemoveMissionTimer();
	intRemoveTransporterTimer();

	//at end of mission always do this
	intRemoveTransporterLaunch();

	//and this...
	//make sure the cheat time is not set for the next mission
	mission.cheatTime = 0;


	//reset the bSetPlayCountDown flag
	setPlayCountDown(true);


	//mission.type = MISSION_NONE;
	mission.type = LEVEL_TYPE::LDS_NONE;

	// reset the transporters
	initTransporters();
}

void endMissionCamChange()
{
	//get any droids remaining from the previous campaign
	processPreviousCampDroids();
}

void endMissionOffClear()
{
	processMission();
	restoreMissionData();

	//reset variables in Droids
	missionResetDroids();
}

void endMissionOffKeep()
{
	processMission();
	restoreMissionData();

	//reset variables in Droids
	missionResetDroids();
}

/*In this case any droids remaining (for selectedPlayer) go into a limbo list
for use in a future mission (expand type) */
void endMissionOffKeepLimbo()
{
	// Save any droids left 'alive'
	processMissionLimbo();

	// Set the lists back to the home base lists
	restoreMissionData();

	//reset variables in Droids
	missionResetDroids();
}

//This happens MID_MISSION now! but is left here in case the scripts fail but somehow get here...?
/*The selectedPlayer's droids which were separated at the start of the
mission need to merged back into the list*/
void endMissionExpandLimbo()
{
	restoreMissionLimboData();
}


//this is called mid Limbo mission via the script
void resetLimboMission()
{
	//add the units that were moved into the mission list at the start of the mission
	restoreMissionLimboData();
	//set the mission type to plain old expand...
	mission.type = LEVEL_TYPE::LDS_EXPAND;
}

/* The update routine for all droids left back at home base
Only interested in Transporters at present*/
void missionDroidUpdate(DROID *psDroid)
{
	ASSERT_OR_RETURN(, psDroid != nullptr, "Invalid unit pointer");

	/*This is required for Transporters that are moved offWorld so the
	saveGame doesn't try to set their position in the map - especially important
	for endCam2 where there isn't a valid map!*/
	if (psDroid->isTransporter())
	{
		psDroid->pos.x = INVALID_XY;
		psDroid->pos.y = INVALID_XY;
	}

	//ignore all droids except Transporters
	if (!psDroid->isTransporter()
	    || !(orderState(psDroid, DORDER_TRANSPORTOUT)  ||
	         orderState(psDroid, DORDER_TRANSPORTIN)     ||
	         orderState(psDroid, DORDER_TRANSPORTRETURN)))
	{
		return;
	}

	// NO ai update droid

	// update the droids order
	if (!orderUpdateDroid(psDroid))
	{
		ASSERT(false, "orderUpdateDroid returned false?");
	}

	// update the action of the droid
	actionUpdateDroid(psDroid);

	//NO move update
}

// Reset variables in Droids such as order and position
static void missionResetDroids()
{
	debug(LOG_SAVE, "called");

	ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " exceeds MAX_PLAYERS", selectedPlayer);

	for (unsigned int player = 0; player < MAX_PLAYERS; player++)
	{
		mutating_list_iterate(apsDroidLists[player], [](DROID* d)
		{
			// Reset order - unless constructor droid that is mid-build
			if ((d->droidType == DROID_CONSTRUCT
				|| d->droidType == DROID_CYBORG_CONSTRUCT)
				&& orderStateObj(d, DORDER_BUILD))
			{
				// Need to set the action time to ignore the previous mission time
				d->actionStarted = gameTime;
			}
			else
			{
				orderDroid(d, DORDER_STOP, ModeImmediate);
			}

			//KILL OFF TRANSPORTER
			if (d->isTransporter())
			{
				vanishDroid(d);
			}
			else
			{
				if (d->pos.x != INVALID_XY && d->pos.y != INVALID_XY)
				{
					// update visibility
					visTilesUpdate(d);
				}
			}
			return IterationResult::CONTINUE_ITERATION;
		});
	}

	mutating_list_iterate(apsDroidLists[selectedPlayer], [](DROID* psDroid)
	{
		bool	placed = false;

		//for all droids that have never left home base
		if (psDroid->pos.x == INVALID_XY && psDroid->pos.y == INVALID_XY)
		{
			STRUCTURE* psStruct = psDroid->psBaseStruct;
			FACTORY* psFactory = nullptr;

			if (psStruct && psStruct->isFactory())
			{
				psFactory = (FACTORY*)psStruct->pFunctionality;
			}
			//find a location next to the factory
			if (psFactory)
			{
				PICKTILE	pickRes;
				UDWORD		x, y;

				// Use factory DP if one
				if (psFactory->psAssemblyPoint)
				{
					x = map_coord(psFactory->psAssemblyPoint->coords.x);
					y = map_coord(psFactory->psAssemblyPoint->coords.y);
				}
				else
				{
					x = map_coord(psStruct->pos.x);
					y = map_coord(psStruct->pos.y);
				}
				pickRes = pickHalfATile(&x, &y, LOOK_FOR_EMPTY_TILE);
				if (pickRes == NO_FREE_TILE)
				{
					ASSERT(false, "missionResetUnits: Unable to find a free location");
				}
				else
				{
					int wx = world_coord(x);
					int wy = world_coord(y);

					droidSetPosition(psDroid, wx, wy);
					placed = true;
				}
			}
			else // if couldn't find the factory - try to place near HQ instead
			{
				for (const STRUCTURE* psStructure : apsStructLists[psDroid->player])
				{
					if (psStructure->pStructureType->type == REF_HQ)
					{
						UDWORD		x = map_coord(psStructure->pos.x);
						UDWORD		y = map_coord(psStructure->pos.y);
						PICKTILE	pickRes = pickHalfATile(&x, &y, LOOK_FOR_EMPTY_TILE);

						if (pickRes == NO_FREE_TILE)
						{
							ASSERT(false, "missionResetUnits: Unable to find a free location");
						}
						else
						{
							int wx = world_coord(x);
							int wy = world_coord(y);

							droidSetPosition(psDroid, wx, wy);
							placed = true;
						}
						break;
					}
				}
			}
			if (placed)
			{
				// Do all the things in build droid that never did when it was built!
				// check the droid is a reasonable distance from the edge of the map
				if (psDroid->pos.x <= world_coord(EDGE_SIZE) ||
					psDroid->pos.y <= world_coord(EDGE_SIZE) ||
					psDroid->pos.x >= world_coord(mapWidth - EDGE_SIZE) ||
					psDroid->pos.y >= world_coord(mapHeight - EDGE_SIZE))
				{
					debug(LOG_ERROR, "missionResetUnits: unit too close to edge of map - removing");
					vanishDroid(psDroid);
					return IterationResult::CONTINUE_ITERATION;
				}

				// People always stand upright
				if (psDroid->droidType != DROID_PERSON && !psDroid->isCyborg())
				{
					updateDroidOrientation(psDroid);
				}
				// Reset the selected flag
				psDroid->selected = false;

				// update visibility
				visTilesUpdate(psDroid);
			}
			else
			{
				//can't put it down so get rid of this droid!!
				ASSERT(false, "missionResetUnits: can't place unit - cancel to continue");
				vanishDroid(psDroid);
			}
		}
		return IterationResult::CONTINUE_ITERATION;
	});
}

/*unloads the Transporter passed into the mission at the specified x/y
goingHome = true when returning from an off World mission*/
void unloadTransporter(DROID *psTransporter, UDWORD x, UDWORD y, bool goingHome)
{
	PerPlayerDroidLists* ppCurrentList;
	UDWORD		droidX, droidY;
	DROID_GROUP	*psGroup;

	ASSERT_OR_RETURN(, psTransporter != nullptr, "Invalid transporter");
	if (goingHome)
	{
		ppCurrentList = &mission.apsDroidLists;
	}
	else
	{
		ppCurrentList = &apsDroidLists;
	}

	//unload all the droids from within the current Transporter
	if (psTransporter->isTransporter())
	{
		ASSERT(psTransporter->psGroup != nullptr, "psTransporter->psGroup is null??");
		for (DROID* psDroid : psTransporter->psGroup->psList)
		{
			if (psDroid == psTransporter)
			{
				break;
			}
			//add it back into current droid lists
			addDroid(psDroid, *ppCurrentList);

			//starting point...based around the value passed in
			droidX = map_coord(x);
			droidY = map_coord(y);
			if (goingHome)
			{
				//swap the droid and map pointers
				swapMissionPointers();
			}
			if (!pickATileGen(&droidX, &droidY, LOOK_FOR_EMPTY_TILE, zonedPAT))
			{
				ASSERT(false, "unloadTransporter: Unable to find a valid location");
				return;
			}
			droidSetPosition(psDroid, world_coord(droidX), world_coord(droidY));
			updateDroidOrientation(psDroid);

			//reset droid orders
			orderDroid(psDroid, DORDER_STOP, ModeImmediate);
			psDroid->selected = false;
			if (!bMultiPlayer)
			{
				// So VTOLs don't try to rearm on another map
				setDroidBase(psDroid, nullptr);
			}
			if (goingHome)
			{
				//swap the droid and map pointers
				swapMissionPointers();
			}
		}

		/* trigger script callback detailing group about to disembark */
		transporterSetScriptCurrent(psTransporter);
		triggerEvent(TRIGGER_TRANSPORTER_LANDED, psTransporter);
		transporterSetScriptCurrent(nullptr);

		/* remove droids from transporter group if not already transferred to script group */
		mutating_list_iterate(psTransporter->psGroup->psList, [&psGroup, psTransporter](DROID* psDroid)
		{
			if (psDroid == psTransporter)
			{
				return IterationResult::BREAK_ITERATION;
			}
			// a commander needs to get it's group back
			if (psDroid->droidType == DROID_COMMAND)
			{
				psGroup = grpCreate();
				psGroup->add(psDroid);
				clearCommandDroidFactory(psDroid);
				return IterationResult::CONTINUE_ITERATION;
			}
			psTransporter->psGroup->remove(psDroid);

			return IterationResult::CONTINUE_ITERATION;
		});
	}

	// Don't do this in multiPlayer
	if (!bMultiPlayer)
	{
		// Send all transporter Droids back to home base if off world
		if (!goingHome)
		{
			/* Stop the camera following the transporter */
			psTransporter->selected = false;

			/* Send transporter offworld */
			UDWORD iX = 0, iY = 0;
			missionGetTransporterExit(psTransporter->player, &iX, &iY);
			orderDroidLoc(psTransporter, DORDER_TRANSPORTRETURN, iX, iY, ModeImmediate);

			// Set the launch time so the transporter doesn't just disappear for CAMSTART/CAMCHANGE
			transporterSetLaunchTime(gameTime);
		}
	}
}

void missionMoveTransporterOffWorld(DROID *psTransporter)
{
	W_CLICKFORM     *psForm;

	if (psTransporter->droidType == DROID_SUPERTRANSPORTER)
	{
		/* trigger script callback */
		transporterSetScriptCurrent(psTransporter);
		triggerEvent(TRIGGER_TRANSPORTER_EXIT, psTransporter);
		transporterSetScriptCurrent(nullptr);

		if (droidRemove(psTransporter, apsDroidLists))
		{
			addDroid(psTransporter, mission.apsDroidLists);
		}

		//stop the droid moving - the moveUpdate happens AFTER the orderUpdate and can cause problems if the droid moves from one tile to another
		moveReallyStopDroid(psTransporter);

		// clear targets / action targets
		setDroidTarget(psTransporter, nullptr);
		for (int i = 0; i < MAX_WEAPONS; i++)
		{
			setDroidActionTarget(psTransporter, nullptr, i);
		}

		//if offworld mission, then add the timer
		//if (mission.type == LDS_MKEEP || mission.type == LDS_MCLEAR)
		if (missionCanReEnforce() && psTransporter->player == selectedPlayer)
		{
			addTransporterTimerInterface();
			//set the data for the transporter timer label
			widgSetUserData(psWScreen, IDTRANTIMER_DISPLAY, (void *)psTransporter);

			//make sure the button is enabled
			psForm = (W_CLICKFORM *)widgGetFromID(psWScreen, IDTRANTIMER_BUTTON);
			if (psForm)
			{
				psForm->setState(WBUT_PLAIN);
			}
		}
		//need a callback for when all the selectedPlayers' reinforcements have been delivered
		if (psTransporter->player == selectedPlayer)
		{
			ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " exceeds MAX_PLAYERS", selectedPlayer);
			auto droidIt = std::find_if(mission.apsDroidLists[selectedPlayer].begin(), mission.apsDroidLists[selectedPlayer].end(), [](DROID* d)
			{
				return !d->isTransporter();
			});
			if (droidIt == mission.apsDroidLists[selectedPlayer].end())
			{
				triggerEvent(TRIGGER_TRANSPORTER_DONE, psTransporter);
			}
		}
	}
	else
	{
		debug(LOG_SAVE, "droid type not transporter!");
	}
}


//add the Mission timer into the top  right hand corner of the screen
bool intAddMissionTimer()
{
	//check to see if it exists already
	if (widgGetFromID(psWScreen, IDTIMER_FORM) != nullptr)
	{
		return true;
	}

	// Add the background
	W_FORMINIT sFormInit;

	sFormInit.formID = 0;
	sFormInit.id = IDTIMER_FORM;
	sFormInit.style = WFORM_PLAIN;

	sFormInit.width = iV_GetImageWidth(IntImages, IMAGE_MISSION_CLOCK); //TIMER_WIDTH;
	sFormInit.height = iV_GetImageHeight(IntImages, IMAGE_MISSION_CLOCK); //TIMER_HEIGHT;
	sFormInit.x = (SWORD)(RADTLX + RADWIDTH - sFormInit.width);
	sFormInit.y = (SWORD)TIMER_Y;
	sFormInit.calcLayout = LAMBDA_CALCLAYOUT_SIMPLE({
		psWidget->move((SWORD)(RADTLX + RADWIDTH - psWidget->width() - 18), TIMER_Y);
	});
	sFormInit.UserData = PACKDWORD_TRI(0, IMAGE_MISSION_CLOCK, IMAGE_MISSION_CLOCK_UP);
	sFormInit.pDisplay = intDisplayMissionClock;

	if (!widgAddForm(psWScreen, &sFormInit))
	{
		return false;
	}

	//add labels for the time display
	W_LABINIT sLabInit;
	sLabInit.formID = IDTIMER_FORM;
	sLabInit.id = IDTIMER_DISPLAY;
	sLabInit.style = WLAB_PLAIN | WIDG_HIDDEN;
	sLabInit.x = TIMER_LABELX;
	sLabInit.y = TIMER_LABELY;
	sLabInit.width = sFormInit.width;//TIMER_WIDTH;
	sLabInit.height = sFormInit.height;//TIMER_HEIGHT;
	sLabInit.pText = WzString::fromUtf8("00:00:00");
	sLabInit.pCallback = intUpdateMissionTimer;

	if (!widgAddLabel(psWScreen, &sLabInit))
	{
		return false;
	}

	return true;
}

//add the Transporter timer into the top left hand corner of the screen
bool intAddTransporterTimer()
{
	// Make sure that Transporter Launch button isn't up as well
	intRemoveTransporterLaunch();

	//check to see if it exists already
	if (widgGetFromID(psWScreen, IDTRANTIMER_BUTTON) != nullptr)
	{
		return true;
	}

	// Add the button form - clickable
	W_FORMINIT sFormInit;
	sFormInit.formID = 0;
	sFormInit.id = IDTRANTIMER_BUTTON;
	sFormInit.style = WFORM_CLICKABLE | WFORM_NOCLICKMOVE;
	sFormInit.x = TRAN_FORM_X;
	sFormInit.y = TRAN_FORM_Y;
	sFormInit.width = iV_GetImageWidth(IntImages, IMAGE_TRANSETA_UP);
	sFormInit.height = iV_GetImageHeight(IntImages, IMAGE_TRANSETA_UP);
	sFormInit.pTip = _("Load Transport");
	sFormInit.pDisplay = intDisplayImageHilight;
	sFormInit.UserData = PACKDWORD_TRI(0, IMAGE_TRANSETA_DOWN, IMAGE_TRANSETA_UP);

	W_FORM * pForm = widgAddForm(psWScreen, &sFormInit);
	if (!pForm)
	{
		return false;
	}

	pForm->setHelp(WidgetHelp()
				   .setTitle(_("Load Transport"))
				   .setDescription(_("Shows the number of units currently loaded into the mission transporter, and the total capacity."))
				   .addInteraction({WidgetHelp::InteractionTriggers::PrimaryClick}, _("Open the Transporter Load Menu")));

	//add labels for the time display
	W_LABINIT sLabInit;
	sLabInit.formID = IDTRANTIMER_BUTTON;
	sLabInit.id = IDTRANTIMER_DISPLAY;
	sLabInit.style = WIDG_HIDDEN;
	sLabInit.x = TRAN_TIMER_X;
	sLabInit.y = TRAN_TIMER_Y;
	sLabInit.width = TRAN_TIMER_WIDTH;
	sLabInit.height = sFormInit.height;
	sLabInit.pCallback = intUpdateTransporterTimer;
	if (!widgAddLabel(psWScreen, &sLabInit))
	{
		return false;
	}

	//add the capacity label
	sLabInit = W_LABINIT();
	sLabInit.formID = IDTRANTIMER_BUTTON;
	sLabInit.id = IDTRANS_CAPACITY;
	sLabInit.x = 70;
	sLabInit.y = 1;
	sLabInit.width = 16;
	sLabInit.height = 16;
	sLabInit.pText = WzString::fromUtf8("00/10");
	sLabInit.pCallback = intUpdateTransCapacity;
	auto psCapacityLabel = widgAddLabel(psWScreen, &sLabInit);
	if (!psCapacityLabel)
	{
		return false;
	}
	psCapacityLabel->setTransparentToMouse(true);

	return true;
}

void missionSetReinforcementTime(UDWORD iTime)
{
	g_iReinforceTime = iTime;
}

UDWORD  missionGetReinforcementTime()
{
	return g_iReinforceTime;
}

//fills in a hours(if bHours = true), minutes and seconds display for a given time in 1000th sec
static void fillTimeDisplay(W_LABEL &Label, UDWORD time, bool bHours)
{
	char psText[100];
	//this is only for the transporter timer - never have hours!
	if (time == LZ_COMPROMISED_TIME)
	{
		strcpy(psText, "--:--");
	}
	else
	{
		struct tm tmp = getUtcTime(static_cast<time_t>(time / GAME_TICKS_PER_SEC));
		strftime(psText, sizeof(psText), bHours ? "%H:%M:%S" : "%M:%S", &tmp);
	}
	Label.setString(WzString::fromUtf8(psText));
}


//update function for the mission timer
void intUpdateMissionTimer(WIDGET *psWidget, const W_CONTEXT *psContext)
{
	W_LABEL		*Label = (W_LABEL *)psWidget;
	UDWORD		timeElapsed;
	SDWORD		timeRemaining;

	// If the cheatTime has been set, then don't want the timer to countdown until stop cheating
	if (mission.cheatTime)
	{
		timeElapsed = mission.cheatTime - mission.startTime;
	}
	else
	{
		timeElapsed = gameTime - mission.startTime;
	}

	if (!challengeActive)
	{
		timeRemaining = mission.time - timeElapsed;
		if (timeRemaining < 0)
		{
			timeRemaining = 0;
		}
	}
	else
	{
		timeRemaining = timeElapsed;
	}

	fillTimeDisplay(*Label, timeRemaining, true);
	Label->show();  // Make sure its visible

	if (challengeActive)
	{
		return;	// all done
	}

	//make timer flash if time remaining < 5 minutes
	if (timeRemaining < FIVE_MINUTES)
	{
		flashMissionButton(IDTIMER_FORM);
	}
	//stop timer from flashing when gets to < 4 minutes
	if (timeRemaining < FOUR_MINUTES)
	{
		stopMissionButtonFlash(IDTIMER_FORM);
	}
	//play audio the first time the timed mission is started
	if (timeRemaining && (missionCountDown & NOT_PLAYED_ACTIVATED))
	{
		audio_QueueTrack(ID_SOUND_MISSION_TIMER_ACTIVATED);
		missionCountDown &= ~NOT_PLAYED_ACTIVATED;
	}
	//play some audio for mission countdown - start at 10 minutes remaining
	if (getPlayCountDown() && timeRemaining < TEN_MINUTES)
	{
		if (timeRemaining < TEN_MINUTES && (missionCountDown & NOT_PLAYED_TEN))
		{
			audio_QueueTrack(ID_SOUND_10_MINUTES_REMAINING);
			missionCountDown &= ~NOT_PLAYED_TEN;
		}
		else if (timeRemaining < FIVE_MINUTES && (missionCountDown & NOT_PLAYED_FIVE))
		{
			audio_QueueTrack(ID_SOUND_5_MINUTES_REMAINING);
			missionCountDown &= ~NOT_PLAYED_FIVE;
		}
		else if (timeRemaining < THREE_MINUTES && (missionCountDown & NOT_PLAYED_THREE))
		{
			audio_QueueTrack(ID_SOUND_3_MINUTES_REMAINING);
			missionCountDown &= ~NOT_PLAYED_THREE;
		}
		else if (timeRemaining < TWO_MINUTES && (missionCountDown & NOT_PLAYED_TWO))
		{
			audio_QueueTrack(ID_SOUND_2_MINUTES_REMAINING);
			missionCountDown &= ~NOT_PLAYED_TWO;
		}
		else if (timeRemaining < ONE_MINUTE && (missionCountDown & NOT_PLAYED_ONE))
		{
			audio_QueueTrack(ID_SOUND_1_MINUTE_REMAINING);
			missionCountDown &= ~NOT_PLAYED_ONE;
		}
	}
}

#define	TRANSPORTER_REINFORCE_LEADIN	10*GAME_TICKS_PER_SEC

//update function for the transporter timer
void intUpdateTransporterTimer(WIDGET *psWidget, const W_CONTEXT *psContext)
{
	W_LABEL		*Label = (W_LABEL *)psWidget;
	DROID		*psTransporter;
	SDWORD		timeRemaining;
	SDWORD		ETA;

	ETA = mission.ETA;
	if (ETA < 0)
	{
		ETA = 0;
	}

	// Get the object associated with this widget.
	psTransporter = (DROID *)Label->pUserData;
	if (psTransporter != nullptr)
	{
		ASSERT(psTransporter != nullptr,
		       "intUpdateTransporterTimer: invalid Droid pointer");

		if (psTransporter->action == DACTION_TRANSPORTIN ||
		    psTransporter->action == DACTION_TRANSPORTWAITTOFLYIN)
		{
			if (mission.ETA == LZ_COMPROMISED_TIME)
			{
				timeRemaining = LZ_COMPROMISED_TIME;
			}
			else
			{
				timeRemaining = mission.ETA - (gameTime - g_iReinforceTime);
				if (timeRemaining < 0)
				{
					timeRemaining = 0;
				}
				if (timeRemaining < TRANSPORTER_REINFORCE_LEADIN)
				{
					// arrived: tell the transporter to move to the new onworld
					// location if not already doing so
					if (psTransporter->action == DACTION_TRANSPORTWAITTOFLYIN)
					{
						missionFlyTransportersIn(selectedPlayer, false);
						executeFnAndProcessScriptQueuedRemovals([psTransporter]()
						{
							triggerEvent(TRIGGER_TRANSPORTER_ARRIVED, psTransporter);
						});
					}
				}
			}
			fillTimeDisplay(*Label, timeRemaining, false);
		}
		else
		{
			fillTimeDisplay(*Label, ETA, false);
		}
	}
	else
	{
		if (missionCanReEnforce())  // ((mission.type == LDS_MKEEP) || (mission.type == LDS_MCLEAR)) & (mission.ETA >= 0) ) {
		{
			fillTimeDisplay(*Label, ETA, false);
		}
		else
		{

			fillTimeDisplay(*Label, 0, false);

		}
	}

	//minutes
	/*calcTime = timeRemaining / (60*GAME_TICKS_PER_SEC);
	Label->aText[0] = (UBYTE)('0'+ calcTime / 10);
	Label->aText[1] = (UBYTE)('0'+ calcTime % 10);
	timeElapsed -= calcTime * (60*GAME_TICKS_PER_SEC);
	//separator
	Label->aText[3] = (UBYTE)(':');
	//seconds
	calcTime = timeRemaining / GAME_TICKS_PER_SEC;
	Label->aText[3] = (UBYTE)('0'+ calcTime / 10);
	Label->aText[4] = (UBYTE)('0'+ calcTime % 10);*/

	Label->show();
}

/* Remove the Mission Timer widgets from the screen*/
void intRemoveMissionTimer()
{
	// Check it's up.
	if (widgGetFromID(psWScreen, IDTIMER_FORM) != nullptr)
	{
		//and remove it.
		widgDelete(psWScreen, IDTIMER_FORM);
	}
}

/* Remove the Transporter Timer widgets from the screen*/
void intRemoveTransporterTimer()
{

	//remove main screen
	if (widgGetFromID(psWScreen, IDTRANTIMER_BUTTON) != nullptr)
	{
		widgDelete(psWScreen, IDTRANTIMER_BUTTON);
	}
}



// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////
// mission result functions for the interface.



static void intDisplayMissionBackDrop(WIDGET *psWidget, UDWORD xOffset, UDWORD yOffset)
{
	// Any widget using intDisplayMissionBackDrop must have its pUserData initialized to a (ScoreDataToScreenCache*)
	assert(psWidget->pUserData != nullptr);
	ScoreDataToScreenCache& cache = *static_cast<ScoreDataToScreenCache *>(psWidget->pUserData);

	scoreDataToScreen(psWidget, cache);
}

static void missionResetInGameState()
{
	// Add the background
	// get rid of reticule etc..
	intResetScreen(false);

	//stop the game if in single player mode
	setMissionPauseState();

	// reset the input state
	resetInput();

	forceHidePowerBar();
	intRemoveReticule();
	intRemoveMissionTimer();
	intRemoveTransporterTimer();
	intHideInGameOptionsButton();
	intHideGroupSelectionMenu();
}

static void intDestroyMissionResultWidgets()
{
	widgDelete(psWScreen, IDMISSIONRES_TITLE);
	widgDelete(psWScreen, IDMISSIONRES_FORM);
	widgDelete(psWScreen, IDMISSIONRES_BACKFORM);
}

static bool _intAddMissionResult(bool result, bool bPlaySuccess, bool showBackDrop)
{
	// ensure the guide screen is closed
	closeGuideScreen();

	missionResetInGameState();
	scoreUpdateVar(WD_MISSION_ENDED); //Store completion time for this mission

	W_FORMINIT sFormInit;

	// add some funky beats
	cdAudio_PlayTrack(SONG_FRONTEND);

	if (!bMultiPlayer && result && showBackDrop)
	{
		if (!screen_GetBackDrop())
		{
			pie_LoadBackDrop(SCREEN_MISSIONEND);
		}
		screen_RestartBackDrop();
	}

	// ensure these widgets are deleted before attempting to create
	intDestroyMissionResultWidgets();

	sFormInit.formID		= 0;
	sFormInit.id			= IDMISSIONRES_BACKFORM;
	sFormInit.style			= WFORM_PLAIN;
	sFormInit.pDisplay		= intDisplayMissionBackDrop;
	sFormInit.pUserData = new ScoreDataToScreenCache();
	sFormInit.onDelete = [](WIDGET *psWidget) {
		assert(psWidget->pUserData != nullptr);
		delete static_cast<ScoreDataToScreenCache *>(psWidget->pUserData);
		psWidget->pUserData = nullptr;
	};
	W_FORM *missionResBackForm = widgAddForm(psWScreen, &sFormInit);
	ASSERT_OR_RETURN(false, missionResBackForm != nullptr, "Failed to create IDMISSIONRES_BACKFORM");
	missionResBackForm->setCalcLayout(LAMBDA_CALCLAYOUT_SIMPLE({
		psWidget->setGeometry(0 + D_W, 0 + D_H, 640, 480);
	}));

	// TITLE
	auto missionResTitle = std::make_shared<IntFormAnimated>();
	missionResBackForm->attach(missionResTitle);
	missionResTitle->id = IDMISSIONRES_TITLE;
	missionResTitle->setCalcLayout(LAMBDA_CALCLAYOUT_SIMPLE({
		psWidget->setGeometry(MISSIONRES_TITLE_X, MISSIONRES_TITLE_Y, MISSIONRES_TITLE_W, MISSIONRES_TITLE_H);
	}));

	// add form
	auto missionResForm = std::make_shared<IntFormAnimated>();
	missionResBackForm->attach(missionResForm);
	missionResForm->id = IDMISSIONRES_FORM;
	missionResForm->setCalcLayout(LAMBDA_CALCLAYOUT_SIMPLE({
		psWidget->setGeometry(MISSIONRES_X, MISSIONRES_Y, MISSIONRES_W, MISSIONRES_H);
	}));

	// description of success/fail
	W_LABINIT sLabInit;
	sLabInit.formID = IDMISSIONRES_TITLE;
	sLabInit.id = IDMISSIONRES_TXT;
	sLabInit.style = WLAB_ALIGNCENTRE;
	sLabInit.x = 0;
	sLabInit.y = 12;
	sLabInit.width = MISSIONRES_TITLE_W;
	sLabInit.height = 16;
	if (result)
	{

		//don't bother adding the text if haven't played the audio
		if (bPlaySuccess)
		{
			sLabInit.pText = WzString::fromUtf8(Cheated ? _("OBJECTIVE ACHIEVED by cheating!") : _("OBJECTIVE ACHIEVED"));//"Objective Achieved";
		}

	}
	else
	{
		sLabInit.pText = WzString::fromUtf8(Cheated ? _("OBJECTIVE FAILED--and you cheated!") : _("OBJECTIVE FAILED")); //"Objective Failed;
	}
	sLabInit.FontID = font_regular;
	if (!widgAddLabel(psWScreen, &sLabInit))
	{
		return false;
	}
	// options.
	W_BUTINIT sButInit;
	sButInit.formID		= IDMISSIONRES_FORM;
	sButInit.style		= WBUT_TXTCENTRE;
	sButInit.width		= MISSION_TEXT_W;
	sButInit.height		= MISSION_TEXT_H;
	sButInit.pDisplay	= displayTextOption;
	sButInit.initPUserDataFunc = []() -> void * { return new DisplayTextOptionCache(); };
	sButInit.onDelete = [](WIDGET *psWidget) {
		assert(psWidget->pUserData != nullptr);
		delete static_cast<DisplayTextOptionCache *>(psWidget->pUserData);
		psWidget->pUserData = nullptr;
	};
	//if won
	if (result || bMultiPlayer)
	{
		// Finished the mission, so display "Continue Game"
		if (!testPlayerHasWon() || bMultiPlayer)
		{
			sButInit.x			= MISSION_1_X;
			sButInit.y			= MISSION_1_Y;
			sButInit.id			= IDMISSIONRES_CONTINUE;
			sButInit.pText		= _("Continue Game");//"Continue Game";
			widgAddButton(psWScreen, &sButInit);
		}

		// Won the game, so display "Quit to main menu"
		if (bMultiPlayer || (testPlayerHasWon() && !bMultiPlayer))
		{
			sButInit.x			= MISSION_2_X;
			sButInit.y			= MISSION_2_Y;
			sButInit.id			= IDMISSIONRES_QUIT;
			sButInit.pText		= _("Quit To Main Menu");
			widgAddButton(psWScreen, &sButInit);
		}

		// FIXME, We got serious issues with savegames at the *END* of some missions, and while they
		// will load, they don't have the correct state information or other settings.
		// See transition from CAM2->CAM3 for a example.
		/* Only add save option if in the game for real, ie, not fastplay.
		* And the player hasn't just completed the whole game
		* Don't add save option if just lost and in debug mode.
		if (!bMultiPlayer && !testPlayerHasWon() && !(testPlayerHasLost() && getDebugMappingStatus()))
		{
			//save
			sButInit.id			= IDMISSIONRES_SAVE;
			sButInit.x			= MISSION_1_X;
			sButInit.y			= MISSION_1_Y;
			sButInit.pText		= _("Save Game");//"Save Game";
			widgAddButton(psWScreen, &sButInit);

			// automatically save the game to be able to restart a mission
			saveGame((char *)"savegames/Autosave.gam", GTYPE_SAVE_START);
		}
		*/
	}
	else
	{
		//load
		sButInit.id			= IDMISSIONRES_LOAD;
		sButInit.x			= MISSION_1_X;
		sButInit.y			= MISSION_1_Y;
		sButInit.pText		= _("Load Saved Game");//"Load Saved Game";
		widgAddButton(psWScreen, &sButInit);
		//quit
		sButInit.id			= IDMISSIONRES_QUIT;
		sButInit.x			= MISSION_2_X;
		sButInit.y			= MISSION_2_Y;
		sButInit.pText		= _("Quit To Main Menu");//"Quit to Main Menu";
		widgAddButton(psWScreen, &sButInit);
	}

	intMode		= INT_MISSIONRES;
	MissionResUp = true;

	/* play result audio */
	if (result == true && bPlaySuccess)
	{
		audio_QueueTrack(ID_SOUND_OBJECTIVE_ACCOMPLISHED);
	}

	return true;
}


bool intAddMissionResult(bool result, bool bPlaySuccess, bool showBackDrop)
{
	ActivityManager::instance().completedMission(result, collectEndGameStatsData(), Cheated);
	return _intAddMissionResult(result, bPlaySuccess, showBackDrop);
}

void intRemoveMissionResultNoAnim()
{
	intDestroyMissionResultWidgets();

	cdAudio_Stop();

	MissionResUp	 	= false;
	intMode				= INT_NORMAL;

	//reset the pauses
	resetMissionPauseState();

	// add back the reticule and power bar.
	intAddReticule();

	intShowPowerBar();
	intShowGroupSelectionMenu();
}

void intRunMissionResult()
{
	wzSetCursor(CURSOR_DEFAULT);

	if (bLoadSaveUp)
	{
		if (runLoadSave(false)) // check for file name.
		{
			if (strlen(sRequestResult))
			{
				debug(LOG_SAVE, "Returned %s", sRequestResult);

				if (!bRequestLoad)
				{
					char msg[256] = {'\0'};

					saveGame(sRequestResult, GTYPE_SAVE_START);
					sstrcpy(msg, _("GAME SAVED :"));
					sstrcat(msg, savegameWithoutExtension(sRequestResult));
					addConsoleMessage(msg, LEFT_JUSTIFY, NOTIFY_MESSAGE);
				}
			}
		}
	}
}

static void missionContineButtonPressed()
{
	if (nextMissionType == LEVEL_TYPE::LDS_CAMSTART
	    || nextMissionType == LEVEL_TYPE::LDS_BETWEEN
	    || nextMissionType == LEVEL_TYPE::LDS_EXPAND
	    || nextMissionType == LEVEL_TYPE::LDS_EXPAND_LIMBO)
	{
		launchMission();
	}
	widgDelete(psWScreen, IDMISSIONRES_FORM);	//close option box.

	if (bMultiPlayer)
	{
		intRemoveMissionResultNoAnim();
	}
}

void intProcessMissionResult(UDWORD id)
{
	switch (id)
	{
	case IDMISSIONRES_LOAD:
		// throw up some filerequester
		addLoadSave(LOAD_MISSIONEND, _("Load Saved Game"));
		break;
	case IDMISSIONRES_SAVE:
		addLoadSave(SAVE_MISSIONEND, _("Save Game"));
		if (widgGetFromID(psWScreen, IDMISSIONRES_QUIT) == nullptr)
		{
			//Add Quit Button now save has been pressed
			W_BUTINIT sButInit;
			sButInit.formID		= IDMISSIONRES_FORM;
			sButInit.style		= WBUT_TXTCENTRE;
			sButInit.width		= MISSION_TEXT_W;
			sButInit.height		= MISSION_TEXT_H;
			sButInit.pDisplay	= displayTextOption;
			sButInit.pUserData = new DisplayTextOptionCache();
			sButInit.onDelete = [](WIDGET *psWidget) {
				assert(psWidget->pUserData != nullptr);
				delete static_cast<DisplayTextOptionCache *>(psWidget->pUserData);
				psWidget->pUserData = nullptr;
			};
			sButInit.id			= IDMISSIONRES_QUIT;
			sButInit.x			= MISSION_3_X;
			sButInit.y			= MISSION_3_Y;
			sButInit.pText		= _("Quit To Main Menu");
			widgAddButton(psWScreen, &sButInit);
		}
		break;
	case IDMISSIONRES_QUIT:
		// catered for by hci.c.
		break;
	case IDMISSIONRES_CONTINUE:
		if (bLoadSaveUp)
		{
			closeLoadSave();				// close save interface if it's up.
		}
		missionContineButtonPressed();
		break;

	default:
		break;
	}
}

// end of interface stuff.
// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////

/*builds a droid back at the home base whilst on a mission - stored in a list made
available to the transporter interface*/
DROID *buildMissionDroid(DROID_TEMPLATE *psTempl, UDWORD x, UDWORD y, UDWORD player)
{
	DROID		*psNewDroid;

	psNewDroid = buildDroid(psTempl, world_coord(x), world_coord(y), player, true, nullptr);
	if (!psNewDroid)
	{
		return nullptr;
	}
	addDroid(psNewDroid, mission.apsDroidLists);
	//set its x/y to impossible values so can detect when return from mission
	psNewDroid->pos.x = INVALID_XY;
	psNewDroid->pos.y = INVALID_XY;

	//set all the droids to selected from when return
	psNewDroid->selected = isSelectable(psNewDroid);

	// Set died parameter correctly
	psNewDroid->died = NOT_CURRENT_LIST;

	return psNewDroid;
}

//this causes the new mission data to be loaded up - only if startMission has been called
void launchMission()
{
	//if (mission.type == MISSION_NONE)
	if (mission.type == LEVEL_TYPE::LDS_NONE)
	{
		// tell the loop that a new level has to be loaded up
		loopMissionState = LMS_NEWLEVEL;
	}
	else
	{
		debug(LOG_SAVE, "Start Mission has not been called");
	}
}


//sets up the game to start a new mission
bool setUpMission(LEVEL_TYPE type)
{
	// Close the interface
	intResetScreen(true);

	/* The last mission must have been successful otherwise endgame would have been called */
	endMission();

	//release the level data for the previous mission
	if (!levReleaseMissionData())
	{
		return false;
	}

	if (type == LEVEL_TYPE::LDS_CAMSTART)
	{
		// Another one of those lovely hacks!!
		bool    bPlaySuccess = true;

		// We don't want the 'mission accomplished' audio/text message at end of cam1
		if (getCampaignNumber() == 2)
		{
			bPlaySuccess = false;
		}
		// Give the option of save/continue
		if (!intAddMissionResult(true, bPlaySuccess, true))
		{
			return false;
		}
		clearCampaignName();
		loopMissionState = LMS_SAVECONTINUE;
	}
	else if (type == LEVEL_TYPE::LDS_MKEEP
	         || type == LEVEL_TYPE::LDS_MCLEAR
	         || type == LEVEL_TYPE::LDS_MKEEP_LIMBO)
	{
		launchMission();
	}
	else
	{
		if (!getWidgetsStatus())
		{
			setWidgetsStatus(true);
			intResetScreen(false);
		}

		// Give the option of save / continue
		if (!intAddMissionResult(true, true, true))
		{
			return false;
		}
		loopMissionState = LMS_SAVECONTINUE;
	}

	return true;
}

//save the power settings before loading in the new map data
void saveMissionPower()
{
	UDWORD	inc;

	for (inc = 0; inc < MAX_PLAYERS; inc++)
	{
		mission.asCurrentPower[inc] = getPower(inc);
	}
}

//add the power from the home base to the current power levels for the mission map
void adjustMissionPower()
{
	UDWORD	inc;

	for (inc = 0; inc < MAX_PLAYERS; inc++)
	{
		addPower(inc, mission.asCurrentPower[inc]);
	}
}

/*sets the appropriate pause states for when the interface is up but the
game needs to be paused*/
void setMissionPauseState()
{
	if (!bMultiPlayer)
	{
		gameTimeStop();
		setGameUpdatePause(true);
		setAudioPause(true);
		setScriptPause(true);
		setConsolePause(true);
	}
}

/*resets the pause states */
void resetMissionPauseState()
{
	if (!bMultiPlayer)
	{
		setGameUpdatePause(false);
		setAudioPause(false);
		setScriptPause(false);
		setConsolePause(false);
		gameTimeStart();
	}
}

//gets the coords for a no go area
LANDING_ZONE *getLandingZone(SDWORD i)
{
	ASSERT((i >= 0) && (i < MAX_NOGO_AREAS), "getLandingZone out of range.");
	return &sLandingZone[i];
}

//Initialises all the nogo areas to 0
void initNoGoAreas()
{
	for (unsigned int i = 0; i < MAX_NOGO_AREAS; ++i)
	{
		sLandingZone[i].x1 = sLandingZone[i].y1 = sLandingZone[i].x2 = sLandingZone[i].y2 = 0;
	}
}

//sets the coords for a no go area
void setNoGoArea(UBYTE x1, UBYTE y1, UBYTE x2, UBYTE y2, UBYTE area)
{
	// make sure that x2 > x1 and y2 > y1
	if (x2 < x1)
	{
		std::swap(x1, x2);
	}
	if (y2 < y1)
	{
		std::swap(y1, y2);
	}

	sLandingZone[area].x1 = x1;
	sLandingZone[area].x2 = x2;
	sLandingZone[area].y1 = y1;
	sLandingZone[area].y2 = y2;

	if (area == 0 && x1 && y1)
	{
		addLandingLights(getLandingX(area) + 64, getLandingY(area) + 64);
	}
}

static inline void addLandingLight(int x, int y, LAND_LIGHT_SPEC spec, bool lit)
{
	// The height the landing lights should be above the ground
	static const unsigned int AboveGround = 16;
	Vector3i pos;

	if (x < 0 || y < 0)
	{
		return;
	}

	pos.x = x;
	pos.z = y;
	pos.y = map_Height(x, y) + AboveGround;

	effectSetLandLightSpec(spec);

	addEffect(&pos, EFFECT_EXPLOSION, EXPLOSION_TYPE_LAND_LIGHT, false, nullptr, lit);
}

static void addLandingLights(UDWORD x, UDWORD y)
{
	addLandingLight(x, y, LL_MIDDLE, true);                 // middle

	addLandingLight(x + 128, y + 128, LL_OUTER, true);     // outer
	addLandingLight(x + 128, y - 128, LL_OUTER, true);
	addLandingLight(x - 128, y + 128, LL_OUTER, true);
	addLandingLight(x - 128, y - 128, LL_OUTER, true);

	addLandingLight(x + 64, y + 64, LL_INNER, true);       // inner
	addLandingLight(x + 64, y - 64, LL_INNER, true);
	addLandingLight(x - 64, y + 64, LL_INNER, true);
	addLandingLight(x - 64, y - 64, LL_INNER, true);
}

/*	checks the x,y passed in are not within the boundary of any Landing Zone
	x and y in tile coords*/
bool withinLandingZone(UDWORD x, UDWORD y)
{
	UDWORD		inc;

	ASSERT(x < mapWidth, "withinLandingZone: x coord bigger than mapWidth");
	ASSERT(y < mapHeight, "withinLandingZone: y coord bigger than mapHeight");


	for (inc = 0; inc < MAX_NOGO_AREAS; inc++)
	{
		if ((x >= (UDWORD)sLandingZone[inc].x1 && x <= (UDWORD)sLandingZone[inc].x2) &&
		    (y >= (UDWORD)sLandingZone[inc].y1 && y <= (UDWORD)sLandingZone[inc].y2))
		{
			return true;
		}
	}
	return false;
}

//returns the x coord for where the Transporter can land (for player 0)
UWORD getLandingX(SDWORD iPlayer)
{
	ASSERT_OR_RETURN(0, iPlayer < MAX_NOGO_AREAS, "getLandingX: player %d out of range", iPlayer);
	return (UWORD)world_coord((sLandingZone[iPlayer].x1 + (sLandingZone[iPlayer].x2 -
	                           sLandingZone[iPlayer].x1) / 2));
}

//returns the y coord for where the Transporter can land
UWORD getLandingY(SDWORD iPlayer)
{
	ASSERT_OR_RETURN(0, iPlayer < MAX_NOGO_AREAS, "getLandingY: player %d out of range", iPlayer);
	return (UWORD)world_coord((sLandingZone[iPlayer].y1 + (sLandingZone[iPlayer].y2 -
	                           sLandingZone[iPlayer].y1) / 2));
}

//returns the x coord for where the Transporter can land back at home base
UDWORD getHomeLandingX()
{
	return map_coord(mission.homeLZ_X);
}

//returns the y coord for where the Transporter can land back at home base
UDWORD getHomeLandingY()
{
	return map_coord(mission.homeLZ_Y);
}

void missionSetTransporterEntry(SDWORD iPlayer, SDWORD iEntryTileX, SDWORD iEntryTileY)
{
	ASSERT_OR_RETURN(, iPlayer < MAX_PLAYERS, "missionSetTransporterEntry: player %i too high", iPlayer);

	if ((iEntryTileX > scrollMinX) && (iEntryTileX < scrollMaxX))
	{
		mission.iTranspEntryTileX[iPlayer] = (UWORD) iEntryTileX;
	}
	else
	{
		debug(LOG_SAVE, "entry point x %i outside scroll limits %i->%i", iEntryTileX, scrollMinX, scrollMaxX);
		mission.iTranspEntryTileX[iPlayer] = (UWORD)(scrollMinX + EDGE_SIZE);
	}

	if ((iEntryTileY > scrollMinY) && (iEntryTileY < scrollMaxY))
	{
		mission.iTranspEntryTileY[iPlayer] = (UWORD) iEntryTileY;
	}
	else
	{
		debug(LOG_SAVE, "entry point y %i outside scroll limits %i->%i", iEntryTileY, scrollMinY, scrollMaxY);
		mission.iTranspEntryTileY[iPlayer] = (UWORD)(scrollMinY + EDGE_SIZE);
	}
}

void missionSetTransporterExit(SDWORD iPlayer, SDWORD iExitTileX, SDWORD iExitTileY)
{
	ASSERT_OR_RETURN(, iPlayer < MAX_PLAYERS, "missionSetTransporterExit: player %i too high", iPlayer);

	if ((iExitTileX > scrollMinX) && (iExitTileX < scrollMaxX))
	{
		mission.iTranspExitTileX[iPlayer] = (UWORD) iExitTileX;
	}
	else
	{
		debug(LOG_SAVE, "entry point x %i outside scroll limits %i->%i", iExitTileX, scrollMinX, scrollMaxX);
		mission.iTranspExitTileX[iPlayer] = (UWORD)(scrollMinX + EDGE_SIZE);
	}

	if ((iExitTileY > scrollMinY) && (iExitTileY < scrollMaxY))
	{
		mission.iTranspExitTileY[iPlayer] = (UWORD) iExitTileY;
	}
	else
	{
		debug(LOG_SAVE, "entry point y %i outside scroll limits %i->%i", iExitTileY, scrollMinY, scrollMaxY);
		mission.iTranspExitTileY[iPlayer] = (UWORD)(scrollMinY + EDGE_SIZE);
	}
}

void missionGetTransporterEntry(SDWORD iPlayer, UWORD *iX, UWORD *iY)
{
	ASSERT_OR_RETURN(, iPlayer < MAX_PLAYERS, "missionGetTransporterEntry: player %i too high", iPlayer);

	*iX = (UWORD) world_coord(mission.iTranspEntryTileX[iPlayer]);
	*iY = (UWORD) world_coord(mission.iTranspEntryTileY[iPlayer]);
}

void missionGetTransporterExit(SDWORD iPlayer, UDWORD *iX, UDWORD *iY)
{
	ASSERT_OR_RETURN(, iPlayer < MAX_PLAYERS, "missionGetTransporterExit: player %i too high", iPlayer);

	*iX = world_coord(mission.iTranspExitTileX[iPlayer]);
	*iY = world_coord(mission.iTranspExitTileY[iPlayer]);
}

/*update routine for mission details */
void missionTimerUpdate()
{
	//don't bother with the time check if have 'cheated'
	if (!mission.cheatTime)
	{
		//Want a mission timer on all types of missions now - AB 26/01/99
		//only interested in off world missions (so far!) and if timer has been set
		if (mission.time >= 0)  //&& (
			//mission.type == LDS_MKEEP || mission.type == LDS_MKEEP_LIMBO ||
			//mission.type == LDS_MCLEAR || mission.type == LDS_BETWEEN))
		{
			//check if time is up
			if ((SDWORD)(gameTime - mission.startTime) > mission.time)
			{
				//the script can call the end game cos have failed!
				executeFnAndProcessScriptQueuedRemovals([]() { triggerEvent(TRIGGER_MISSION_TIMEOUT); });
			}
		}
	}
}


// Remove any objects left ie walls,structures and droids that are not the selected player.
//
void missionDestroyObjects()
{
	UBYTE Player, i;

	debug(LOG_SAVE, "called");
	proj_FreeAllProjectiles();
	for (Player = 0; Player < MAX_PLAYERS; Player++)
	{
		if (Player != selectedPlayer)
		{
			// AI player, clear out old data

			mutating_list_iterate(apsDroidLists[Player], [](DROID* d)
			{
				removeDroidBase(d);
				return IterationResult::CONTINUE_ITERATION;
			});

			//clear out the mission lists as well to make sure no Transporters exist
			apsDroidLists[Player] = std::move(mission.apsDroidLists[Player]);

			mutating_list_iterate(apsDroidLists[Player], [](DROID* psDroid)
			{
				//make sure its died flag is not set since we've swapped the apsDroidList pointers over
				psDroid->died = false;
				removeDroidBase(psDroid);
				return IterationResult::CONTINUE_ITERATION;
			});
			mission.apsDroidLists[Player].clear();

			mutating_list_iterate(apsStructLists[Player], [](STRUCTURE* s)
			{
				removeStruct(s, true);
				return IterationResult::CONTINUE_ITERATION;
			});
		}
	}

	// human player, check that we do not reference the cleared out data
	ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " exceeds MAX_PLAYERS", selectedPlayer);
	Player = selectedPlayer;

	for (DROID* psDroid : apsDroidLists[Player])
	{
		if (psDroid->psBaseStruct && psDroid->psBaseStruct->died)
		{
			setDroidBase(psDroid, nullptr);
		}
		for (i = 0; i < MAX_WEAPONS; i++)
		{
			if (psDroid->psActionTarget[i] && psDroid->psActionTarget[i]->died)
			{
				setDroidActionTarget(psDroid, nullptr, i);
				// Clear action too if this requires a valid first action target
				if (i == 0
				    && psDroid->action != DACTION_MOVEFIRE
				    && psDroid->action != DACTION_TRANSPORTIN
				    && psDroid->action != DACTION_TRANSPORTOUT)
				{
					psDroid->action = DACTION_NONE;
				}
			}
		}
		if (psDroid->order.psObj && psDroid->order.psObj->died)
		{
			setDroidTarget(psDroid, nullptr);
		}
	}

	for (STRUCTURE* psStruct : apsStructLists[Player])
	{
		for (i = 0; i < MAX_WEAPONS; i++)
		{
			if (psStruct->psTarget[i] && psStruct->psTarget[i]->died)
			{
				setStructureTarget(psStruct, nullptr, i, ORIGIN_UNKNOWN);
			}
		}
	}

	// FIXME: check that orders do not reference anything bad?

	if (!psDestroyedObj.empty())
	{
		debug(LOG_INFO, "%zu destroyed objects", psDestroyedObj.size());
	}
	gameTime++;	// Wonderful hack to ensure objects destroyed above get free'ed up by objmemUpdate.
	objmemUpdate();	// Actually free objects removed above
}

void processPreviousCampDroids()
{
	ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " exceeds MAX_PLAYERS", selectedPlayer);

	// See if any are left
	if (!mission.apsDroidLists[selectedPlayer].empty())
	{
		mutating_list_iterate(mission.apsDroidLists[selectedPlayer], [](DROID* psDroid)
		{
			// We want to kill off all droids now! - AB 27/01/99
			// KILL OFF TRANSPORTER
			if (droidRemove(psDroid, mission.apsDroidLists))
			{
				addDroid(psDroid, apsDroidLists);
				vanishDroid(psDroid);
			}
			return IterationResult::CONTINUE_ITERATION;
		});
	}
}

//access functions for droidsToSafety flag - so we don't have to end the mission when a Transporter fly's off world
void setDroidsToSafetyFlag(bool set)
{
	bDroidsToSafety = set;
}

bool getDroidsToSafetyFlag()
{
	return bDroidsToSafety;
}


//access functions for bPlayCountDown flag - true = play coded mission count down
void setPlayCountDown(UBYTE set)
{
	bPlayCountDown = set;
}

bool getPlayCountDown()
{
	return bPlayCountDown;
}


//checks to see if the player has any droids (except Transporters left)
bool missionDroidsRemaining(UDWORD player)
{
	ASSERT_OR_RETURN(false, player < MAX_PLAYERS, "invalid player: %" PRIu32 "", player);
	for (const DROID *psDroid : apsDroidLists[player])
	{
		if (!psDroid->isTransporter())
		{
			return true;
		}
	}
	return false;
}

/*called when a Transporter gets to the edge of the world and the droids are
being flown to safety. The droids inside the Transporter are placed into the
mission list for later use*/
void moveDroidsToSafety(DROID *psTransporter)
{
	ASSERT_OR_RETURN(, psTransporter->isTransporter(), "unit not a Transporter");

	if (psTransporter->psGroup != nullptr)
	{
		//move droids out of Transporter into mission list
		mutating_list_iterate(psTransporter->psGroup->psList, [psTransporter](DROID* psDroid)
		{
			if (psDroid == psTransporter)
			{
				return IterationResult::BREAK_ITERATION;
			}
			psTransporter->psGroup->remove(psDroid);
			//cam change add droid
			psDroid->pos.x = INVALID_XY;
			psDroid->pos.y = INVALID_XY;
			addDroid(psDroid, mission.apsDroidLists);

			return IterationResult::CONTINUE_ITERATION;
		});
	}

	//move the transporter into the mission list also
	if (droidRemove(psTransporter, apsDroidLists))
	{
		addDroid(psTransporter, mission.apsDroidLists);
	}
}

void clearMissionWidgets()
{
	//remove any widgets that are up due to the missions
	if (mission.time > 0)
	{
		intRemoveMissionTimer();
	}

	if (mission.ETA >= 0)
	{
		intRemoveTransporterTimer();
	}

	intRemoveTransporterLaunch();
}

/**
 * Try to find a transporter among the player's droids, or in the mission list (transporter waiting to come back).
 */
static DROID *find_transporter()
{
	if (selectedPlayer >= MAX_PLAYERS)
	{
		return nullptr;
	}

	for (DROID* droid : apsDroidLists[selectedPlayer])
	{
		if (droid->isTransporter())
		{
			return droid;
		}
	}
	for (DROID* droid : mission.apsDroidLists[selectedPlayer])
	{
		if (droid->isTransporter())
		{
			return droid;
		}
	}

	return nullptr;
}

void resetMissionWidgets()
{
	if (mission.type == LEVEL_TYPE::LDS_NONE)
	{
		return;
	}

	//add back any widgets that should be up due to the missions
	if (mission.time > 0)
	{
		intAddMissionTimer();
		//make sure its not flashing when added
		stopMissionButtonFlash(IDTIMER_FORM);
	}

	DROID* transporter = find_transporter();

	// Check if not a reinforcement type mission and we have an transporter
	if (!missionForReInforcements() && transporter != nullptr && !transporterFlying(transporter))
	{
		// Show launch button if the transporter has not already been launched
		intAddTransporterLaunch(transporter);
	}
	else if (mission.ETA >= 0)
	{
		addTransporterTimerInterface();
	}

}

/*deals with any selectedPlayer's transporters that are flying in when the
mission ends. bOffWorld is true if the Mission is currently offWorld*/
void emptyTransporters(bool bOffWorld)
{
	ASSERT_OR_RETURN(, selectedPlayer < MAX_PLAYERS, "selectedPlayer %" PRIu32 " >= MAX_PLAYERS", selectedPlayer);

	//see if there are any Transporters in the world
	mutating_list_iterate(apsDroidLists[selectedPlayer], [bOffWorld](DROID* psTransporter)
	{
		if (psTransporter->isTransporter())
		{
			//if flying in, empty the contents
			if (orderState(psTransporter, DORDER_TRANSPORTIN))
			{
				/* if we're offWorld, all we need to do is put the droids into the apsDroidList
				and processMission() will assign them a location etc */
				if (bOffWorld)
				{
					mutating_list_iterate(psTransporter->psGroup->psList, [psTransporter](DROID* psDroid)
					{
						if (psDroid == psTransporter)
						{
							return IterationResult::BREAK_ITERATION;
						}
						//take it out of the Transporter group
						psTransporter->psGroup->remove(psDroid);
						//add it back into current droid lists
						addDroid(psDroid, apsDroidLists);

						return IterationResult::CONTINUE_ITERATION;
					});
				}
				/* we're not offWorld so add to mission.apsDroidList to be
				processed by the endMission function */
				else
				{
					mutating_list_iterate(psTransporter->psGroup->psList, [psTransporter](DROID* psDroid)
					{
						if (psDroid == psTransporter)
						{
							return IterationResult::BREAK_ITERATION;
						}
						//take it out of the Transporter group
						psTransporter->psGroup->remove(psDroid);
						//add it back into current droid lists
						addDroid(psDroid, mission.apsDroidLists);

						return IterationResult::CONTINUE_ITERATION;
					});
				}
				//now kill off the Transporter
				vanishDroid(psTransporter);
			}
			else if (!bOffWorld && orderState(psTransporter, DORDER_TRANSPORTRETURN))
			{
				//also destroy transporters in the process of flying back and we're not offWorld
				vanishDroid(psTransporter);
			}
		}
		return IterationResult::CONTINUE_ITERATION;
	});

	//deal with any transporters that are waiting to come over
	mutating_list_iterate(mission.apsDroidLists[selectedPlayer], [](DROID* psTransporter)
	{
		if (psTransporter->isTransporter())
		{
			//for each droid within the transporter...
			mutating_list_iterate(psTransporter->psGroup->psList, [psTransporter](DROID* psDroid)
			{
				if (psDroid == psTransporter)
				{
					return IterationResult::BREAK_ITERATION;
				}
				//take it out of the Transporter group
				psTransporter->psGroup->remove(psDroid);
				//add it back into mission droid lists
				addDroid(psDroid, mission.apsDroidLists);

				return IterationResult::CONTINUE_ITERATION;
			});
		}
		//don't need to destroy the transporter here - it is dealt with by the endMission process
		return IterationResult::CONTINUE_ITERATION;
	});
}

/*bCheating = true == start of cheat
bCheating = false == end of cheat */
void setMissionCheatTime(bool bCheating)
{
	if (bCheating)
	{
		mission.cheatTime = gameTime;
	}
	else
	{
		//adjust the mission start time for the duration of the cheat!
		mission.startTime += gameTime - mission.cheatTime;
		mission.cheatTime = 0;
	}
}
