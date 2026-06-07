#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "gfx_api_attachment.h"
#include "gfx_api_formats_def.h"

#include <nonstd/optional.hpp>

namespace gfx_api
{

struct abstract_texture;

enum class PipelineSurfaceId : uint8_t
{
	SceneColor,
	SceneMSAAColor,
	SceneDepth,
	ShadowMap,
	Count
};

enum class PipelineSurfaceUsage : uint8_t
{
	ColorResolve,
	ColorMSAA,
	DepthStencil,
	DepthOnly,
};

struct PipelineSurfaceMeta
{
	PipelineSurfaceUsage usage = PipelineSurfaceUsage::ColorResolve;
	pixel_format format = pixel_format::invalid;
	uint32_t samples = 1;
};

/// Static defaults for each pipeline surface (samples may be overridden at registration).
PipelineSurfaceMeta getPipelineSurfaceMeta(PipelineSurfaceId id);

/// Fixed-size registry of persistent pipeline render targets.
class PipelineSurfaceRegistry
{
public:
	void registerSurface(PipelineSurfaceId id, abstract_texture* surface);
	void invalidateSurface(PipelineSurfaceId id);
	abstract_texture* get(PipelineSurfaceId id) const;
	bool isRegistered(PipelineSurfaceId id) const;

	/// Runtime sample count (SceneMSAAColor / SceneDepth when MSAA is enabled).
	void setSurfaceSamples(PipelineSurfaceId id, uint32_t samples);
	PipelineSurfaceMeta meta(PipelineSurfaceId id) const;

	nonstd::optional<PipelineSurfaceId> findSurfaceId(abstract_texture* texture) const;

private:
	std::array<abstract_texture*, static_cast<size_t>(PipelineSurfaceId::Count)> _surfaces {};
	std::array<uint32_t, static_cast<size_t>(PipelineSurfaceId::Count)> _samplesOverride {};
};

AttachmentDesc makePipelineSurfaceAttachment(PipelineSurfaceId id,
	AttachmentLoadOp op = AttachmentLoadOp::Clear,
	ClearValue clear = ClearValue::colorClear());

} // namespace gfx_api
