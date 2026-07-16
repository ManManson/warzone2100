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
/** @file texture_signature.cpp
 * Implementation of `TextureSignature`.
 */

#if defined(WZ_VULKAN_ENABLED)

#include "vk/texture_signature.h"

uint32_t TextureSignature::descriptorCount() const
{
	// Each entry maps to one `eCombinedImageSampler` descriptor today.
	return static_cast<uint32_t>(entries.size());
}

TextureSignature TextureSignature::from(const std::vector<gfx_api::texture_input>& textureDesc)
{
	TextureSignature signature;
	signature.entries.reserve(textureDesc.size());
	for (const auto& texture : textureDesc)
	{
		TextureSignatureEntry entry;
		entry.bindingId = static_cast<uint32_t>(texture.id);
		entry.sampler = texture.sampler;
		entry.target = texture.target;
		entry.border = texture.border;
		signature.entries.push_back(entry);
	}
	return signature;
}

#endif // defined(WZ_VULKAN_ENABLED)
