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

#include <nonstd/optional.hpp>

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
/// Color transients use uncompressed color pixel_format values; depth transients use
/// pixel_format::FORMAT_D24_UNORM_S8 (backend maps to the real depth/stencil format).
///
/// Resource cache trilogy (shared transient images + backend-specific pass objects):
///
/// Transient images (all backends): FrameResourceCache
///
/// Vulkan dynamic passes:
/// - Render passes: PassLayoutKey → getOrCreatePassRenderPassId (cached vk::RenderPass)
/// - Framebuffers: FramebufferResourceCache (cached vk::Framebuffer per attachment-view key)
///
/// OpenGL dynamic passes:
/// - Dynamic pass FBOs: DynamicFBOCache (cached GLuint FBO per attachment binding key)
///   (GL has no render-pass object; swapchain uses default framebuffer directly)
///
/// Lifecycle (callers must preserve this ordering):
/// 1. releaseAll() at frame graph reset (start of accumulation).
/// 2. acquireImage() / acquire() on framebuffer caches while executing the graph.
/// 3. submitFrame() on the backend (Vulkan: buffering_mechanism::swap waits on the frame fence).
/// 4. purgeUnused() after submit — drop images/FBOs not acquired this frame.
///
/// Vulkan framebuffer purge defers GPU destruction via fbo_to_delete queues.
/// OpenGL FBO purge calls glDeleteFramebuffers immediately after the frame is presented.
/// Transient image purge destroys textures via unique_ptr destructors (VK may defer further).
class FrameResourceCache
{
public:
	using CreateImageFn = std::function<std::unique_ptr<abstract_texture>()>;
	/// Invoked for each pooled image dropped by purgeUnused() or clear() before destruction.
	using PurgeImageFn = std::function<void(abstract_texture*)>;

	abstract_texture* acquireImage(const ImageResourceKey& key, CreateImageFn createFn);

	/// Mark all cached images as unused for the new frame (LegitVulkan Release).
	void releaseAll();

	/// Drop unused capacity and empty keys after frame resolve (LegitVulkan PurgeUnused).
	void purgeUnused(const PurgeImageFn& onPurge = PurgeImageFn());

	void clear(const PurgeImageFn& onPurge = PurgeImageFn());

private:
	struct ImageCacheEntry
	{
		size_t usedCount = 0;
		std::vector<std::unique_ptr<abstract_texture>> images;
	};

	std::vector<std::pair<ImageResourceKey, ImageCacheEntry>> _imageCache;
};

/// Key for pooled Vulkan framebuffer instances (render pass + attachment views + size).
struct FramebufferResourceKey
{
	size_t renderPassId = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	/// VkImageView handles stored as uint64_t for backend-neutral cache code.
	std::vector<uint64_t> attachmentViewHandles;

	bool operator<(const FramebufferResourceKey& other) const
	{
		return std::tie(renderPassId, width, height, attachmentViewHandles)
			< std::tie(other.renderPassId, other.width, other.height, other.attachmentViewHandles);
	}

	bool operator==(const FramebufferResourceKey& other) const
	{
		return renderPassId == other.renderPassId
			&& width == other.width
			&& height == other.height
			&& attachmentViewHandles == other.attachmentViewHandles;
	}
};

/// Per-frame pool of reusable Vulkan framebuffers for dynamic attachment passes.
/// Lifecycle mirrors FrameResourceCache: releaseAll at graph reset, acquire during passes,
/// purgeUnused after submitFrame. Purge/clear callbacks defer GPU destruction until safe.
class FramebufferResourceCache
{
public:
	using CreateFramebufferFn = std::function<uint64_t()>;
	using PurgeFramebufferFn = std::function<void(uint64_t)>;

	uint64_t acquire(const FramebufferResourceKey& key, CreateFramebufferFn createFn);

	void releaseAll();

	void purgeUnused(const PurgeFramebufferFn& onPurge = PurgeFramebufferFn());

	void clear(const PurgeFramebufferFn& onPurge = PurgeFramebufferFn());

private:
	struct FramebufferCacheEntry
	{
		size_t usedCount = 0;
		std::vector<uint64_t> framebuffers;
	};

	std::vector<std::pair<FramebufferResourceKey, FramebufferCacheEntry>> _framebufferCache;
};

/// Key for pooled OpenGL FBO instances (attachment object ids + layers + size).
struct DynamicFBOKey
{
	struct Slot
	{
		uint32_t objectId = 0;
		uint32_t arrayLayer = 0;
		bool isRenderbuffer = false;

		bool operator<(const Slot& other) const
		{
			return std::tie(objectId, arrayLayer, isRenderbuffer)
				< std::tie(other.objectId, other.arrayLayer, other.isRenderbuffer);
		}

		bool operator==(const Slot& other) const
		{
			return objectId == other.objectId
				&& arrayLayer == other.arrayLayer
				&& isRenderbuffer == other.isRenderbuffer;
		}
	};

	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<Slot> colorSlots;
	nonstd::optional<Slot> depthSlot;

	bool operator<(const DynamicFBOKey& other) const
	{
		return std::tie(width, height, colorSlots, depthSlot)
			< std::tie(other.width, other.height, other.colorSlots, other.depthSlot);
	}

	bool operator==(const DynamicFBOKey& other) const
	{
		return width == other.width
			&& height == other.height
			&& colorSlots == other.colorSlots
			&& depthSlot == other.depthSlot;
	}
};

/// Per-frame pool of reusable OpenGL FBOs for dynamic attachment passes.
class DynamicFBOCache
{
public:
	using CreateFBOFn = std::function<uint32_t()>;
	using PurgeFBOFn = std::function<void(uint32_t)>;

	uint32_t acquire(const DynamicFBOKey& key, CreateFBOFn createFn);

	void releaseAll();

	void purgeUnused(const PurgeFBOFn& onPurge = PurgeFBOFn());

	void clear(const PurgeFBOFn& onPurge = PurgeFBOFn());

private:
	struct DynamicFBOCacheEntry
	{
		size_t usedCount = 0;
		std::vector<uint32_t> fbos;
	};

	std::vector<std::pair<DynamicFBOKey, DynamicFBOCacheEntry>> _dynamicFBOCache;
};

} // namespace gfx_api
