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

#include <nonstd/optional.hpp>

#include <cstdint>
#include <string>

using nonstd::optional;

/// Process-wide run parameters set from the command line before the game starts.
/// Read-only after clparse. Every field has the "option absent" value as its default, so an
/// ordinary launch is unchanged.
struct RunControl
{
	optional<uint32_t> gameSeed;       ///< synchronised RNG seed; unset = rand()
	uint32_t quitAtGameTime = 0;       ///< quit cleanly at this gameTime; 0 = never
	bool deterministicLatency = false; ///< wall-clock-free order-queue latency
	std::string replayName;            ///< fixed replay filename stem; empty = timestamped
};

const RunControl &runControl();
RunControl &runControlMutable(); ///< clparse only, before the game starts

/// Quit once gameTime reaches quitAtGameTime. No-op when that field is 0.
void runControlMaybeQuit();
