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
	RenderPassDesc desc;
	desc.type = type;
	desc.debugName = debugName;
	desc.recordFunc = std::move(recordFunc);
	_render_passes.push_back(std::move(desc));
}

void RenderGraph::addDepthPass(size_t cascadeIndex, const std::string& debugName,
	RenderPassDesc::RecordFunc recordFunc)
{
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

	auto& ctx = gfx_api::context::get();
	bool defaultPassActive = false;

	for (auto& pass : _render_passes)
	{
		ctx.debugStringMarker(pass.debugName.c_str());

		switch (pass.type)
		{
		case RenderPassType::Depth:
			ctx.beginDepthPass(pass.depthPassIndex);
			if (pass.recordFunc)
			{
				pass.recordFunc();
			}
			ctx.endCurrentDepthPass();
			break;

		case RenderPassType::Scene:
			ctx.beginSceneRenderPass();
			if (pass.recordFunc)
			{
				pass.recordFunc();
			}
			ctx.endSceneRenderPass();
			break;

		case RenderPassType::Default:
			if (!defaultPassActive)
			{
				ctx.beginRenderPass();
				defaultPassActive = true;
			}
			if (pass.recordFunc)
			{
				pass.recordFunc();
			}
			break;
		}
	}

	if (defaultPassActive)
	{
		ctx.endRenderPass();
	}

	_render_passes.clear();
	_executing = false;
}

void RenderGraph::reset()
{
	_render_passes.clear();
}

} // namespace gfx_api
