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

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <ranges>

enum class TickAdvanceKind
{
	WallClock,
	Forced
};

enum class SimClockMode
{
	Paused,
	Realtime,
	FastForward,
	CatchUp,
	Unthrottled,
	Manual
};

enum class PresentationKind
{
	Full,
	OverlayOnly,
	None
};

struct ClockPolicyRow
{
	SimClockMode mode;
	TickAdvanceKind tick;
	PresentationKind present;
};

constexpr ClockPolicyRow CLOCK_POLICIES[] = {
	{SimClockMode::Paused,      TickAdvanceKind::WallClock, PresentationKind::Full},
	{SimClockMode::Realtime,    TickAdvanceKind::WallClock, PresentationKind::Full},
	{SimClockMode::FastForward, TickAdvanceKind::WallClock, PresentationKind::Full},
	{SimClockMode::CatchUp,     TickAdvanceKind::Forced,    PresentationKind::OverlayOnly},
	{SimClockMode::Unthrottled, TickAdvanceKind::Forced,    PresentationKind::None},
	{SimClockMode::Manual,      TickAdvanceKind::Forced,    PresentationKind::None},
};

// How often a windowed catch-up returns to wzEventLoopOneFrame. Long enough that
// the sim dominates a seek; short of the few seconds a window manager waits
// before treating the process as not responding. 0 on a catch-up means no yield.
constexpr uint32_t CATCH_UP_PRESENT_INTERVAL_MS = 200;

consteval bool clockPolicyValid(ClockPolicyRow r)
{
	if (r.tick == TickAdvanceKind::WallClock && r.present == PresentationKind::OverlayOnly)
	{
		return false;
	}
	const bool catchUpOrManual = r.mode == SimClockMode::CatchUp || r.mode == SimClockMode::Manual;
	if (catchUpOrManual && r.tick != TickAdvanceKind::Forced)
	{
		return false;
	}
	if (r.mode == SimClockMode::CatchUp && r.present != PresentationKind::OverlayOnly)
	{
		return false;
	}
	if (r.mode == SimClockMode::Manual && r.present != PresentationKind::None)
	{
		return false;
	}
	if (r.mode == SimClockMode::Unthrottled
	    && (r.tick != TickAdvanceKind::Forced || r.present != PresentationKind::None))
	{
		return false;
	}
	return true;
}

static_assert(std::ranges::all_of(CLOCK_POLICIES, clockPolicyValid));

template <typename P>
concept SimClockPolicy = requires {
	{ P::mode } -> std::convertible_to<SimClockMode>;
	{ P::tick } -> std::convertible_to<TickAdvanceKind>;
	{ P::present } -> std::convertible_to<PresentationKind>;
} && clockPolicyValid({P::mode, P::tick, P::present});

constexpr ClockPolicyRow clockPolicyFor(SimClockMode mode)
{
	for (const auto& row : CLOCK_POLICIES)
	{
		if (row.mode == mode)
		{
			return row;
		}
	}
	return CLOCK_POLICIES[1]; // Realtime fallback
}

class SimClock
{
public:
	static SimClock& instance();

	void setMode(SimClockMode mode);
	void catchUpTo(uint32_t targetGameTime, uint32_t presentIntervalMs);
	void cancelCatchUp();
	void requestManualTicks(size_t n);
	void reset();

	SimClockMode mode() const { return mode_; }
	TickAdvanceKind tickKind() const { return clockPolicyFor(mode_).tick; }
	PresentationKind presentation() const { return clockPolicyFor(mode_).present; }
	bool shouldTickThisFrame(size_t ticksSoFar, uint32_t frameElapsedMs) const;
	uint32_t catchUpTarget() const { return catchUpTarget_; }
	uint32_t catchUpOrigin() const { return catchUpOrigin_; }
	bool isCatchingUp() const { return mode_ == SimClockMode::CatchUp; }

private:
	SimClock() = default;

	SimClockMode mode_ = SimClockMode::Realtime;
	uint32_t catchUpTarget_ = 0;
	uint32_t catchUpOrigin_ = 0;
	uint32_t presentIntervalMs_ = 0;
	size_t manualTicksRemaining_ = 0;
};
