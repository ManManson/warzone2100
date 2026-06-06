#pragma once

namespace gfx_api
{

struct RenderPassDesc;

/// Resolve pass presets, transient attachments, and viewport dimensions before execution.
bool resolvePassDescription(RenderPassDesc& pass);

} // namespace gfx_api
