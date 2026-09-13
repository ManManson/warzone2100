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

#include "sim_clock.h"

#include "lib/framework/frame.h"

SimClock& SimClock::instance()
{
	static SimClock clock;
	return clock;
}

void SimClock::setMode(SimClockMode mode)
{
	mode_ = mode;
}

void SimClock::catchUpTo(uint32_t)
{
	ASSERT(false, "Catch-up is not wired yet.");
}

void SimClock::cancelCatchUp()
{
	if (mode_ == SimClockMode::CatchUp)
	{
		mode_ = SimClockMode::Realtime;
	}
	catchUpTarget_ = 0;
	catchUpOrigin_ = 0;
}

void SimClock::requestManualTicks(size_t)
{
	ASSERT(false, "Manual ticks are not wired yet.");
}

void SimClock::reset()
{
	mode_ = SimClockMode::Realtime;
	catchUpTarget_ = 0;
	catchUpOrigin_ = 0;
	manualTicksRemaining_ = 0;
}

bool SimClock::shouldTickThisFrame(size_t, uint32_t) const
{
	switch (mode_)
	{
		case SimClockMode::Paused:
		case SimClockMode::Realtime:
		case SimClockMode::FastForward:
			// Keep the existing gameLoop contract: attempt ticks until simulateOneGameTick
			// reports no update. Realtime must not be capped at one attempt — renderBudget
			// and spectator fast-forward both rely on extra WallClock iterations.
			return true;
		case SimClockMode::CatchUp:
		case SimClockMode::Manual:
			ASSERT(false, "Catch-up / manual clock modes are not wired yet.");
			return false;
	}
	return false;
}
