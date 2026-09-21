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

#include "replay_archive.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <nonstd/optional.hpp>

using nonstd::optional;
using nonstd::nullopt;

class IReplayKeyframeStore
{
public:
	virtual ~IReplayKeyframeStore() = default;
	virtual void put(uint32_t gameTime, std::string blob, ReplayQueueCursors cursors) = 0;
	virtual optional<uint32_t> latestAtOrBefore(uint32_t gameTime) const = 0;
	virtual const std::string* blobAt(uint32_t gameTime) const = 0;
	virtual const ReplayQueueCursors* cursorsAt(uint32_t gameTime) const = 0;
};

class ReplayKeyframeStore : public IReplayKeyframeStore
{
public:
	static std::unique_ptr<ReplayKeyframeStore> make();

	void put(uint32_t gameTime, std::string blob, ReplayQueueCursors cursors) override;
	optional<uint32_t> latestAtOrBefore(uint32_t gameTime) const override;
	const std::string* blobAt(uint32_t gameTime) const override;
	const ReplayQueueCursors* cursorsAt(uint32_t gameTime) const override;

private:
	struct Entry
	{
		std::string blob;
		ReplayQueueCursors cursors;
	};
	std::map<uint32_t, Entry> snapshots_;
};
