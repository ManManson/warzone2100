#include "gfx_api_pipeline_surfaces.h"

#include "gfx_api.h"

namespace gfx_api
{

void PipelineSurfaceRegistry::registerSurface(PipelineSurfaceId id, abstract_texture* surface)
{
	_surfaces[static_cast<size_t>(id)] = surface;
}

void PipelineSurfaceRegistry::invalidateSurface(PipelineSurfaceId id)
{
	_surfaces[static_cast<size_t>(id)] = nullptr;
}

abstract_texture* PipelineSurfaceRegistry::get(PipelineSurfaceId id) const
{
	return _surfaces[static_cast<size_t>(id)];
}

bool PipelineSurfaceRegistry::isRegistered(PipelineSurfaceId id) const
{
	return _surfaces[static_cast<size_t>(id)] != nullptr;
}

AttachmentDesc makePipelineSurfaceAttachment(PipelineSurfaceId id, AttachmentLoadOp op, ClearValue clear)
{
	AttachmentDesc desc;
	desc.source = AttachmentSource::Texture;
	desc.texture = context::get().getPipelineSurface(id);
	desc.loadOp = op;
	desc.clearValue = clear;
	return desc;
}

} // namespace gfx_api
