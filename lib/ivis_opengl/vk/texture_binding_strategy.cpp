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
/** @file texture_binding_strategy.cpp
 * Implementation of texture binding mode selection and policy helpers.
 */

#if defined(WZ_VULKAN_ENABLED)

#include "vk/texture_binding_strategy.h"

TextureBindingDecision TextureBindingPlanner::select(
	const TextureSignature& signature,
	const PushDescriptorSupport& caps)
{
	const uint32_t descriptorCount = signature.descriptorCount();
	if (caps.usableFor(descriptorCount))
	{
		return { TextureDescriptorBindingMode::Push, caps.reasonFor(descriptorCount) };
	}
	return { TextureDescriptorBindingMode::AllocatedSet, caps.reasonFor(descriptorCount) };
}

vk::DescriptorSetLayoutCreateFlags textureLayoutFlagsFor(TextureDescriptorBindingMode mode)
{
	if (mode == TextureDescriptorBindingMode::Push)
	{
		return vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
	}
	return vk::DescriptorSetLayoutCreateFlags {};
}

TextureSamplerMode textureSamplerModeFor(TextureDescriptorBindingMode mode)
{
	(void)mode;
	return TextureSamplerMode::Immutable;
}

const char* textureBindingModeName(TextureDescriptorBindingMode mode)
{
	switch (mode)
	{
	case TextureDescriptorBindingMode::Push:
		return "Push";
	case TextureDescriptorBindingMode::AllocatedSet:
		return "AllocatedSet";
	}
	return "Unknown";
}

#endif // defined(WZ_VULKAN_ENABLED)
