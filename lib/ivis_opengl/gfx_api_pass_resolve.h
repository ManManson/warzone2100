#pragma once

namespace gfx_api
{

struct RenderPassDesc;

/// Resolve pass presets, transient attachments, and viewport dimensions before execution.
/// Returns false if the pass cannot be executed.
bool resolvePassDescription(RenderPassDesc& pass);

} // namespace gfx_api
