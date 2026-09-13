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

#pragma once

#include "replay_command.h"

#include "lib/gamelib/gtime.h"

#include <nonstd/optional.hpp>
#include <string>

using nonstd::optional;
using nonstd::nullopt;

struct ReplayBusyStatus
{
	uint32_t originMs = 0;
	uint32_t targetMs = 0;
};

class ReplaySession
{
public:
	static ReplaySession& instance();
	static bool active();

	void attachAfterReplayLoad(uint32_t durationMs);
	void shutdown();

	void dispatch(ReplayCommand cmd);

	uint32_t durationMs() const { return durationMs_; }
	uint32_t timelineDurationMs() const;
	uint32_t playheadMs() const { return gameTime; }
	bool isPaused() const;
	size_t fastForwardTicks() const;
	bool isBusy() const { return false; }
	optional<ReplayBusyStatus> busy() const { return nullopt; }

private:
	ReplaySession() = default;

	void handle(CmdPlay);
	void handle(CmdPause);
	void handle(CmdSeekAbsolute);
	void handle(CmdSeekRelative);
	void handle(CmdSeekToStart);
	void handle(CmdCycleRate);
	void handle(CmdSetRate);

	void syncClockModeFromPlayback();

	bool attached_ = false;
	uint32_t durationMs_ = 0;
};

std::string formatReplayTime(uint32_t timeMs, bool useHours);
std::string formatReplayClockLabel(uint32_t playheadMs, uint32_t durationMs);
