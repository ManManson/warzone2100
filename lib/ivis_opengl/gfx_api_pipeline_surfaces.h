#pragma once

#include "gfx_api_attachment.h"

#include <array>
#include <cstddef>
#include <cstdint>

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

/// Fixed-size registry of persistent pipeline render targets.
class PipelineSurfaceRegistry
{
public:
	void registerSurface(PipelineSurfaceId id, abstract_texture* surface);
	void invalidateSurface(PipelineSurfaceId id);
	abstract_texture* get(PipelineSurfaceId id) const;
	bool isRegistered(PipelineSurfaceId id) const;

private:
	std::array<abstract_texture*, static_cast<size_t>(PipelineSurfaceId::Count)> _surfaces {};
};

AttachmentDesc makePipelineSurfaceAttachment(PipelineSurfaceId id,
	AttachmentLoadOp op = AttachmentLoadOp::Clear,
	ClearValue clear = ClearValue::colorClear());

} // namespace gfx_api
