#pragma once

#include <functional>
#include <vector>
#include <string>

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
	Default   // Swapchain / compositing + UI
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

	// The function that records draw calls for this pass.
	// When called, the context is already configured for this pass
	// (correct command buffer, FBO, viewport, etc.)
	using RecordFunc = std::function<void()>;
	RecordFunc recordFunc;
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
	void execute();

	// Clear all accumulated passes without executing.
	void clear();

	// Query how many passes are currently queued.
	size_t passCount() const { return _render_passes.size(); }

private:

	std::vector<RenderPassDesc> _render_passes;
};

} // namespace gfx_api
