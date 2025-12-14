// SPDX-License-Identifier: GPL-2.0-or-later

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

#include <stdint.h>
#include <memory>
#include <vector>

#include "geometry.h" // Vector2i
#include "map.h"      // map_coord

/// <summary>
/// Heatmap structure for use by pathfinding code.
/// This helps droids to avoid tiles occupied by other droids, thus overall
/// improving pathfinding for droid groups which actively maneuver between
/// other droid formations.
///
/// Has exactly the same dimensions as the game map (should be explicitly
/// initialized by calling `init()`).
///
/// The internal structure uses a rolling offset to avoid having to clear and re-assign
/// all tiles of the heatmap every tick. The offset is advanced once per simulation tick
/// by calling `advanceOffset()`, which naturally support "heat dissipation" over time.
///
/// NOTE: internal heat values are represented by an 32-bit integer per tile.
/// There is also an owner ID per tile, which helps to avoid self-interference
/// when a droid queries the heat value of a tile it has just added heat to.
///
/// WARNING: this class is NOT thread-safe. It is expected that only
/// the main thread will modify it, while pathfinding threads will only
/// read from read-only snapshots taken at the start of each simulation tick (created by
/// calling `takeSnapshot()`).
/// </summary>
class PathHeatmap
{
	struct private_init_tag_t {};

public:

	static constexpr uint32_t DEFAULT_HEAT_AMOUNT = 16;
	// Special value designating that the owner droid ID should be ignored when querying heat.
	static constexpr uint32_t NO_OWNER = UINT32_MAX;

	// Helper constructor for creating read-only snapshots via `takeSnapshot()`,
	// not accessible publicly
	explicit PathHeatmap(private_init_tag_t, const PathHeatmap& other);

	static PathHeatmap& instance();
	std::shared_ptr<const PathHeatmap> takeSnapshot() const;

	void init(int width, int height);
	void shutdown();

	// Per-tick update (called once per simulation tick), advances the rolling offset by 1.
	void advanceOffset();

	// Add heat for a single tile (tile coordinates)
	void addPointHeat(uint32_t droidId, int tileX, int tileY, uint32_t amount = DEFAULT_HEAT_AMOUNT);
	uint32_t getHeat(int tileX, int tileY, uint32_t excludeOwnerDroidId = NO_OWNER) const;

private:

	struct HeatCell
	{
		uint32_t heatValue;
		uint32_t ownerDroidId;
	};

	explicit PathHeatmap() = default;
	PathHeatmap(const PathHeatmap &) = default;

	unsigned getIndex(int x, int y) const { return (static_cast<unsigned>(y) << widthShift_) | static_cast<unsigned>(x); }
	bool isInBounds(int x, int y) const { return x >= 0 && x < mapWidth_ && y >= 0 && y < mapHeight_; }

	uint32_t offset_ = 0;
	int mapWidth_ = 0;
	int mapHeight_ = 0;
	int widthShift_ = 0;
	int stride_ = 0;
	std::vector<HeatCell> heatCells_;
};
