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

#include "replay_session.h"

#include "replay_archive.h"
#include "replay_keyframes.h"

#include "lib/framework/frame.h"
#include "lib/framework/string_ext.h"
#include "lib/gamelib/gtime.h"
#include "lib/ivis_opengl/gfx_api.h"
#include "lib/netplay/nettypes.h"
#include "lib/netplay/sync_debug.h"
#include "lib/sound/audio.h"
#include "gamestate_serialize.h"
#include "loop.h"
#include "sim_clock.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <limits>
#include <variant>

ReplaySession& ReplaySession::instance()
{
	static ReplaySession session;
	return session;
}

ReplaySession::~ReplaySession() = default;

bool ReplaySession::active()
{
	return NETisReplay() && instance().attached_;
}

void ReplaySession::attachAfterReplayLoad(uint32_t durationMs)
{
	attached_ = true;
	durationMs_ = durationMs;
	archive_ = ReplayMessageArchive::buildFromLoadedQueues();
	keyframes_ = ReplayKeyframeStore::make();
	capturedInitialState_ = false;
	syncClockModeFromPlayback();
}

void ReplaySession::shutdown()
{
	attached_ = false;
	durationMs_ = 0;
	resumePaused_ = false;
	capturedInitialState_ = false;
	pendingSeekMs_.reset();
	archive_.reset();
	keyframes_.reset();
	SimClock::instance().reset();
	audio_StopAll();
	audio_setPause(false);
	if (gfx_api::context::isInitialized())
	{
		gfx_api::context::get().releaseSwapchainFreeze();
	}
}

void ReplaySession::dispatch(ReplayCommand cmd)
{
	std::visit([this](auto&& c) { handle(c); }, cmd);
}

bool ReplaySession::isPaused() const
{
	return gameTimeIsStopped();
}

size_t ReplaySession::fastForwardTicks() const
{
	return getMaxFastForwardTicks();
}

uint32_t ReplaySession::timelineDurationMs() const
{
	return std::max(durationMs_, playheadMs());
}

uint32_t ReplaySession::keyframeHorizonMs() const
{
	if (!keyframes_)
	{
		return 0;
	}
	return keyframes_->latestAtOrBefore(std::numeric_limits<uint32_t>::max()).value_or(0);
}

bool ReplaySession::isBusy() const
{
	return pendingSeekMs_.has_value() || SimClock::instance().isCatchingUp();
}

optional<ReplayBusyStatus> ReplaySession::busy() const
{
	auto& clock = SimClock::instance();
	if (pendingSeekMs_.has_value())
	{
		return ReplayBusyStatus{playheadMs(), *pendingSeekMs_};
	}
	if (!clock.isCatchingUp())
	{
		return nullopt;
	}
	return ReplayBusyStatus{clock.catchUpOrigin(), clock.catchUpTarget()};
}

void ReplaySession::syncClockModeFromPlayback()
{
	if (gameTimeIsStopped())
	{
		SimClock::instance().setMode(SimClockMode::Paused);
		return;
	}
	SimClock::instance().setMode(getMaxFastForwardTicks() > 0 ? SimClockMode::FastForward : SimClockMode::Realtime);
}

void ReplaySession::onGameLoopBegin()
{
	captureInitialStateIfNeeded();
}

void ReplaySession::onGameLoopAfterTicks(bool tickExhausted)
{
	maybePlantKeyframe();
	auto& clock = SimClock::instance();
	if (!clock.isCatchingUp())
	{
		return;
	}
	if (gameTime >= clock.catchUpTarget() || tickExhausted)
	{
		finishCatchUp();
	}
}

void ReplaySession::cancelBusy()
{
	if (pendingSeekMs_.has_value())
	{
		pendingSeekMs_.reset();
		resumePaused_ = true;
		applyResumeIntent();
		return;
	}
	if (!SimClock::instance().isCatchingUp())
	{
		return;
	}
	resumePaused_ = true;
	finishCatchUp();
}

void ReplaySession::beginCatchUpAudio()
{
	audio_setPause(true);
	audio_StopAll();
}

void ReplaySession::endCatchUpAudio()
{
	audio_StopAll();
	audio_setPause(resumePaused_);
}

