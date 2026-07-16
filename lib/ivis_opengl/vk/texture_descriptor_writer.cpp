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
/** @file texture_descriptor_writer.cpp
 * Implementation of `TextureDescriptorWriter`.
 */

#if defined(WZ_VULKAN_ENABLED)

#include "vk/texture_descriptor_writer.h"

#include "gfx_api_vk.h"
#include "vk/vulkan_backend_texture_type.h"
#include "lib/framework/wzapp.h"

const std::vector<vk::WriteDescriptorSet>& TextureDescriptorWriter::build(
	TextureDescriptorWriteBatch& scratch,
	const std::vector<gfx_api::texture_input>& descriptions,
	const std::vector<gfx_api::abstract_texture*>& textures,
	vk::DescriptorSet dstSetForWrites,
	TextureSamplerMode samplerMode,
	const DefaultTextureViews& defaults)
{
	ASSERT(samplerMode == TextureSamplerMode::Immutable,
		"Dynamic texture samplers are not implemented");

	scratch.prepare(textures.size());

	for (size_t i = 0; i < textures.size(); ++i)
	{
		auto* texture = textures[i];
		vk::ImageView imageView;
		vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		if (texture != nullptr)
		{
			auto texture_type = static_cast<VulkanBackendInternalTextureType>(texture->backend_internal_value());
			auto target_type = descriptions.at(i).target;
			switch (texture_type)
			{
				case VulkanBackendInternalTextureType::Texture:
					ASSERT(target_type == gfx_api::pixel_format_target::texture_2d, "Unexpected target type: (%d)", static_cast<int>(target_type));
					imageView = static_cast<VkTexture*>(texture)->view.get();
					break;
				case VulkanBackendInternalTextureType::TextureArray:
					ASSERT(target_type == gfx_api::pixel_format_target::texture_2d_array, "Unexpected target type: (%d)", static_cast<int>(target_type));
					imageView = static_cast<VkTextureArray*>(texture)->view.get();
					break;
				case VulkanBackendInternalTextureType::DepthMap:
					ASSERT(target_type == gfx_api::pixel_format_target::depth_map, "Unexpected target type: (%d)", static_cast<int>(target_type));
					imageView = static_cast<VkDepthMapImage*>(texture)->view.get();
					break;
				case VulkanBackendInternalTextureType::RenderedImage:
					ASSERT(target_type == gfx_api::pixel_format_target::texture_2d, "Unexpected target type: (%d)", static_cast<int>(target_type));
					imageView = static_cast<VkRenderedImage*>(texture)->view.get();
					break;
				case VulkanBackendInternalTextureType::AttachmentImage:
					ASSERT(target_type == gfx_api::pixel_format_target::texture_2d, "Unexpected target type: (%d)", static_cast<int>(target_type));
					imageView = static_cast<VkAttachmentImage*>(texture)->view;
					break;
				case VulkanBackendInternalTextureType::SwapchainColorSurface:
				case VulkanBackendInternalTextureType::SwapchainMsaaColorSurface:
				case VulkanBackendInternalTextureType::SwapchainDepthSurface:
					debug(LOG_FATAL, "Swapchain pipeline surfaces are not shader-sampled");
					break;
				case VulkanBackendInternalTextureType::Invalid:
					debug(LOG_FATAL, "Invalid internal texture type??");
					break;
			}
		}

		if (!imageView)
		{
			switch (descriptions.at(i).target)
			{
				case gfx_api::pixel_format_target::texture_2d:
					imageView = defaults.texture2D;
					break;
				case gfx_api::pixel_format_target::depth_map:
					imageView = defaults.depthMap;
					break;
				case gfx_api::pixel_format_target::texture_2d_array:
					imageView = defaults.texture2DArray;
					break;
			}
		}

		switch (descriptions.at(i).target)
		{
			case gfx_api::pixel_format_target::texture_2d:
			case gfx_api::pixel_format_target::texture_2d_array:
				// use the default: vk::ImageLayout::eShaderReadOnlyOptimal
				break;
			case gfx_api::pixel_format_target::depth_map:
				imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
				break;
		}

		scratch.imageInfos.emplace_back(vk::DescriptorImageInfo()
			.setImageView(imageView)
			.setImageLayout(imageLayout));
	}

	for (size_t i = 0; i < textures.size(); ++i)
	{
		scratch.writes.emplace_back(
			vk::WriteDescriptorSet()
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDstSet(dstSetForWrites)
				.setPImageInfo(&scratch.imageInfos[i])
				.setDstBinding(static_cast<uint32_t>(descriptions[i].id))
		);
	}

	return scratch.writes;
}

#endif // defined(WZ_VULKAN_ENABLED)
