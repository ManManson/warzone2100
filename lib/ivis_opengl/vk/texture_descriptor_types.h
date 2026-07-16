// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project (https://github.com/Warzone2100)

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
/** @file texture_descriptor_types.h
 * Shared types for Vulkan texture descriptor push and allocated-set binding.
 */

#pragma once

#if defined(WZ_VULKAN_ENABLED)

#include "vk/vulkan_hpp_include.h"

#include <cstdint>
#include <vector>

/// <summary>
/// Per-frame counters for texture descriptor binding work.
///
/// Accumulated on `perFrameResources_t`.
/// Snapshotted to `VkRoot::lastSubmittedTextureDescriptorStats` at submit time
/// for optional debug logging.
/// </summary>
struct TextureDescriptorStats
{
	uint64_t bindCalls = 0;
	uint64_t allocatedSets = 0;
	uint64_t updateCalls = 0;
	uint64_t boundSets = 0;
	uint64_t pushCalls = 0;
	uint64_t pushedDescriptors = 0;

	void reset()
	{
		*this = {};
	}
};

/// <summary>
/// Reusable scratch buffers for `vk::WriteDescriptorSet` image writes.
///
/// Built by `TextureDescriptorWriter` and consumed by `bind_textures`.
/// </summary>
struct TextureDescriptorWriteBatch
{
	std::vector<vk::DescriptorImageInfo> imageInfos;
	std::vector<vk::WriteDescriptorSet> writes;

	void prepare(size_t count)
	{
		imageInfos.clear();
		writes.clear();
		imageInfos.reserve(count);
		writes.reserve(count);
	}
};

/// <summary>
/// How a PSO's texture descriptor set is bound at draw time.
/// </summary>
enum class TextureDescriptorBindingMode : uint8_t
{
	AllocatedSet,
	Push,
};

/// <summary>
/// Whether descriptor writes supply samplers or rely on immutable samplers in the layout.
/// </summary>
enum class TextureSamplerMode : uint8_t
{
	Immutable,
	Dynamic,
};

/// <summary>
/// Result of choosing a texture descriptor binding mode for a PSO.
///
/// `reason` is logged at PSO creation when `WZ_VK_TEXTURE_BINDING_LOG` is set.
/// </summary>
struct TextureBindingDecision
{
	TextureDescriptorBindingMode mode = TextureDescriptorBindingMode::AllocatedSet;
	const char* reason = "unknown";
};

/// <summary>
/// Runtime capability and limits for `VK_KHR_push_descriptor` texture binding.
///
/// Negotiated once on `VkRoot`.
/// Consumed by `TextureBindingPlanner` when choosing push vs pool allocation.
/// </summary>
struct PushDescriptorSupport
{
	bool extensionAdvertised = false;
	bool extensionEnabled = false;
	bool commandLoaded = false;
	bool disabledByPolicy = false;
	uint32_t maxDescriptors = 0;

	bool usableFor(uint32_t descriptorCount) const
	{
		return extensionEnabled
			&& commandLoaded
			&& !disabledByPolicy
			&& descriptorCount > 0
			&& descriptorCount <= maxDescriptors;
	}

	/// <summary>
	/// Human-readable reason for push viability at the given descriptor count.
	/// Stable string literals suitable for logging.
	/// </summary>
	const char* reasonFor(uint32_t descriptorCount) const
	{
		if (disabledByPolicy)
		{
			return "disabled by WZ_VK_DISABLE_PUSH_DESCRIPTORS";
		}
		if (!extensionAdvertised)
		{
			return "extension not advertised";
		}
		if (!extensionEnabled)
		{
			return "Properties2 query failed or maxPushDescriptors is zero";
		}
		if (!commandLoaded)
		{
			return "vkCmdPushDescriptorSetKHR unavailable";
		}
		if (descriptorCount == 0)
		{
			return "no texture descriptors";
		}
		if (descriptorCount > maxDescriptors)
		{
			return "descriptor count exceeds maxPushDescriptors";
		}
		return "push descriptors available";
	}
};

/// <summary>
/// Per-PSO texture descriptor set layout and binding strategy.
///
/// Stored on `VkPSO::textures`.
/// Layout uses `ePushDescriptorKHR` when `mode` is `Push`.
/// </summary>
struct TextureDescriptorSetState
{
	vk::DescriptorSetLayout layout;
	uint32_t setIndex = 0;
	uint32_t descriptorCount = 0;
	TextureDescriptorBindingMode mode = TextureDescriptorBindingMode::AllocatedSet;
};

#endif // defined(WZ_VULKAN_ENABLED)
