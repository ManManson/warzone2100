#pragma once

namespace gfx_api
{

struct RenderPassDesc;

/// Resolve legacy presets, attachment sources, transient allocations, and viewport dimensions.
/// All pass types share this single path; RenderPassType only selects preset filling.
bool resolvePassDescription(RenderPassDesc& pass);

} // namespace gfx_api
