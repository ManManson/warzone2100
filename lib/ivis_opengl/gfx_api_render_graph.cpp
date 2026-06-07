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

RenderPassBuilder RenderPassBuilder::create(const std::string& debugName)
{
	return RenderPassBuilder(debugName);
}

RenderPassBuilder::RenderPassBuilder(std::string debugName)
{
	_desc.debugName = std::move(debugName);
}

RenderPassBuilder& RenderPassBuilder::depthCascadeIndex(size_t idx)
{
	_desc.depthCascadeIndex = idx;
	return *this;
}

RenderPassBuilder& RenderPassBuilder::sceneFramebuffer()
{
	_desc.sceneFramebuffer = true;
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
	pass.depthCascadeIndex = cascadeIndex;
	pass.recordFunc = std::move(recordFunc);
	return pass;
}

RenderPassDesc makeScenePass(const std::string& debugName, RenderPassDesc::RecordFunc recordFunc)
{
	RenderPassDesc pass;
	pass.debugName = debugName;
	pass.sceneFramebuffer = true;
	pass.recordFunc = std::move(recordFunc);
	return pass;
}

RenderPassDesc makeSwapchainPass(const std::string& debugName, AttachmentLoadOp loadOp,
	RenderPassDesc::RecordFunc recordFunc)
{
	RenderPassDesc pass;
	pass.debugName = debugName;
	pass.colorAttachments.push_back(AttachmentDesc::swapchain(loadOp));
	pass.recordFunc = std::move(recordFunc);
	return pass;
}

} // namespace gfx_api
