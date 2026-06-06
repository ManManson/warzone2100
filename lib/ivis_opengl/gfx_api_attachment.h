#pragma once

#include <array>
#include <cstdint>

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

/// Describes a color, depth, or resolve attachment for a render pass.
/// texture == nullptr requests a transient allocation at execute time.
struct AttachmentDesc
{
	abstract_texture* texture = nullptr;
	AttachmentLoadOp loadOp = AttachmentLoadOp::Clear;
	ClearValue clearValue;
	uint32_t mipLevel = 0;
	uint32_t arrayLayer = 0;

	bool shouldClear() const
	{
		return loadOp == AttachmentLoadOp::Clear;
	}

	static AttachmentDesc color(abstract_texture* tex, AttachmentLoadOp op = AttachmentLoadOp::Clear,
		ClearValue clear = ClearValue::colorClear())
	{
		AttachmentDesc desc;
		desc.texture = tex;
		desc.loadOp = op;
		desc.clearValue = clear;
		return desc;
	}

	static AttachmentDesc depth(abstract_texture* tex, AttachmentLoadOp op = AttachmentLoadOp::Clear,
		ClearValue clear = ClearValue::depthStencilClear())
	{
		AttachmentDesc desc;
		desc.texture = tex;
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
