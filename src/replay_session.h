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

#include <memory>
#include <nonstd/optional.hpp>
#include <string>

using nonstd::optional;
using nonstd::nullopt;

class IReplayKeyframeStore;
class IReplayMessageArchive;

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

	~ReplaySession();

	void attachAfterReplayLoad(uint32_t durationMs);
	void shutdown();

	void dispatch(ReplayCommand cmd);
	void onGameLoopBegin();
	void onGameLoopAfterTicks(bool tickExhausted);
	void cancelBusy();

	uint32_t durationMs() const { return durationMs_; }
	uint32_t timelineDurationMs() const;
	uint32_t playheadMs() const { return gameTime; }
	bool isPaused() const;
	size_t fastForwardTicks() const;
	bool isBusy() const;
	optional<ReplayBusyStatus> busy() const;

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
	void seekTo(uint32_t timeMs);
	void captureInitialStateIfNeeded();
	void maybePlantKeyframe();
	optional<uint32_t> latestKeyframeForSeek(uint32_t timeMs) const;
	void restoreFromKeyframe(uint32_t keyframeTime);
	void applyResumeIntent();
	void finishCatchUp();

	bool attached_ = false;
	uint32_t durationMs_ = 0;
	bool resumePaused_ = false;
	bool capturedInitialState_ = false;
	std::unique_ptr<IReplayMessageArchive> archive_;
	std::unique_ptr<IReplayKeyframeStore> keyframes_;
};

std::string formatReplayTime(uint32_t timeMs, bool useHours);
std::string formatReplayClockLabel(uint32_t playheadMs, uint32_t durationMs);
