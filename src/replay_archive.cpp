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

#include "replay_archive.h"

#include "lib/netplay/netplay.h"
#include "lib/netplay/nettypes.h"

#include <nonstd/optional.hpp>

using nonstd::optional;
using nonstd::nullopt;

namespace
{

optional<uint32_t> gameTimeFromGameTimeMessage(const NetMessage& message)
{
	if (message.type() != GAME_GAME_TIME)
	{
		return nullopt;
	}
	MessageReader r(message);
	uint32_t latencyTicks = 0;
	uint32_t checkTime = 0;
	NETuint32_t(r, latencyTicks);
	NETuint32_t(r, checkTime);
	return checkTime;
}

} // namespace

std::unique_ptr<ReplayMessageArchive> ReplayMessageArchive::buildFromLoadedQueues()
{
	auto archive = std::make_unique<ReplayMessageArchive>();
	archive->buildIndex();
	return archive;
}

void ReplayMessageArchive::buildIndex()
{
	index_.clear();
	const unsigned host = NetPlay.hostPlayer;
	if (host >= MAX_GAMEQUEUE_SLOTS)
	{
		return;
	}
	NETgameQueueForEachMessage(host, [this](const NetMessage& message) {
		if (auto checkTime = gameTimeFromGameTimeMessage(message))
		{
			index_.push_back(ReplayTimeIndexEntry{*checkTime});
		}
	});
}

uint32_t ReplayMessageArchive::lastIndexedGameTime() const
{
	return index_.empty() ? 0 : index_.back().gameTime;
}

void ReplayMessageArchive::rewindToGameTime(uint32_t gameTime)
{
	for (unsigned player = 0; player < MAX_GAMEQUEUE_SLOTS; ++player)
	{
		NETgameQueueRewindReadCursorToStart(player);
		if (gameTime == 0)
		{
			continue;
		}
		while (NETgameQueueHaveMessage(player))
		{
			const NetMessage* message = NETgameQueueGetMessage(player);
			if (message == nullptr)
			{
				break;
			}
			if (auto checkTime = gameTimeFromGameTimeMessage(*message))
			{
				if (*checkTime >= gameTime)
				{
					break;
				}
			}
			NETgameQueuePopMessage(player);
		}
	}
}
