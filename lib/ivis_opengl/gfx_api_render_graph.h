#pragma once

#include "gfx_api_attachment.h"

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>
#include <string>

#include <nonstd/optional.hpp>
using nonstd::optional;
using nonstd::nullopt;

namespace gfx_api
{

/// Describes what a render pass needs to do.
struct RenderPassDesc
{
	std::string debugName;

	/// When set, resolve fills the depth shadow-map attachment for this cascade index.
	optional<size_t> depthCascadeIndex;

	/// When true, resolve fills scene color + backend-internal depth attachments.
	bool sceneFramebuffer = false;

	// Records draw calls for this pass. Must only invoke gfx_api draw/bind APIs.
	// When called, executeRenderGraph() is active and the backend has configured
	// the pass (command buffer, framebuffer, viewport, etc.).
	using RecordFunc = std::function<void()>;
	RecordFunc recordFunc;

	std::vector<AttachmentDesc> colorAttachments;
	optional<AttachmentDesc> depthAttachment;
	optional<AttachmentDesc> resolveAttachment;
	optional<std::pair<uint32_t, uint32_t>> viewportSize;
	std::vector<abstract_texture*> inputTextures;
};

/// Fluent builder for RenderPassDesc. Returns a pass description by value;
/// call build() on an rvalue to finalize (move-only).
class RenderPassBuilder
{
public:
	static RenderPassBuilder create(const std::string& debugName);

	RenderPassBuilder& depthCascadeIndex(size_t idx);
	RenderPassBuilder& sceneFramebuffer();

	RenderPassBuilder& colorAttachment(abstract_texture* tex, AttachmentLoadOp loadOp,
		ClearValue clearValue = ClearValue::colorClear());
	RenderPassBuilder& colorAttachment(abstract_texture* tex, bool clear);
	RenderPassBuilder& transientColorAttachment(AttachmentLoadOp loadOp = AttachmentLoadOp::Clear,
		ClearValue clearValue = ClearValue::colorClear());
	RenderPassBuilder& swapchainAttachment(AttachmentLoadOp loadOp = AttachmentLoadOp::Load,
		ClearValue clearValue = ClearValue::colorClear());

	RenderPassBuilder& depthAttachment(abstract_texture* tex, AttachmentLoadOp loadOp,
		ClearValue clearValue = ClearValue::depthStencilClear());
	RenderPassBuilder& depthAttachment(abstract_texture* tex, bool clear);

	RenderPassBuilder& resolveAttachment(abstract_texture* tex,
		AttachmentLoadOp loadOp = AttachmentLoadOp::DontCare,
		ClearValue clearValue = ClearValue::colorClear());

	RenderPassBuilder& viewport(uint32_t w, uint32_t h);
	RenderPassBuilder& inputTexture(abstract_texture* tex);
	RenderPassBuilder& record(RenderPassDesc::RecordFunc func);

	RenderPassDesc build() &&;

private:
	explicit RenderPassBuilder(std::string debugName);

	RenderPassDesc _desc;
};

/// The render graph abstraction: accumulates render pass descriptions,
/// then executes them all in sequence.
class RenderGraph
{
public:

	RenderGraph() = default;
	~RenderGraph() = default;

	RenderGraph(const RenderGraph&) = delete;
	RenderGraph& operator=(const RenderGraph&) = delete;

	void addRenderPass(RenderPassDesc desc);

	void execute();
	void reset();

	size_t passCount() const { return _render_passes.size(); }

private:

	std::vector<RenderPassDesc> _render_passes;
	bool _executing = false;
};

RenderPassDesc makeDepthCascadePass(size_t cascadeIndex, const std::string& debugName,
	RenderPassDesc::RecordFunc recordFunc);
RenderPassDesc makeScenePass(const std::string& debugName, RenderPassDesc::RecordFunc recordFunc);
RenderPassDesc makeSwapchainPass(const std::string& debugName, AttachmentLoadOp loadOp,
	RenderPassDesc::RecordFunc recordFunc);

} // namespace gfx_api