void ReplaySession::applyResumeIntent()
{
	endCatchUpAudio();
	if (resumePaused_)
	{
		if (!gameTimeIsStopped())
		{
			gameTimeStop();
		}
		setAudioPause(true);
		setConsolePause(true);
		SimClock::instance().setMode(SimClockMode::Paused);
		return;
	}
	setAudioPause(false);
	setConsolePause(false);
	if (gameTimeIsStopped())
	{
		gameTimeStart();
	}
	syncClockModeFromPlayback();
}

void ReplaySession::finishCatchUp()
{
	SimClock::instance().cancelCatchUp();
	applyResumeIntent();
	graphicsTime = gameTime;
	gameTimeRebaseRealTimeBase();
}

void ReplaySession::captureInitialStateIfNeeded()
{
	if (capturedInitialState_ || !keyframes_)
	{
		return;
	}
	capturedInitialState_ = true;
	try
	{
		nlohmann::ordered_json j = gamestate::gameStateToJson(gamestate::ScriptScope::AllInstances);
		j.erase("presentation");
		keyframes_->put(0, j.dump());
	}
	catch (const std::exception& e)
	{
		debug(LOG_ERROR, "Replay: failed to capture t=0 GameState: %s", e.what());
	}
}

void ReplaySession::maybePlantKeyframe()
{
	if (!keyframes_ || !capturedInitialState_)
	{
		return;
	}
	const uint32_t slot = (gameTime / REPLAY_KEYFRAME_INTERVAL_MS) * REPLAY_KEYFRAME_INTERVAL_MS;
	if (slot == 0 || keyframes_->blobAt(slot) != nullptr)
	{
		return;
	}
	try
	{
		nlohmann::ordered_json j = gamestate::gameStateToJson(gamestate::ScriptScope::AllInstances);
		j.erase("presentation");
		keyframes_->put(slot, j.dump());
	}
	catch (const std::exception& e)
	{
		debug(LOG_ERROR, "Replay: failed to plant keyframe at %u: %s", slot, e.what());
	}
}

optional<uint32_t> ReplaySession::latestKeyframeForSeek(uint32_t timeMs) const
{
	if (!keyframes_)
	{
		return nullopt;
	}
	if (auto exact = keyframes_->latestAtOrBefore(timeMs))
	{
		return exact;
	}
	return nullopt;
}

void ReplaySession::restoreFromKeyframe(uint32_t keyframeTime)
{
	const std::string* blob = keyframes_ ? keyframes_->blobAt(keyframeTime) : nullptr;
	if (blob == nullptr)
	{
		setGameTime(2);
		return;
	}
	try
	{
		nlohmann::ordered_json j = gamestate::parseJsonBounded(blob->data(), blob->data() + blob->size());
		j.erase("presentation");
		gamestate::gameStateFromJson(j, gamestate::ScriptScope::AllInstances);
		resetSyncDebug();
		applyResumeSyncDebugCrc();
		setSyncCheckFloorTime(gameTime);
	}
	catch (const std::exception& e)
	{
		debug(LOG_ERROR, "Replay: failed to restore keyframe at %u: %s", keyframeTime, e.what());
		setGameTime(2);
	}
}

void ReplaySession::startCatchUpTo(uint32_t timeMs)
{
	auto& clock = SimClock::instance();
	const optional<uint32_t> keyframe = latestKeyframeForSeek(timeMs);
	const bool cheaperKeyframe = keyframe.has_value() && *keyframe > playheadMs();
	const bool needRestore = timeMs < playheadMs() || cheaperKeyframe;
	if (gameTimeIsStopped())
	{
		gameTimeStart();
	}
	clock.catchUpTo(timeMs);
	if (needRestore)
	{
		const uint32_t restoreTime = keyframe.value_or(0);
		restoreFromKeyframe(restoreTime);
		if (archive_)
		{
			archive_->rewindToGameTime(restoreTime);
		}
	}
}

void ReplaySession::commitPendingSeek()
{
	if (!pendingSeekMs_.has_value())
	{
		return;
	}
	const uint32_t T = *pendingSeekMs_;
	pendingSeekMs_.reset();
	startCatchUpTo(T);
}

