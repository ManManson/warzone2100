#include "gfx_api_pipeline_surfaces.h"

#include "gfx_api.h"

namespace gfx_api
{

PipelineSurfaceMeta getPipelineSurfaceMeta(PipelineSurfaceId id)
{
	PipelineSurfaceMeta meta;
	switch (id)
	{
	case PipelineSurfaceId::SceneColor:
		meta.usage = PipelineSurfaceUsage::ColorResolve;
		meta.format = pixel_format::FORMAT_RGBA8_UNORM_PACK8;
		meta.samples = 1;
		break;
	case PipelineSurfaceId::SceneMSAAColor:
		meta.usage = PipelineSurfaceUsage::ColorMSAA;
		meta.format = pixel_format::FORMAT_RGBA8_UNORM_PACK8;
		meta.samples = 1;
		break;
	case PipelineSurfaceId::SceneDepth:
		meta.usage = PipelineSurfaceUsage::DepthStencil;
		meta.format = pixel_format::FORMAT_D24_UNORM_S8;
		meta.samples = 1;
		break;
	case PipelineSurfaceId::ShadowMap:
		meta.usage = PipelineSurfaceUsage::DepthOnly;
		meta.format = pixel_format::FORMAT_D24_UNORM_S8;
		meta.samples = 1;
		break;
	case PipelineSurfaceId::Count:
		break;
	}
	return meta;
}

void PipelineSurfaceRegistry::registerSurface(PipelineSurfaceId id, abstract_texture* surface)
{
	_surfaces[static_cast<size_t>(id)] = surface;
}

void PipelineSurfaceRegistry::invalidateSurface(PipelineSurfaceId id)
{
	_surfaces[static_cast<size_t>(id)] = nullptr;
	_samplesOverride[static_cast<size_t>(id)] = 0;
}

abstract_texture* PipelineSurfaceRegistry::get(PipelineSurfaceId id) const
{
	return _surfaces[static_cast<size_t>(id)];
}

bool PipelineSurfaceRegistry::isRegistered(PipelineSurfaceId id) const
{
	return _surfaces[static_cast<size_t>(id)] != nullptr;
}

void PipelineSurfaceRegistry::setSurfaceSamples(PipelineSurfaceId id, uint32_t samples)
{
	_samplesOverride[static_cast<size_t>(id)] = samples;
}

PipelineSurfaceMeta PipelineSurfaceRegistry::meta(PipelineSurfaceId id) const
{
	PipelineSurfaceMeta result = getPipelineSurfaceMeta(id);
	const uint32_t overrideSamples = _samplesOverride[static_cast<size_t>(id)];
	if (overrideSamples > 0)
	{
		result.samples = overrideSamples;
	}
	return result;
}

nonstd::optional<PipelineSurfaceId> PipelineSurfaceRegistry::findSurfaceId(abstract_texture* texture) const
{
	if (texture == nullptr)
	{
		return nonstd::nullopt;
	}
	for (size_t i = 0; i < static_cast<size_t>(PipelineSurfaceId::Count); ++i)
	{
		if (_surfaces[i] == texture)
		{
			return static_cast<PipelineSurfaceId>(i);
		}
	}
	return nonstd::nullopt;
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
