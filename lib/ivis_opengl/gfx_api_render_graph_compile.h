#pragma once

#include "gfx_api_render_graph.h"

#include <cstdint>
#include <vector>

#include <nonstd/optional.hpp>

namespace gfx_api
{

/// Backend-agnostic image layout state tracked during graph compilation.
enum class CompileImageLayout : uint8_t
{
	Undefined,
	ShaderReadOnly,
	DepthReadOnly,
	ColorAttachment,
	DepthAttachment,
	Present,
};

struct ImageBarrierOp
{
	abstract_texture* texture = nullptr;
	bool isDepth = false;
	CompileImageLayout oldLayout = CompileImageLayout::Undefined;
	CompileImageLayout newLayout = CompileImageLayout::Undefined;
};

struct LayoutStateUpdate
{
	abstract_texture* texture = nullptr;
	CompileImageLayout layout = CompileImageLayout::Undefined;
};

/// Render-pass attachment final layouts derived at compile time (feeds VK PassLayoutKey).
struct CompiledPassLayoutMetadata
{
	CompileImageLayout depthFinalLayout = CompileImageLayout::DepthAttachment;
	std::vector<CompileImageLayout> colorFinalLayouts;
	nonstd::optional<CompileImageLayout> resolveFinalLayout;
};

struct CompiledPass
{
	RenderPassDesc desc;
	size_t graphIndex = 0;
	bool skipped = false;
	std::vector<ResolvedRead> resolvedReads;
	std::vector<ImageBarrierOp> prePassBarriers;
	std::vector<LayoutStateUpdate> postPassLayoutUpdates;
	CompiledPassLayoutMetadata renderPassLayouts;
};

enum class PostPassAttachmentKind : uint8_t
{
	Color,
	Resolve,
	Depth,
};

/// Post-pass layout for an attachment write; nullopt when compile state should not advance.
nonstd::optional<CompileImageLayout> getAttachmentPostPassLayout(
	const RenderPassDesc& pass,
	const AttachmentDesc& attachment,
	PostPassAttachmentKind kind,
	size_t colorIndex = 0);

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
