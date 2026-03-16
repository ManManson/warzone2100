#include "gfx_api_render_graph.h"

#include "gfx_api.h"

#include <string>

namespace gfx_api
{

void RenderGraph::addRenderPass(RenderPassDesc desc)
{
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
	auto& ctx = gfx_api::context::get();

	for (auto& pass : _render_passes)
	{
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
			// Default pass is already started by beginRenderPass()
			// (called from pie_ScreenFrameRenderBegin).
			// The record function just draws into the current default pass.
			if (pass.recordFunc)
			{
				pass.recordFunc();
			}
			break;
		}
	}

	_render_passes.clear();
}

void RenderGraph::clear()
{
	_render_passes.clear();
}

} // namespace gfx_api