void ReplaySession::seekTo(uint32_t timeMs)
{
	auto& clock = SimClock::instance();
	const uint32_t T = quantizeReplaySeekMs(timeMs, timelineDurationMs());
	if (T == playheadMs() && !clock.isCatchingUp() && !pendingSeekMs_.has_value())
	{
		return;
	}

	if (!clock.isCatchingUp() && !pendingSeekMs_.has_value())
	{
		resumePaused_ = isPaused();
	}

	beginCatchUpAudio();

	if (clock.isCatchingUp())
	{
		startCatchUpTo(T);
		return;
	}

	if (gameTimeIsStopped())
	{
		gameTimeStart();
	}
	pendingSeekMs_ = T;
	if (gfx_api::context::isInitialized())
	{
		gfx_api::context::get().requestSwapchainFreezeCapture();
	}
}

void ReplaySession::handle(CmdPlay)
{
	resumePaused_ = false;
	if (SimClock::instance().isCatchingUp())
	{
		return;
	}
	setAudioPause(false);
	setConsolePause(false);
	if (gameTimeIsStopped())
	{
		gameTimeStart();
	}
	syncClockModeFromPlayback();
}

void ReplaySession::handle(CmdPause)
{
	resumePaused_ = true;
	if (SimClock::instance().isCatchingUp())
	{
		return;
	}
	if (!gameTimeIsStopped())
	{
		gameTimeStop();
	}
	setAudioPause(true);
	setConsolePause(true);
	SimClock::instance().setMode(SimClockMode::Paused);
}

void ReplaySession::handle(CmdSeekAbsolute cmd)
{
	seekTo(cmd.timeMs);
}

void ReplaySession::handle(CmdSeekRelative cmd)
{
	uint32_t target = playheadMs();
	if (cmd.deltaMs < 0)
	{
		const uint32_t back = static_cast<uint32_t>(-cmd.deltaMs);
		target = (playheadMs() < back) ? 0 : playheadMs() - back;
	}
	else
	{
		target += static_cast<uint32_t>(cmd.deltaMs);
	}
	seekTo(target);
}

void ReplaySession::handle(CmdSeekToStart)
{
	seekTo(0);
}

void ReplaySession::handle(CmdCycleRate cmd)
{
	const auto currFastForward = getMaxFastForwardTicks();
	size_t newFastForward = 0;
	if (currFastForward == 0)
	{
		newFastForward = (!cmd.reverse) ? 1 : WZ_MAX_REPLAY_FASTFORWARD_TICKS;
	}
	else if (currFastForward == 1)
	{
		newFastForward = (!cmd.reverse) ? WZ_MAX_REPLAY_FASTFORWARD_TICKS : 0;
	}
	else if (cmd.reverse && currFastForward == WZ_MAX_REPLAY_FASTFORWARD_TICKS)
	{
		newFastForward = 1;
	}
	setMaxFastForwardTicks(newFastForward, true);
	if (!SimClock::instance().isCatchingUp())
	{
		syncClockModeFromPlayback();
	}
}

void ReplaySession::handle(CmdSetRate cmd)
{
	setMaxFastForwardTicks(cmd.rate.ticksPerFrame, true);
	if (!SimClock::instance().isCatchingUp())
	{
		syncClockModeFromPlayback();
	}
}

std::string formatReplayTime(uint32_t timeMs, bool useHours)
{
	const uint32_t totalSec = timeMs / GAME_TICKS_PER_SEC;
	const uint32_t s = totalSec % 60;
	const uint32_t m = (totalSec / 60) % 60;
	const uint32_t h = totalSec / 3600;
	if (useHours)
	{
		return astringf("%u:%02u:%02u", h, m, s);
	}
	return astringf("%u:%02u", m, s);
}

std::string formatReplayClockLabel(uint32_t playheadMs, uint32_t durationMs)
{
	const uint32_t totalMs = std::max(durationMs, playheadMs);
	if (totalMs == 0)
	{
		return "--:-- / --:--";
	}
	const bool useHours = totalMs >= 3600u * GAME_TICKS_PER_SEC;
	return astringf("%s / %s", formatReplayTime(playheadMs, useHours).c_str(), formatReplayTime(totalMs, useHours).c_str());
}
