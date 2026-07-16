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
/** @file texture_descriptor_writer.h
 * Builds `vk::WriteDescriptorSet` entries for texture binding.
 */

#pragma once

#if defined(WZ_VULKAN_ENABLED)

#include "gfx_api.h"
#include "vk/texture_descriptor_types.h"
#include "vk/vulkan_hpp_include.h"

#include <vector>

/// <summary>
/// Fallback image views used when a texture slot is null or unresolved.
/// </summary>
struct DefaultTextureViews
{
	vk::ImageView texture2D;
	vk::ImageView texture2DArray;
	vk::ImageView depthMap;
};

/// <summary>
/// Shared write construction for push and allocated-set texture binding.
/// </summary>
struct TextureDescriptorWriter
{
	static const std::vector<vk::WriteDescriptorSet>& build(
		TextureDescriptorWriteBatch& scratch,
		const std::vector<gfx_api::texture_input>& descriptions,
		const std::vector<gfx_api::abstract_texture*>& textures,
		vk::DescriptorSet dstSetForWrites,
		TextureSamplerMode samplerMode,
		const DefaultTextureViews& defaults);
};

#endif // defined(WZ_VULKAN_ENABLED)
