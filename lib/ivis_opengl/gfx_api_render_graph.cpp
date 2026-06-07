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
	RenderPassDesc::RecordFunc recordFunc, AttachmentLoadOp swapchainLoadOp)
{
	addRenderPass(
		std::move(
			RenderPassBuilder::create(debugName)
				.type(type)
				.swapchainLoadOp(swapchainLoadOp)
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
{
	_desc.debugName = std::move(debugName);
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

RenderPassBuilder& RenderPassBuilder::colorAttachment(abstract_texture* tex, AttachmentLoadOp loadOp,
	ClearValue clearValue)
{
	_desc.colorAttachments.push_back(AttachmentDesc::color(tex, loadOp, clearValue));
	return *this;
}

RenderPassBuilder& RenderPassBuilder::colorAttachment(abstract_texture* tex, bool clear)
{
	_desc.colorAttachments.push_back(AttachmentDesc::fromLegacyClear(tex, clear));
	return *this;
}

RenderPassBuilder& RenderPassBuilder::transientColorAttachment(AttachmentLoadOp loadOp, ClearValue clearValue)
{
	_desc.colorAttachments.push_back(AttachmentDesc::transientColor(loadOp, clearValue));
	return *this;
}

RenderPassBuilder& RenderPassBuilder::swapchainAttachment(AttachmentLoadOp loadOp, ClearValue clearValue)
{
	_desc.colorAttachments.push_back(AttachmentDesc::swapchain(loadOp, clearValue));
	_desc.swapchainLoadOp = loadOp;
	_desc.swapchainLoadOpExplicit = true;
	return *this;
}

RenderPassBuilder& RenderPassBuilder::depthAttachment(abstract_texture* tex, AttachmentLoadOp loadOp,
	ClearValue clearValue)
{
	_desc.depthAttachment = AttachmentDesc::depth(tex, loadOp, clearValue);
	return *this;
}

RenderPassBuilder& RenderPassBuilder::depthAttachment(abstract_texture* tex, bool clear)
{
	_desc.depthAttachment = AttachmentDesc::depth(tex,
		clear ? AttachmentLoadOp::Clear : AttachmentLoadOp::Load);
	return *this;
}

RenderPassBuilder& RenderPassBuilder::resolveAttachment(abstract_texture* tex, AttachmentLoadOp loadOp,
	ClearValue clearValue)
{
	_desc.resolveAttachment = AttachmentDesc::color(tex, loadOp, clearValue);
	return *this;
}

RenderPassBuilder& RenderPassBuilder::viewport(uint32_t w, uint32_t h)
{
	_desc.viewportSize = std::make_pair(w, h);
	return *this;
}

RenderPassBuilder& RenderPassBuilder::inputTexture(abstract_texture* tex)
{
	_desc.inputTextures.push_back(tex);
	return *this;
}

RenderPassBuilder& RenderPassBuilder::swapchainLoadOp(AttachmentLoadOp loadOp)
{
	_desc.swapchainLoadOp = loadOp;
	_desc.swapchainLoadOpExplicit = true;
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
	gfx_api::context::get().releaseTransientRenderTargets();
	_render_passes.clear();
}

RenderPassDesc makeDepthCascadePass(size_t cascadeIndex, const std::string& debugName,
	RenderPassDesc::RecordFunc recordFunc)
{
	RenderPassDesc pass;
	pass.debugName = debugName;
	pass.type = RenderPassType::Depth;
	pass.depthPassIndex = cascadeIndex;
	pass.recordFunc = std::move(recordFunc);
	return pass;
}

RenderPassDesc makeScenePass(const std::string& debugName, RenderPassDesc::RecordFunc recordFunc)
{
	RenderPassDesc pass;
	pass.debugName = debugName;
	pass.type = RenderPassType::Scene;
	pass.recordFunc = std::move(recordFunc);
	return pass;
}

RenderPassDesc makeSwapchainPass(const std::string& debugName, AttachmentLoadOp loadOp,
	RenderPassDesc::RecordFunc recordFunc)
{
	RenderPassDesc pass;
	pass.debugName = debugName;
	pass.type = RenderPassType::Default;
	pass.colorAttachments.push_back(AttachmentDesc::swapchain(loadOp));
	pass.swapchainLoadOp = loadOp;
	pass.swapchainLoadOpExplicit = true;
	pass.recordFunc = std::move(recordFunc);
	return pass;
}

} // namespace gfx_api
