#pragma once

#include <array>
#include <cstdint>

#include <nonstd/optional.hpp>

namespace gfx_api
{

struct abstract_texture;

/// Load operation for a render pass attachment
enum class AttachmentLoadOp
{
	Clear,
	Load,
	DontCare
};

/// Store operation for a render pass attachment (post-pass contract).
enum class AttachmentStoreOp : uint8_t
{
	Store,      // keep contents (shadow depth, resolve output, transient scratch)
	DontCare,   // contents not needed after pass (MSAA intermediate color)
	Resolve,    // MSAA color → resolve attachment (VK subpass; GL blit target)
	Invalidate, // tile-friendly discard (scene depth/stencil after scene pass)
};

/// Clear values for color and depth/stencil attachments.
struct ClearValue
{
	std::array<float, 4> color = {0.f, 0.f, 0.f, 1.f};
	float depth = 1.f;
	uint32_t stencil = 0;

	static ClearValue colorClear(float r = 0.f, float g = 0.f, float b = 0.f, float a = 1.f)
	{
		ClearValue v;
		v.color = {r, g, b, a};
		return v;
	}

	static ClearValue depthStencilClear(float depthValue = 1.f, uint32_t stencilValue = 0)
	{
		ClearValue v;
		v.depth = depthValue;
		v.stencil = stencilValue;
		return v;
	}
};

/// Where an attachment's backing storage comes from at execute time.
enum class AttachmentSource
{
	Texture,          // explicit abstract_texture* or pipeline surface
	Transient,        // per-frame cache allocation (texture resolved at execute)
	Swapchain         // current drawable / swapchain image
};

/// Describes a color, depth, or resolve attachment for a render pass.
struct AttachmentDesc
{
	AttachmentSource source = AttachmentSource::Texture;
	abstract_texture* texture = nullptr;
	AttachmentLoadOp loadOp = AttachmentLoadOp::Clear;
	/// When unset, resolved by applyDefaultAttachmentStoreOps() during pass resolution.
	nonstd::optional<AttachmentStoreOp> storeOp;
	ClearValue clearValue;
	uint32_t mipLevel = 0;
	uint32_t arrayLayer = 0;

	bool shouldClear() const
	{
		return loadOp == AttachmentLoadOp::Clear;
	}

	bool isTransient() const
	{
		return source == AttachmentSource::Transient;
	}

	bool isSwapchain() const
	{
		return source == AttachmentSource::Swapchain;
	}

	static AttachmentDesc color(abstract_texture* tex, AttachmentLoadOp op = AttachmentLoadOp::Clear,
		ClearValue clear = ClearValue::colorClear())
	{
		AttachmentDesc desc;
		desc.source = AttachmentSource::Texture;
		desc.texture = tex;
		desc.loadOp = op;
		desc.clearValue = clear;
		return desc;
	}

	static AttachmentDesc depth(abstract_texture* tex, AttachmentLoadOp op = AttachmentLoadOp::Clear,
		ClearValue clear = ClearValue::depthStencilClear())
	{
		AttachmentDesc desc;
		desc.source = AttachmentSource::Texture;
		desc.texture = tex;
		desc.loadOp = op;
		desc.clearValue = clear;
		return desc;
	}

	/// Per-frame pooled color target; texture is filled in during pass resolution.
	static AttachmentDesc transientColor(AttachmentLoadOp op = AttachmentLoadOp::Clear,
		ClearValue clear = ClearValue::colorClear())
	{
		AttachmentDesc desc;
		desc.source = AttachmentSource::Transient;
		desc.loadOp = op;
		desc.clearValue = clear;
		return desc;
	}

	/// Per-frame pooled depth/stencil target; texture is filled in during pass resolution.
	static AttachmentDesc transientDepth(AttachmentLoadOp op = AttachmentLoadOp::Clear,
		ClearValue clear = ClearValue::depthStencilClear())
	{
		AttachmentDesc desc;
		desc.source = AttachmentSource::Transient;
		desc.loadOp = op;
		desc.clearValue = clear;
		return desc;
	}

	/// Current swapchain / default framebuffer color target.
	static AttachmentDesc swapchain(AttachmentLoadOp op = AttachmentLoadOp::Load,
		ClearValue clear = ClearValue::colorClear())
	{
		AttachmentDesc desc;
		desc.source = AttachmentSource::Swapchain;
		desc.loadOp = op;
		desc.clearValue = clear;
		return desc;
	}

	/// Legacy bool-clear shim: true → Clear, false → Load.
	static AttachmentDesc fromLegacyClear(abstract_texture* tex, bool clear)
	{
		return color(tex, clear ? AttachmentLoadOp::Clear : AttachmentLoadOp::Load);
	}
};

} // namespace gfx_api
