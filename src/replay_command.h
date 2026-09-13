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

#include "lib/gamelib/gtime.h"

#include <algorithm>
#include <cstdint>
#include <variant>

constexpr uint32_t REPLAY_UI_QUANTUM_MS = GAME_TICKS_PER_SEC;
constexpr uint32_t REPLAY_SKIP_STEP_MS = 15 * GAME_TICKS_PER_SEC;
constexpr uint32_t REPLAY_KEYFRAME_INTERVAL_MS = 60 * GAME_TICKS_PER_SEC;
constexpr size_t WZ_MAX_REPLAY_FASTFORWARD_TICKS = 3;

static_assert(REPLAY_UI_QUANTUM_MS % GAME_TICKS_PER_UPDATE == 0);
static_assert(REPLAY_SKIP_STEP_MS % GAME_TICKS_PER_UPDATE == 0);
static_assert(REPLAY_KEYFRAME_INTERVAL_MS % GAME_TICKS_PER_UPDATE == 0);

constexpr uint32_t quantizeReplaySeekMs(uint32_t timeMs, uint32_t durationMs)
{
	const uint32_t q = (timeMs / REPLAY_UI_QUANTUM_MS) * REPLAY_UI_QUANTUM_MS;
	return std::min(q, durationMs);
}

static_assert(quantizeReplaySeekMs(1499, 100000) == 1000);
static_assert(quantizeReplaySeekMs(999999, 45000) == 45000);

struct PlaybackRate
{
	size_t ticksPerFrame = 0;
};

struct CmdPlay {};
struct CmdPause {};
struct CmdSeekAbsolute { uint32_t timeMs = 0; };
struct CmdSeekRelative { int32_t deltaMs = 0; };
struct CmdSeekToStart {};
struct CmdCycleRate { bool reverse = false; };
struct CmdSetRate { PlaybackRate rate; };

using ReplayCommand = std::variant<
	CmdPlay, CmdPause, CmdSeekAbsolute, CmdSeekRelative,
	CmdSeekToStart, CmdCycleRate, CmdSetRate>;
