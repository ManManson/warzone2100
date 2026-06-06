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
#include <string>
#include <tuple>
#include <vector>

namespace gfx_api
{

struct abstract_texture;

/// Key for pooled transient image allocations (LegitVulkan ImageCache::ImageKey subset).
struct ImageResourceKey
{
	pixel_format format = pixel_format::invalid;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t arrayLayers = 1;
	uint32_t samples = 1;
	std::string debugName;

	bool operator<(const ImageResourceKey& other) const
	{
		return std::tie(format, width, height, arrayLayers, samples)
			< std::tie(other.format, other.width, other.height, other.arrayLayers, other.samples);
	}

	bool operator==(const ImageResourceKey& other) const
	{
		return format == other.format
			&& width == other.width
			&& height == other.height
			&& arrayLayers == other.arrayLayers
			&& samples == other.samples;
	}

	static ImageResourceKey color2d(pixel_format fmt, uint32_t w, uint32_t h, uint32_t sampleCount = 1)
	{
		ImageResourceKey key;
		key.format = fmt;
		key.width = w;
		key.height = h;
		key.arrayLayers = 1;
		key.samples = sampleCount;
		return key;
	}
};

/// Subresource view into an image (for depth cascade layers, mip slices, etc.).
struct ImageViewResourceKey
{
	abstract_texture* image = nullptr;
	uint32_t baseMipLevel = 0;
	uint32_t mipLevels = 1;
	uint32_t baseArrayLayer = 0;
	uint32_t arrayLayers = 1;
	std::string debugName;

	bool operator<(const ImageViewResourceKey& other) const
	{
		return std::tie(image, baseMipLevel, mipLevels, baseArrayLayer, arrayLayers)
			< std::tie(other.image, other.baseMipLevel, other.mipLevels, other.baseArrayLayer, other.arrayLayers);
	}
};

/// Per-frame pool of reusable transient images (LegitVulkan ImageCache pattern).
class FrameResourceCache
{
public:
	using CreateImageFn = std::function<std::unique_ptr<abstract_texture>()>;

	abstract_texture* acquireImage(const ImageResourceKey& key, CreateImageFn createFn);

	/// Mark all cached images as unused for the new frame (LegitVulkan Release).
	void releaseAll();

	/// Drop unused capacity and empty keys after frame resolve (LegitVulkan PurgeUnused).
	void purgeUnused();

	void clear();

private:
	struct ImageCacheEntry
	{
		size_t usedCount = 0;
		std::vector<std::unique_ptr<abstract_texture>> images;
	};

	std::vector<std::pair<ImageResourceKey, ImageCacheEntry>> _imageCache;
};

} // namespace gfx_api
