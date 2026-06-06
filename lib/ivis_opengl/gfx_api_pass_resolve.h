#pragma once

namespace gfx_api
{

struct RenderPassDesc;

/// Resolve pass presets, transient attachments, and viewport dimensions before execution.
/// For Default passes, isFirstDefaultPassInFrame selects Clear vs Load for swapchain targeting.
bool resolvePassDescription(RenderPassDesc& pass, bool isFirstDefaultPassInFrame = false);

} // namespace gfx_api
