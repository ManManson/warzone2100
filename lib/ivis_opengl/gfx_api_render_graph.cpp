#include "gfx_api_render_graph.h"

#include "gfx_api.h"

#include <string>

namespace gfx_api
{

void RenderGraph::addRenderPass(RenderPassDesc desc)
{
	ASSERT(!_executing, "Cannot add passes during execute()");
	_render_passes.push_back(std::move(desc));
}

void RenderGraph::addRenderPass(RenderPassType type, const std::string& debugName,
	RenderPassDesc::RecordFunc recordFunc)
{
	ASSERT(!_executing, "Cannot add passes during execute()");
	RenderPassDesc desc;
	desc.type = type;
	desc.debugName = debugName;
	desc.recordFunc = std::move(recordFunc);
	_render_passes.push_back(std::move(desc));
}

void RenderGraph::addDepthPass(size_t cascadeIndex, const std::string& debugName,
	RenderPassDesc::RecordFunc recordFunc)
{
	ASSERT(!_executing, "Cannot add passes during execute()");
	RenderPassDesc desc;
	desc.type = RenderPassType::Depth;
	desc.depthPassIndex = cascadeIndex;
	desc.debugName = debugName;
	desc.recordFunc = std::move(recordFunc);
	_render_passes.push_back(std::move(desc));
}

void RenderGraph::execute()
{
	ASSERT(!_executing, "Re-entrant execute() call detected");
	_executing = true;

	gfx_api::context::get().executeRenderGraph(_render_passes);

	reset();
	_executing = false;
}

void RenderGraph::reset()
{
	_render_passes.clear();
}

} // namespace gfx_api
