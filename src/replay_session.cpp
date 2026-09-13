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

#include "lib/framework/string_ext.h"
#include "lib/netplay/nettypes.h"
#include "loop.h"
#include "sim_clock.h"

#include <algorithm>
#include <variant>

ReplaySession& ReplaySession::instance()
{
	static ReplaySession session;
	return session;
}

bool ReplaySession::active()
{
	return NETisReplay() && instance().attached_;
}

void ReplaySession::attachAfterReplayLoad(uint32_t durationMs)
{
	attached_ = true;
	durationMs_ = durationMs;
	syncClockModeFromPlayback();
}

void ReplaySession::shutdown()
{
	attached_ = false;
	durationMs_ = 0;
	SimClock::instance().reset();
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

void ReplaySession::syncClockModeFromPlayback()
{
	if (gameTimeIsStopped())
	{
		SimClock::instance().setMode(SimClockMode::Paused);
		return;
	}
	SimClock::instance().setMode(getMaxFastForwardTicks() > 0 ? SimClockMode::FastForward : SimClockMode::Realtime);
}

void ReplaySession::handle(CmdPlay)
{
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
	if (!gameTimeIsStopped())
	{
		gameTimeStop();
	}
	setAudioPause(true);
	setConsolePause(true);
	SimClock::instance().setMode(SimClockMode::Paused);
}

void ReplaySession::handle(CmdSeekAbsolute)
{
	// Seek is wired in a later milestone.
}

void ReplaySession::handle(CmdSeekRelative)
{
	// Seek is wired in a later milestone.
}

void ReplaySession::handle(CmdSeekToStart)
{
	// Seek is wired in a later milestone.
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
	syncClockModeFromPlayback();
}

void ReplaySession::handle(CmdSetRate cmd)
{
	setMaxFastForwardTicks(cmd.rate.ticksPerFrame, true);
	syncClockModeFromPlayback();
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
