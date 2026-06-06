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

#pragma once

#include <cstddef>
#include <cstdint>

#include "gfx_api_formats_def.h"
#include <functional>
#include <memory>
#include <vector>

namespace gfx_api
{

struct abstract_texture;

struct TransientRenderTargetKey
{
	pixel_format format = pixel_format::invalid;
	uint32_t width = 0;
	uint32_t height = 0;

	bool operator==(const TransientRenderTargetKey& other) const
	{
		return format == other.format && width == other.width && height == other.height;
	}
};

/// <summary>
/// Per-backend pool of reusable offscreen color render targets.
/// Textures are returned to the pool via releaseAll() at frame start.
/// </summary>
class TransientRenderTargetPool
{
public:
	using CreateFn = std::function<std::unique_ptr<abstract_texture>()>;

	abstract_texture* acquire(const TransientRenderTargetKey& key, CreateFn createFn);
	void releaseAll();
	void clear();

private:
	struct Entry
	{
		TransientRenderTargetKey key;
		std::unique_ptr<abstract_texture> texture;
		bool inUse = false;
	};

	std::vector<Entry> _entries;
};

} // namespace gfx_api
