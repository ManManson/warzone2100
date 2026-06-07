#pragma once

#include "gfx_api_attachment.h"
#include "gfx_api_render_graph.h"

#include <nonstd/optional.hpp>

namespace gfx_api
{

struct RenderPassDesc;

/// Backend execution route inferred from resolved attachments.
enum class ResolvedPassRoute
{
	Swapchain,
	DynamicAttachments
};

struct PassOutputView
{
	abstract_texture* texture = nullptr;
	uint32_t arrayLayer = 0;
	uint32_t mipLevel = 0;
	bool isDepth = false;
	bool isMultisampled = false;
};

/// Resolve attachment sources, transient allocations, and viewport dimensions.
/// All passes share this single path; factories set explicit attachments.
bool resolvePassDescription(RenderPassDesc& pass);

/// Select the backend execution path after resolvePassDescription().
ResolvedPassRoute routeResolvedPass(const RenderPassDesc& pass);

/// True when a resolved swapchain pass can share an open swapchain render pass batch.
/// Clear load ops start a new batch (Vulkan render pass boundary).
bool canExtendSwapchainBatch(const RenderPassDesc& pass);

/// Load op of the first swapchain color attachment, if any.
nonstd::optional<AttachmentLoadOp> getSwapchainColorLoadOp(const RenderPassDesc& pass);

/// True when the pass has no color attachments and a resolved depth attachment.
bool passIsDepthOnly(const RenderPassDesc& pass);

/// True when resolveAttachment is set and the first color attachment is multisampled.
bool passNeedsMsaaResolve(const RenderPassDesc& pass);

/// True when the resolved depth attachment includes a stencil component (scene depth, not shadow map).
bool attachmentDepthHasStencil(const AttachmentDesc& attachment);

/// Resolve a producer pass output attachment for readPassOutput().
nonstd::optional<PassOutputView> getPassOutputAttachment(
	const RenderPassDesc& producer,
	AttachmentRole role,
	uint32_t colorIndex = 0);

} // namespace gfx_api
