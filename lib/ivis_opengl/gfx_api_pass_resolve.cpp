#include "gfx_api_pass_resolve.h"

#include "gfx_api.h"
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
	case AttachmentSource::BackendInternal:
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
	if (pass.depthAttachment.has_value() && !pass.depthAttachment->isBackendInternal())
	{
		tryFromTexture(pass.depthAttachment->texture);
	}
	if (pass.resolveAttachment.has_value())
	{
		tryFromTexture(pass.resolveAttachment->texture);
	}

	if ((width == 0 || height == 0) && pass.depthCascadeIndex.has_value())
	{
		const size_t depthDim = ctx.getDepthPassDimensions(pass.depthCascadeIndex.value());
		if (depthDim > 0)
		{
			width = static_cast<uint32_t>(depthDim);
			height = static_cast<uint32_t>(depthDim);
		}
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

void applyDepthCascadeAttachments(RenderPassDesc& pass, gfx_api::context& ctx)
{
	if (!pass.depthCascadeIndex.has_value() || pass.depthAttachment.has_value())
	{
		return;
	}

	gfx_api::abstract_texture* depthTexture = ctx.getDepthTexture();
	if (depthTexture == nullptr)
	{
		return;
	}

	pass.depthAttachment = AttachmentDesc::depth(depthTexture, AttachmentLoadOp::Clear);
	pass.depthAttachment->arrayLayer = static_cast<uint32_t>(pass.depthCascadeIndex.value());

	const size_t depthDim = ctx.getDepthPassDimensions(pass.depthCascadeIndex.value());
	if (depthDim > 0)
	{
		pass.viewportSize = std::make_pair(static_cast<uint32_t>(depthDim), static_cast<uint32_t>(depthDim));
	}
}

void applySceneFramebufferAttachments(RenderPassDesc& pass, gfx_api::context& ctx)
{
	if (pass.colorAttachments.empty())
	{
		gfx_api::abstract_texture* sceneTexture = ctx.getSceneTexture();
		if (sceneTexture == nullptr)
		{
			return;
		}

		pass.colorAttachments.push_back(AttachmentDesc::color(sceneTexture, AttachmentLoadOp::Clear));

		const auto dims = ctx.getRenderTargetDimensions(sceneTexture);
		if (dims.has_value())
		{
			pass.viewportSize = dims;
		}
	}

	if (!pass.depthAttachment.has_value())
	{
		pass.depthAttachment = AttachmentDesc::backendInternalDepth(AttachmentLoadOp::Clear);
	}
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
	if (pass.depthCascadeIndex.has_value())
	{
		applyDepthCascadeAttachments(pass, ctx);
	}
	if (pass.sceneFramebuffer)
	{
		applySceneFramebufferAttachments(pass, ctx);
	}
	if (passHasSwapchainColorAttachment(pass))
	{
		applySwapchainViewport(pass, ctx);
	}
}

void syncSwapchainLoadOpFromAttachments(RenderPassDesc& pass)
{
	for (const auto& colorAttachment : pass.colorAttachments)
	{
		if (colorAttachment.isSwapchain())
		{
			pass.swapchainLoadOp = colorAttachment.loadOp;
			pass.swapchainLoadOpExplicit = true;
			return;
		}
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
		ASSERT(false, "Transient depth attachments are not supported yet for pass \"%s\"", pass.debugName.c_str());
		return false;
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
	syncSwapchainLoadOpFromAttachments(pass);

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
	if (route == ResolvedPassRoute::DepthCascade)
	{
		ASSERT_OR_RETURN(false, pass.depthAttachment.has_value()
			&& pass.depthAttachment->source == AttachmentSource::Texture
			&& pass.depthAttachment->texture != nullptr,
			"Depth cascade pass \"%s\" could not resolve depth attachment", pass.debugName.c_str());
	}
	if (route == ResolvedPassRoute::SceneFramebuffer)
	{
		ASSERT_OR_RETURN(false, !pass.colorAttachments.empty(),
			"Scene pass \"%s\" could not resolve scene color attachment", pass.debugName.c_str());
		ASSERT_OR_RETURN(false, pass.depthAttachment.has_value(),
			"Scene pass \"%s\" requires a depth attachment (explicit or backend-internal)",
			pass.debugName.c_str());
	}

	if (!resolveTransientAttachments(pass, ctx, width, height))
	{
		ASSERT(false, "Failed to resolve transient attachments for pass \"%s\"", pass.debugName.c_str());
		return false;
	}

	return true;
}

bool canExtendSwapchainBatch(const RenderPassDesc& pass)
{
	return routeResolvedPass(pass) == ResolvedPassRoute::Swapchain
		&& pass.swapchainLoadOp != AttachmentLoadOp::Clear;
}

ResolvedPassRoute routeResolvedPass(const RenderPassDesc& pass)
{
	if (passHasSwapchainColorAttachment(pass))
	{
		return ResolvedPassRoute::Swapchain;
	}

	const bool hasColor = !pass.colorAttachments.empty();
	const bool hasBackendDepth = pass.depthAttachment.has_value() && pass.depthAttachment->isBackendInternal();
	if (hasColor && hasBackendDepth)
	{
		return ResolvedPassRoute::SceneFramebuffer;
	}

	const bool depthOnly = !hasColor
		&& pass.depthAttachment.has_value()
		&& pass.depthAttachment->source == AttachmentSource::Texture
		&& pass.depthAttachment->texture != nullptr;
	if (depthOnly)
	{
		return ResolvedPassRoute::DepthCascade;
	}

	return ResolvedPassRoute::DynamicAttachments;
}

} // namespace gfx_api
