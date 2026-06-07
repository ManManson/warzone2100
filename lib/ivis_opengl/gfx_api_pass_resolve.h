#pragma once

namespace gfx_api
{

struct RenderPassDesc;

/// Backend execution route inferred from resolved attachments (not RenderPassType).
enum class ResolvedPassRoute
{
	Swapchain,
	DepthCascade,
	SceneFramebuffer,
	DynamicAttachments
};

/// Resolve legacy presets, attachment sources, transient allocations, and viewport dimensions.
/// All pass types share this single path; RenderPassType only selects preset filling.
bool resolvePassDescription(RenderPassDesc& pass);

/// Select the backend execution path after resolvePassDescription().
ResolvedPassRoute routeResolvedPass(const RenderPassDesc& pass);

} // namespace gfx_api
