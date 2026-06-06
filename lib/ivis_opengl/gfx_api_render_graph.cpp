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
	addRenderPass(
		std::move(
			RenderPassBuilder::create(debugName)
				.type(type)
				.record(std::move(recordFunc)))
			.build());
}

void RenderGraph::addDepthPass(size_t cascadeIndex, const std::string& debugName,
	RenderPassDesc::RecordFunc recordFunc)
{
	addRenderPass(
		std::move(
			RenderPassBuilder::create(debugName)
				.type(RenderPassType::Depth)
				.depthIndex(cascadeIndex)
				.record(std::move(recordFunc)))
			.build());
}

RenderPassBuilder RenderPassBuilder::create(const std::string& debugName)
{
	return RenderPassBuilder(debugName);
}

RenderPassBuilder::RenderPassBuilder(std::string debugName)
	: _desc{std::move(debugName), RenderPassType::Default, 0, {}}
{
}

RenderPassBuilder& RenderPassBuilder::type(RenderPassType t)
{
	_desc.type = t;
	return *this;
}

RenderPassBuilder& RenderPassBuilder::depthIndex(size_t idx)
{
	_desc.depthPassIndex = idx;
	return *this;
}

RenderPassBuilder& RenderPassBuilder::record(RenderPassDesc::RecordFunc func)
{
	_desc.recordFunc = std::move(func);
	return *this;
}

RenderPassDesc RenderPassBuilder::build() &&
{
	return std::move(_desc);
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
