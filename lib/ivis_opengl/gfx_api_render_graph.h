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

/// <summary>
/// Identifies a logical render pass within the graph.
/// Backends map this to their internal pass representation.
/// </summary>
enum class RenderPassType
{
	Depth,    // Shadow map cascade
	Scene,    // Offscreen 3D scene
	Default,  // Swapchain / compositing + UI
	Custom    // Declarative attachments (color/depth/viewport on RenderPassDesc)
};

/// <summary>
/// Describes what a render pass needs to do.
/// </summary>
struct RenderPassDesc
{
	std::string debugName;
	RenderPassType type = RenderPassType::Default;

	// For Depth passes: which cascade index
	size_t depthPassIndex = 0;

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

/// <summary>
/// Fluent builder for RenderPassDesc. Returns a pass description by value;
/// call build() on an rvalue to finalize (move-only).
/// </summary>
class RenderPassBuilder
{
public:
	static RenderPassBuilder create(const std::string& debugName);

	RenderPassBuilder& type(RenderPassType t);
	RenderPassBuilder& depthIndex(size_t idx);

	RenderPassBuilder& colorAttachment(abstract_texture* tex, AttachmentLoadOp loadOp,
		ClearValue clearValue = ClearValue::colorClear());
	RenderPassBuilder& colorAttachment(abstract_texture* tex, bool clear);

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

/// <summary>
/// The render graph abstraction: accumulates render pass descriptions,
/// then executes them all in sequence.
/// </summary>
class RenderGraph
{
public:

	RenderGraph() = default;
	~RenderGraph() = default;

	RenderGraph(const RenderGraph&) = delete;
	RenderGraph& operator=(const RenderGraph&) = delete;

	// Add a render pass to the graph.
	// Passes execute in the order they are added.
	void addRenderPass(RenderPassDesc desc);

	// Convenience overload: add a pass with just a type, name, and record function.
	void addRenderPass(RenderPassType type, const std::string& debugName,
						RenderPassDesc::RecordFunc recordFunc);

	// Convenience overload for depth passes that need a cascade index.
	void addDepthPass(size_t cascadeIndex, const std::string& debugName,
						RenderPassDesc::RecordFunc recordFunc);

	// Execute all accumulated passes in order, then clear the pass list.
	// This drives the backend through its begin/end pass transitions.
	// Manages the default render pass lifecycle:
	//   - Calls beginPass(Default) before the first Default pass.
	//   - Calls endPass()+submitFrame() after all passes are done (if the default pass was started).
	void execute();

	// Reset the graph for a new frame. Clears all accumulated passes.
	void reset();

	// Query how many passes are currently queued.
	size_t passCount() const { return _render_passes.size(); }

private:

	std::vector<RenderPassDesc> _render_passes;
	bool _executing = false;
};

} // namespace gfx_api
