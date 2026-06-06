#include "gfx_api_pass_resolve.h"

#include "gfx_api.h"
#include "gfx_api_render_graph.h"

namespace gfx_api
{

namespace
{

bool passHasResolvedAttachments(const RenderPassDesc& pass)
{
	if (!pass.colorAttachments.empty())
	{
		return true;
	}
	return pass.depthAttachment.has_value() && pass.depthAttachment->texture != nullptr;
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
		tryFromTexture(colorAttachment.texture);
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
	if (attachment.texture != nullptr)
	{
		return true;
	}

	attachment.texture = ctx.acquireTransientRenderTarget(format, width, height);
	return attachment.texture != nullptr;
}

void applyDepthPreset(RenderPassDesc& pass, gfx_api::context& ctx)
{
	if (pass.depthAttachment.has_value())
	{
		return;
	}

	gfx_api::abstract_texture* depthTexture = ctx.getDepthTexture();
	if (depthTexture == nullptr)
	{
		return;
	}

	pass.depthAttachment = AttachmentDesc::depth(depthTexture, AttachmentLoadOp::Clear);
	pass.depthAttachment->arrayLayer = static_cast<uint32_t>(pass.depthPassIndex);

	const size_t depthDim = ctx.getDepthPassDimensions(pass.depthPassIndex);
	if (depthDim > 0)
	{
		pass.viewportSize = std::make_pair(static_cast<uint32_t>(depthDim), static_cast<uint32_t>(depthDim));
	}
}

void applyScenePreset(RenderPassDesc& pass, gfx_api::context& ctx)
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

void applyDefaultPreset(RenderPassDesc& pass, gfx_api::context& ctx)
{
	const auto drawable = ctx.getDrawableDimensions();
	if (!pass.viewportSize.has_value() && drawable.first > 0 && drawable.second > 0)
	{
		pass.viewportSize = drawable;
	}
}

void applyTypePreset(RenderPassDesc& pass, gfx_api::context& ctx)
{
	switch (pass.type)
	{
	case RenderPassType::Depth:
		applyDepthPreset(pass, ctx);
		break;
	case RenderPassType::Scene:
		applyScenePreset(pass, ctx);
		break;
	case RenderPassType::Default:
		applyDefaultPreset(pass, ctx);
		break;
	case RenderPassType::Custom:
		break;
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

	if (pass.depthAttachment.has_value())
	{
		if (pass.depthAttachment->texture == nullptr)
		{
			ASSERT(false, "Transient depth attachments are not supported yet for pass \"%s\"", pass.debugName.c_str());
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

	applyTypePreset(pass, ctx);

	uint32_t width = 0;
	uint32_t height = 0;
	tryInferPassDimensions(pass, ctx, width, height);

	switch (pass.type)
	{
	case RenderPassType::Depth:
		ASSERT_OR_RETURN(false, pass.depthAttachment.has_value() && pass.depthAttachment->texture != nullptr,
			"Depth pass \"%s\" could not resolve depth attachment", pass.debugName.c_str());
		break;
	case RenderPassType::Scene:
		ASSERT_OR_RETURN(false, !pass.colorAttachments.empty(),
			"Scene pass \"%s\" could not resolve scene color attachment", pass.debugName.c_str());
		ASSERT_OR_RETURN(false, pass.depthAttachment.has_value(),
			"Scene pass \"%s\" requires a depth attachment (explicit or backend-internal)", pass.debugName.c_str());
		break;
	case RenderPassType::Default:
		ASSERT_OR_RETURN(false, pass.swapchainLoadOpExplicit,
			"Default pass \"%s\" must set an explicit swapchainLoadOp", pass.debugName.c_str());
		break;
	case RenderPassType::Custom:
		ASSERT_OR_RETURN(false, passHasResolvedAttachments(pass),
			"Custom pass \"%s\" requires at least one color or depth attachment", pass.debugName.c_str());
		ASSERT_OR_RETURN(false, width > 0 && height > 0,
			"Custom pass \"%s\" requires viewportSize or an attachment with known dimensions",
			pass.debugName.c_str());
		if (!resolveTransientAttachments(pass, ctx, width, height))
		{
			ASSERT(false, "Failed to resolve transient attachments for pass \"%s\"", pass.debugName.c_str());
			return false;
		}
		break;
	}

	return true;
}

} // namespace gfx_api
