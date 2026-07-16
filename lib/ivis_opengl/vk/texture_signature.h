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
/** @file texture_signature.h
 * Immutable description of a PSO's shader-facing texture bindings.
 */

#pragma once

#if defined(WZ_VULKAN_ENABLED)

#include "gfx_api.h"

#include <cstdint>
#include <vector>

/// <summary>
/// One texture slot in a pipeline's descriptor set layout.
/// </summary>
struct TextureSignatureEntry
{
	uint32_t bindingId = 0;
	gfx_api::sampler_type sampler = gfx_api::sampler_type::bilinear;
	gfx_api::pixel_format_target target = gfx_api::pixel_format_target::texture_2d;
	gfx_api::border_color border = gfx_api::border_color::none;
};

/// <summary>
/// Comparable layout key derived from `gfx_api::texture_input` entries.
///
/// Used by the texture binding planner.
/// </summary>
struct TextureSignature
{
	std::vector<TextureSignatureEntry> entries;

	uint32_t descriptorCount() const;

	static TextureSignature from(const std::vector<gfx_api::texture_input>& textureDesc);
};

inline bool operator==(const TextureSignatureEntry& lhs, const TextureSignatureEntry& rhs)
{
	return lhs.bindingId == rhs.bindingId
		&& lhs.sampler == rhs.sampler
		&& lhs.target == rhs.target
		&& lhs.border == rhs.border;
}

inline bool operator!=(const TextureSignatureEntry& lhs, const TextureSignatureEntry& rhs)
{
	return !(lhs == rhs);
}

inline bool operator==(const TextureSignature& lhs, const TextureSignature& rhs)
{
	return lhs.entries == rhs.entries;
}

inline bool operator!=(const TextureSignature& lhs, const TextureSignature& rhs)
{
	return !(lhs == rhs);
}

#endif // defined(WZ_VULKAN_ENABLED)
