#include "gfx_api_pass_resolve.h"

#include "gfx_api.h"
#include "gfx_api_pipeline_surfaces.h"
#include "gfx_api_render_graph.h"

#include <algorithm>

namespace gfx_api
{

namespace
{

bool attachmentIsResolved(const AttachmentDesc& attachment)
{
	switch (attachment.source)
	{
	case AttachmentSource::Texture:
		return attachment.texture != nullptr;
	case AttachmentSource::Transient:
	case AttachmentSource::Swapchain:
		return true;
	}
	return false;
}

bool passHasResolvedAttachments(const RenderPassDesc& pass)
{
	for (const auto& colorAttachment : pass.colorAttachments)
	{
		if (attachmentIsResolved(colorAttachment))
		{
			return true;
		}
	}
	return pass.depthAttachment.has_value() && attachmentIsResolved(pass.depthAttachment.value());
}

bool passHasSwapchainColorAttachment(const RenderPassDesc& pass)
{
	return std::any_of(pass.colorAttachments.begin(), pass.colorAttachments.end(),
		[](const AttachmentDesc& attachment) { return attachment.isSwapchain(); });
}

void tryInferPassDimensions(RenderPassDesc& pass, gfx_api::context& ctx, uint32_t& width, uint32_t& height)
{
	if (pass.viewportSize.has_value())
	{
		width = pass.viewportSize->first;
		height = pass.viewportSize->second;
		if (width > 0 && height > 0)
		{
			return;
		}
	}

	auto tryFromTexture = [&](gfx_api::abstract_texture* texture) {
		if ((width > 0 && height > 0) || texture == nullptr)
		{
			return;
		}
		const auto dims = ctx.getRenderTargetDimensions(texture);
		if (dims.has_value())
		{
			width = dims->first;
			height = dims->second;
		}
	};

	for (const auto& colorAttachment : pass.colorAttachments)
	{
		if (!colorAttachment.isSwapchain())
		{
			tryFromTexture(colorAttachment.texture);
		}
	}
	if (pass.depthAttachment.has_value())
	{
		tryFromTexture(pass.depthAttachment->texture);
	}
	if (pass.resolveAttachment.has_value())
	{
		tryFromTexture(pass.resolveAttachment->texture);
	}

	if (width == 0 || height == 0)
	{
		const auto drawable = ctx.getDrawableDimensions();
		if (drawable.first > 0 && drawable.second > 0)
		{
			width = drawable.first;
			height = drawable.second;
		}
	}

	if (width > 0 && height > 0)
	{
		pass.viewportSize = std::make_pair(width, height);
	}
}

bool resolveTransientAttachment(gfx_api::context& ctx, AttachmentDesc& attachment,
	uint32_t width, uint32_t height, pixel_format format)
{
	if (!attachment.isTransient())
	{
		return true;
	}
	if (attachment.texture != nullptr)
	{
		return true;
	}

	attachment.source = AttachmentSource::Texture;
	attachment.texture = ctx.acquireTransientRenderTarget(format, width, height);
	return attachment.texture != nullptr;
}

void applySwapchainViewport(RenderPassDesc& pass, gfx_api::context& ctx)
{
	const auto drawable = ctx.getDrawableDimensions();
	if (!pass.viewportSize.has_value() && drawable.first > 0 && drawable.second > 0)
	{
		pass.viewportSize = drawable;
	}
}

void resolveMissingAttachments(RenderPassDesc& pass, gfx_api::context& ctx)
{
	if (passHasSwapchainColorAttachment(pass))
	{
		applySwapchainViewport(pass, ctx);
	}
}

bool resolveTransientAttachments(RenderPassDesc& pass, gfx_api::context& ctx, uint32_t width, uint32_t height)
{
	const auto colorFormat = pixel_format::FORMAT_RGBA8_UNORM_PACK8;

	for (auto& colorAttachment : pass.colorAttachments)
	{
		if (!resolveTransientAttachment(ctx, colorAttachment, width, height, colorFormat))
		{
			return false;
		}
	}

	if (pass.depthAttachment.has_value() && pass.depthAttachment->isTransient())
	{
		const auto depthFormat = ctx.getDepthStencilFormat();
		ASSERT_OR_RETURN(false, depthFormat != pixel_format::invalid,
			"Transient depth attachment on pass \"%s\" requires a valid depth/stencil format",
			pass.debugName.c_str());
		if (!resolveTransientAttachment(ctx, pass.depthAttachment.value(), width, height, depthFormat))
		{
			return false;
		}
	}

	if (pass.resolveAttachment.has_value())
	{
		if (!resolveTransientAttachment(ctx, pass.resolveAttachment.value(), width, height, colorFormat))
		{
			return false;
		}
	}

	return true;
}

} // namespace

bool resolvePassDescription(RenderPassDesc& pass)
{
	auto& ctx = gfx_api::context::get();

	resolveMissingAttachments(pass, ctx);

	uint32_t width = 0;
	uint32_t height = 0;
	tryInferPassDimensions(pass, ctx, width, height);

	ASSERT_OR_RETURN(false, passHasResolvedAttachments(pass),
		"Pass \"%s\" requires at least one resolved color or depth attachment", pass.debugName.c_str());
	ASSERT_OR_RETURN(false, width > 0 && height > 0,
		"Pass \"%s\" requires viewportSize or inferrable attachment dimensions", pass.debugName.c_str());

	const ResolvedPassRoute route = routeResolvedPass(pass);
	if (route == ResolvedPassRoute::Swapchain)
	{
		ASSERT_OR_RETURN(false, passHasSwapchainColorAttachment(pass),
			"Swapchain pass \"%s\" must declare a swapchain color attachment", pass.debugName.c_str());
	}
	if (!resolveTransientAttachments(pass, ctx, width, height))
	{
		ASSERT(false, "Failed to resolve transient attachments for pass \"%s\"", pass.debugName.c_str());
		return false;
	}

	return true;
}

nonstd::optional<AttachmentLoadOp> getSwapchainColorLoadOp(const RenderPassDesc& pass)
{
	for (const auto& colorAttachment : pass.colorAttachments)
	{
		if (colorAttachment.isSwapchain())
		{
			return colorAttachment.loadOp;
		}
	}
	return nonstd::nullopt;
}

bool canExtendSwapchainBatch(const RenderPassDesc& pass)
{
	const auto loadOp = getSwapchainColorLoadOp(pass);
	return routeResolvedPass(pass) == ResolvedPassRoute::Swapchain
		&& loadOp.has_value()
		&& loadOp.value() != AttachmentLoadOp::Clear;
}

ResolvedPassRoute routeResolvedPass(const RenderPassDesc& pass)
{
	if (passHasSwapchainColorAttachment(pass))
	{
		return ResolvedPassRoute::Swapchain;
	}

	return ResolvedPassRoute::DynamicAttachments;
}

bool passIsDepthOnly(const RenderPassDesc& pass)
{
	return pass.colorAttachments.empty()
		&& pass.depthAttachment.has_value()
		&& pass.depthAttachment->texture != nullptr;
}

bool passNeedsMsaaResolve(const RenderPassDesc& pass)
{
	if (!pass.resolveAttachment.has_value() || pass.resolveAttachment->texture == nullptr)
	{
		return false;
	}
	if (pass.colorAttachments.empty() || pass.colorAttachments[0].texture == nullptr)
	{
		return false;
	}
	return gfx_api::context::get().isMultisampledColorAttachment(pass.colorAttachments[0].texture);
}

bool attachmentDepthHasStencil(const AttachmentDesc& attachment)
{
	if (attachment.texture == nullptr)
	{
		return false;
	}
	auto& ctx = gfx_api::context::get();
	// Shadow map is depth-component only; scene depth and transient depth use D24S8.
	return attachment.texture != ctx.getPipelineSurface(PipelineSurfaceId::ShadowMap);
}

} // namespace gfx_api
