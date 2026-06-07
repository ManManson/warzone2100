#pragma once

#include "gfx_api_render_graph.h"

#include <vector>

namespace gfx_api
{

struct ImageBarrierOp
{
	abstract_texture* texture = nullptr;
	bool isDepth = false;
};

struct CompiledPass
{
	RenderPassDesc desc;
	size_t graphIndex = 0;
	bool skipped = false;
	std::vector<ResolvedRead> resolvedReads;
	std::vector<ImageBarrierOp> prePassBarriers;
};

class CompiledRenderGraph
{
public:
	bool compile(std::vector<RenderPassDesc>& descs);
	const std::vector<CompiledPass>& passes() const { return _passes; }

private:
	std::vector<CompiledPass> _passes;
};

/// Resolve reads for a single pass (used by execute before full compile integration).
bool resolvePassReads(const std::vector<RenderPassDesc>& descs, size_t passIndex,
	std::vector<ResolvedRead>& outReads);

RenderPassContext buildRenderPassContext(const std::vector<RenderPassDesc>& descs, size_t passIndex);
RenderPassContext buildRenderPassContext(const CompiledPass& compiledPass);

} // namespace gfx_api
