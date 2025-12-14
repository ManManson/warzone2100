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

#include "path_heatmap.h"
#include <algorithm>
#include <limits>

PathHeatmap::PathHeatmap(private_init_tag_t, const PathHeatmap& other)
	: PathHeatmap(other)
{
}

PathHeatmap& PathHeatmap::instance()
{
	static PathHeatmap instance;
	return instance;
}

std::shared_ptr<const PathHeatmap> PathHeatmap::takeSnapshot() const
{
	return std::make_shared<PathHeatmap>(private_init_tag_t{}, *this);
}

void PathHeatmap::init(int width, int height)
{
	if (width <= 0 || height <= 0)
	{
		mapWidth_ = mapHeight_ = 0;
		heatCells_.clear();
		return;
	}

	mapWidth_ = width;
	mapHeight_ = height;

	widthShift_ = 0;
	while ((1u << widthShift_) < static_cast<unsigned>(mapWidth_))
	{
		++widthShift_;
	}
	stride_ = 1 << widthShift_;

	heatCells_.clear();
	heatCells_.resize(static_cast<size_t>(stride_) * static_cast<size_t>(mapHeight_));

	// Initialize cells to zero/none
	for (auto& c : heatCells_)
	{
		c.heatValue = 0;
		c.ownerDroidId = std::numeric_limits<uint32_t>::max();
	}

	offset_ = 0;
}

void PathHeatmap::shutdown()
{
	heatCells_.clear();
	mapWidth_ = mapHeight_ = widthShift_ = stride_ = 0;
	offset_ = 0;
}

void PathHeatmap::advanceOffset()
{
	++offset_;
}

void PathHeatmap::addPointHeat(uint32_t droidId, int tileX, int tileY, uint32_t amount)
{
	if (!isInBounds(tileX, tileY))
	{
		return;
	}

	unsigned idx = getIndex(tileX, tileY);
	uint32_t storeVal = offset_ + amount;
	auto& cell = heatCells_[idx];
	if (cell.heatValue < storeVal)
	{
		cell.heatValue = storeVal;
		cell.ownerDroidId = droidId;
	}
}

uint32_t PathHeatmap::getHeat(int tileX, int tileY, uint32_t excludeOwnerDroidId /* = NO_OWNER */) const
{
	if (!isInBounds(tileX, tileY))
	{
		return 0;
	}

	unsigned idx = getIndex(tileX, tileY);
	const auto& cell = heatCells_[idx];
	uint32_t offset = offset_;

	if (cell.ownerDroidId == excludeOwnerDroidId)
	{
		return 0;
	}
	if (cell.heatValue <= offset)
	{
		return 0;
	}
	return cell.heatValue - offset;
}
