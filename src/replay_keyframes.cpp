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

#include "replay_keyframes.h"

std::unique_ptr<ReplayKeyframeStore> ReplayKeyframeStore::make()
{
	return std::make_unique<ReplayKeyframeStore>();
}

void ReplayKeyframeStore::put(uint32_t gameTime, std::string blob, ReplayQueueCursors cursors)
{
	snapshots_[gameTime] = Entry{std::move(blob), cursors};
}

optional<uint32_t> ReplayKeyframeStore::latestAtOrBefore(uint32_t gameTime) const
{
	if (snapshots_.empty())
	{
		return nullopt;
	}
	auto it = snapshots_.upper_bound(gameTime);
	if (it == snapshots_.begin())
	{
		return nullopt;
	}
	--it;
	return it->first;
}

const std::string* ReplayKeyframeStore::blobAt(uint32_t gameTime) const
{
	const auto it = snapshots_.find(gameTime);
	if (it == snapshots_.end())
	{
		return nullptr;
	}
	return &it->second.blob;
}

const ReplayQueueCursors* ReplayKeyframeStore::cursorsAt(uint32_t gameTime) const
{
	const auto it = snapshots_.find(gameTime);
	if (it == snapshots_.end())
	{
		return nullptr;
	}
	return &it->second.cursors;
}
