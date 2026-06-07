#pragma once

#include "gfx_api_attachment.h"
#include "gfx_api_pipeline_surfaces.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <nonstd/optional.hpp>
using nonstd::optional;
using nonstd::nullopt;

namespace gfx_api
{

struct abstract_texture;

using PassHandle = size_t;
static constexpr PassHandle kInvalidPassHandle = ~PassHandle(0);

enum class AttachmentRole : uint8_t
{
	/// Resolve target if MSAA resolve exists, else colorAttachments[0].
	PrimaryColor,
	/// colorAttachments[attachmentIndex].
	Color,
	/// depthAttachment (includes arrayLayer from producer).
	Depth,
	/// resolveAttachment.
	Resolve,
};

enum class ReadSource : uint8_t
{
	ExplicitTexture,
	PipelineSurface,
	PassOutput,
};

struct ReadDesc
{
	ReadSource source = ReadSource::ExplicitTexture;
	abstract_texture* texture = nullptr;
	PipelineSurfaceId pipelineSurface = PipelineSurfaceId::SceneColor;
	PassHandle producerPass = kInvalidPassHandle;
	AttachmentRole producerRole = AttachmentRole::PrimaryColor;
	uint32_t attachmentIndex = 0;
	optional<uint32_t> arrayLayer;
	optional<uint32_t> mipLevel;
};

struct ResolvedRead
{
	abstract_texture* texture = nullptr;
	uint32_t arrayLayer = 0;
	uint32_t mipLevel = 0;
	bool isDepth = false;
};

class RenderPassContext;

/// Describes what a render pass needs to do.
struct RenderPassDesc
{
	std::string debugName;

	// Records draw calls for this pass. Must only invoke gfx_api draw/bind APIs.
	// When called, executeRenderGraph() is active and the backend has configured
	// the pass (command buffer, framebuffer, viewport, etc.).
	using RecordFunc = std::function<void(const RenderPassContext&)>;
	RecordFunc recordFunc;

	std::vector<AttachmentDesc> colorAttachments;
	optional<AttachmentDesc> depthAttachment;
	optional<AttachmentDesc> resolveAttachment;
	optional<std::pair<uint32_t, uint32_t>> viewportSize;
	std::vector<ReadDesc> reads;
};

/// Per-pass context passed to record callbacks; reads are compile-resolved.
class RenderPassContext
{
public:
	RenderPassContext(const RenderPassDesc& desc, std::vector<ResolvedRead> resolvedReads);

	abstract_texture* getRead(size_t index) const;
	uint32_t getReadArrayLayer(size_t index) const;
	abstract_texture* getPipelineSurface(PipelineSurfaceId id) const;
	std::pair<uint32_t, uint32_t> viewportSize() const;

private:
	const RenderPassDesc& _desc;
	std::vector<ResolvedRead> _resolvedReads;
};

/// Fluent builder for RenderPassDesc. Returns a pass description by value;
/// call build() on an rvalue to finalize (move-only).
class RenderPassBuilder
{
public:
	static RenderPassBuilder create(const std::string& debugName);

	RenderPassBuilder& colorAttachment(abstract_texture* tex, AttachmentLoadOp loadOp,
		ClearValue clearValue = ClearValue::colorClear());
	RenderPassBuilder& colorAttachment(abstract_texture* tex, bool clear);
	RenderPassBuilder& transientColorAttachment(AttachmentLoadOp loadOp = AttachmentLoadOp::Clear,
		ClearValue clearValue = ClearValue::colorClear());
	RenderPassBuilder& transientDepthAttachment(AttachmentLoadOp loadOp = AttachmentLoadOp::Clear,
		ClearValue clearValue = ClearValue::depthStencilClear());
	RenderPassBuilder& swapchainAttachment(AttachmentLoadOp loadOp = AttachmentLoadOp::Load,
		ClearValue clearValue = ClearValue::colorClear());

	RenderPassBuilder& depthAttachment(abstract_texture* tex, AttachmentLoadOp loadOp,
		ClearValue clearValue = ClearValue::depthStencilClear());
	RenderPassBuilder& depthAttachment(abstract_texture* tex, bool clear);

	RenderPassBuilder& resolveAttachment(abstract_texture* tex,
		AttachmentLoadOp loadOp = AttachmentLoadOp::DontCare,
		ClearValue clearValue = ClearValue::colorClear());

	RenderPassBuilder& viewport(uint32_t w, uint32_t h);
	RenderPassBuilder& readTexture(abstract_texture* tex);
	RenderPassBuilder& readSurface(PipelineSurfaceId id);
	RenderPassBuilder& readPassOutput(PassHandle producer, AttachmentRole role, uint32_t colorIndex = 0);
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

	PassHandle addRenderPass(RenderPassDesc desc);

	void execute();
	void reset();

	size_t passCount() const { return _render_passes.size(); }

private:

	std::vector<RenderPassDesc> _render_passes;
	bool _executing = false;
};

RenderPassDesc makeDepthCascadePass(size_t cascadeIndex, const std::string& debugName,
	RenderPassDesc::RecordFunc recordFunc);
RenderPassDesc makeDepthPrePass(const std::string& debugName, RenderPassDesc::RecordFunc recordFunc);
RenderPassDesc makeScenePass(const std::string& debugName, RenderPassDesc::RecordFunc recordFunc);
RenderPassDesc makeSwapchainPass(const std::string& debugName, AttachmentLoadOp loadOp,
	RenderPassDesc::RecordFunc recordFunc);

} // namespace gfx_api
