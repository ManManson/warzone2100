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

#include "gfx_api_frame_resource_cache.h"

#include "gfx_api.h"

#include <algorithm>

namespace gfx_api
{

abstract_texture* FrameResourceCache::acquireImage(const ImageResourceKey& key, CreateImageFn createFn)
{
	auto cacheIt = std::find_if(_imageCache.begin(), _imageCache.end(),
		[&key](const auto& entry) { return entry.first == key; });

	if (cacheIt == _imageCache.end())
	{
		_imageCache.emplace_back(key, ImageCacheEntry {});
		cacheIt = std::prev(_imageCache.end());
	}

	ImageCacheEntry& cacheEntry = cacheIt->second;
	if (cacheEntry.usedCount + 1 > cacheEntry.images.size())
	{
		auto texture = createFn();
		ASSERT(texture != nullptr, "Failed to create transient image for frame resource cache");
		cacheEntry.images.emplace_back(std::move(texture));
	}

	return cacheEntry.images[cacheEntry.usedCount++].get();
}

void FrameResourceCache::releaseAll()
{
	for (auto& cacheEntry : _imageCache)
	{
		cacheEntry.second.usedCount = 0;
	}
}

namespace
{

void dropExcessImages(std::vector<std::unique_ptr<abstract_texture>>& images, size_t usedCount,
	const FrameResourceCache::PurgeImageFn& onPurge)
{
	ASSERT(usedCount <= images.size(), "Frame resource cache usedCount exceeds image count");
	while (images.size() > usedCount)
	{
		abstract_texture* dropped = images.back().get();
		if (onPurge && dropped != nullptr)
		{
			onPurge(dropped);
		}
		images.pop_back();
	}
}

} // namespace

void FrameResourceCache::purgeUnused(const PurgeImageFn& onPurge)
{
	for (auto it = _imageCache.begin(); it != _imageCache.end(); )
	{
		ImageCacheEntry& cacheEntry = it->second;
		dropExcessImages(cacheEntry.images, cacheEntry.usedCount, onPurge);
		if (cacheEntry.usedCount == 0)
		{
			it = _imageCache.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void FrameResourceCache::clear(const PurgeImageFn& onPurge)
{
	if (onPurge)
	{
		for (auto& cacheEntry : _imageCache)
		{
			for (const auto& image : cacheEntry.second.images)
			{
				if (image != nullptr)
				{
					onPurge(image.get());
				}
			}
		}
	}
	_imageCache.clear();
}

uint64_t FramebufferResourceCache::acquire(const FramebufferResourceKey& key, CreateFramebufferFn createFn)
{
	auto cacheIt = std::find_if(_framebufferCache.begin(), _framebufferCache.end(),
		[&key](const auto& entry) { return entry.first == key; });

	if (cacheIt == _framebufferCache.end())
	{
		_framebufferCache.emplace_back(key, FramebufferCacheEntry {});
		cacheIt = std::prev(_framebufferCache.end());
	}

	FramebufferCacheEntry& cacheEntry = cacheIt->second;
	if (cacheEntry.usedCount + 1 > cacheEntry.framebuffers.size())
	{
		const uint64_t framebuffer = createFn();
		ASSERT(framebuffer != 0, "Failed to create framebuffer for frame resource cache");
		cacheEntry.framebuffers.push_back(framebuffer);
	}

	return cacheEntry.framebuffers[cacheEntry.usedCount++];
}

void FramebufferResourceCache::releaseAll()
{
	for (auto& cacheEntry : _framebufferCache)
	{
		cacheEntry.second.usedCount = 0;
	}
}

namespace
{

void dropExcessFramebuffers(std::vector<uint64_t>& framebuffers, size_t usedCount,
	const FramebufferResourceCache::PurgeFramebufferFn& onPurge)
{
	ASSERT(usedCount <= framebuffers.size(), "Framebuffer cache usedCount exceeds framebuffer count");
	while (framebuffers.size() > usedCount)
	{
		const uint64_t dropped = framebuffers.back();
		if (onPurge && dropped != 0)
		{
			onPurge(dropped);
		}
		framebuffers.pop_back();
	}
}

} // namespace

void FramebufferResourceCache::purgeUnused(const PurgeFramebufferFn& onPurge)
{
	for (auto it = _framebufferCache.begin(); it != _framebufferCache.end(); )
	{
		FramebufferCacheEntry& cacheEntry = it->second;
		dropExcessFramebuffers(cacheEntry.framebuffers, cacheEntry.usedCount, onPurge);
		if (cacheEntry.usedCount == 0)
		{
			it = _framebufferCache.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void FramebufferResourceCache::clear(const PurgeFramebufferFn& onPurge)
{
	if (onPurge)
	{
		for (auto& cacheEntry : _framebufferCache)
		{
			for (const uint64_t framebuffer : cacheEntry.second.framebuffers)
			{
				if (framebuffer != 0)
				{
					onPurge(framebuffer);
				}
			}
		}
	}
	_framebufferCache.clear();
}

} // namespace gfx_api
