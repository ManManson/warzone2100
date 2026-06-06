/*
	This file is part of Warzone 2100.
	Copyright (C) 2017-2026  Warzone 2100 Project

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

#include "gfx_api_transient_render_targets.h"

#include "gfx_api.h"

namespace gfx_api
{

abstract_texture* TransientRenderTargetPool::acquire(const TransientRenderTargetKey& key, CreateFn createFn)
{
	for (auto& entry : _entries)
	{
		if (!entry.inUse && entry.key == key)
		{
			entry.inUse = true;
			return entry.texture.get();
		}
	}

	auto texture = createFn();
	ASSERT(texture != nullptr, "Failed to create transient render target");
	_entries.push_back({key, std::move(texture), true});
	return _entries.back().texture.get();
}

void TransientRenderTargetPool::releaseAll()
{
	for (auto& entry : _entries)
	{
		entry.inUse = false;
	}
}

void TransientRenderTargetPool::clear()
{
	_entries.clear();
}

} // namespace gfx_api
