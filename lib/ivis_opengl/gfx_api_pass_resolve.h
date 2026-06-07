#pragma once

namespace gfx_api
{

struct RenderPassDesc;

/// Backend execution route inferred from resolved attachments.
enum class ResolvedPassRoute
{
	Swapchain,
	DepthCascade,
	SceneFramebuffer,
	DynamicAttachments
};

/// Resolve legacy presets, attachment sources, transient allocations, and viewport dimensions.
/// All passes share this single path; factories set depthCascadeIndex / sceneFramebuffer / attachments.
bool resolvePassDescription(RenderPassDesc& pass);

/// Select the backend execution path after resolvePassDescription().
ResolvedPassRoute routeResolvedPass(const RenderPassDesc& pass);

/// True when a resolved swapchain pass can share an open swapchain render pass batch.
/// Clear load ops start a new batch (Vulkan render pass boundary).
bool canExtendSwapchainBatch(const RenderPassDesc& pass);

} // namespace gfx_api
