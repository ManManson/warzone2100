#include "gfx_api_render_graph_compile.h"

#include "gfx_api.h"
#include "gfx_api_pass_resolve.h"
#include "gfx_api_pipeline_surfaces.h"

#include <unordered_set>

namespace gfx_api
{

namespace
{

bool passHasSwapchainColor(const RenderPassDesc& pass)
{
	for (const auto& colorAttachment : pass.colorAttachments)
	{
		if (colorAttachment.isSwapchain())
		{
			return true;
		}
	}
	return false;
}

nonstd::optional<ResolvedRead> resolveSingleRead(const ReadDesc& read,
	const std::vector<RenderPassDesc>& descs, size_t consumerIndex)
{
	auto& ctx = gfx_api::context::get();
	ResolvedRead resolved;

	switch (read.source)
	{
	case ReadSource::ExplicitTexture:
		ASSERT_OR_RETURN(nonstd::nullopt, read.texture != nullptr,
			"Pass \"%s\" read[%zu]: explicit texture is null",
			descs[consumerIndex].debugName.c_str(), consumerIndex);
		resolved.texture = read.texture;
		break;
	case ReadSource::PipelineSurface:
		resolved.texture = ctx.getPipelineSurface(read.pipelineSurface);
		ASSERT_OR_RETURN(nonstd::nullopt, resolved.texture != nullptr,
			"Pass \"%s\": pipeline surface %u is not registered",
			descs[consumerIndex].debugName.c_str(), static_cast<unsigned>(read.pipelineSurface));
		break;
	case ReadSource::PassOutput:
		ASSERT_OR_RETURN(nonstd::nullopt, read.producerPass != kInvalidPassHandle,
			"Pass \"%s\": PassOutput read has invalid producer handle",
			descs[consumerIndex].debugName.c_str());
		ASSERT_OR_RETURN(nonstd::nullopt, read.producerPass < consumerIndex,
			"Pass \"%s\": cannot read output from pass %zu (not prior to consumer %zu)",
			descs[consumerIndex].debugName.c_str(), read.producerPass, consumerIndex);
		ASSERT_OR_RETURN(nonstd::nullopt, read.producerPass < descs.size(),
			"Pass \"%s\": producer pass handle %zu out of range",
			descs[consumerIndex].debugName.c_str(), read.producerPass);
		ASSERT_OR_RETURN(nonstd::nullopt, !passHasSwapchainColor(descs[read.producerPass]),
			"Pass \"%s\": cannot read PassOutput from swapchain pass %zu",
			descs[consumerIndex].debugName.c_str(), read.producerPass);
		{
			const auto output = getPassOutputAttachment(descs[read.producerPass],
				read.producerRole, read.attachmentIndex);
			ASSERT_OR_RETURN(nonstd::nullopt, output.has_value(),
				"Pass \"%s\": failed to resolve PassOutput from pass %zu role %u",
				descs[consumerIndex].debugName.c_str(), read.producerPass,
				static_cast<unsigned>(read.producerRole));
			if (read.producerRole == AttachmentRole::Color && output->isMultisampled)
			{
				ASSERT(false,
					"Pass \"%s\": cannot read multisampled Color attachment from pass %zu; use PrimaryColor or Resolve",
					descs[consumerIndex].debugName.c_str(), read.producerPass);
				return nonstd::nullopt;
			}
			resolved.texture = output->texture;
			resolved.arrayLayer = read.arrayLayer.has_value() ? read.arrayLayer.value() : output->arrayLayer;
			resolved.mipLevel = read.mipLevel.has_value() ? read.mipLevel.value() : output->mipLevel;
			resolved.isDepth = output->isDepth;
		}
		break;
	}

	if (read.arrayLayer.has_value() && read.source != ReadSource::PassOutput)
	{
		resolved.arrayLayer = read.arrayLayer.value();
	}
	if (read.mipLevel.has_value() && read.source != ReadSource::PassOutput)
	{
		resolved.mipLevel = read.mipLevel.value();
	}

	ASSERT_OR_RETURN(nonstd::nullopt, resolved.texture != nullptr,
		"Pass \"%s\": unresolved read texture", descs[consumerIndex].debugName.c_str());
	return resolved;
}

void planImageBarriers(std::vector<CompiledPass>& passes)
{
	for (CompiledPass& compiledPass : passes)
	{
		compiledPass.prePassBarriers.clear();
		if (compiledPass.skipped)
		{
			continue;
		}

		std::unordered_set<abstract_texture*> seenTextures;
		for (const ResolvedRead& read : compiledPass.resolvedReads)
		{
			if (read.texture == nullptr || !seenTextures.insert(read.texture).second)
			{
				continue;
			}
			ImageBarrierOp barrier;
			barrier.texture = read.texture;
			barrier.isDepth = read.isDepth;
			compiledPass.prePassBarriers.push_back(barrier);
		}
	}
}

} // namespace

bool resolvePassReads(const std::vector<RenderPassDesc>& descs, size_t passIndex,
	std::vector<ResolvedRead>& outReads)
{
	outReads.clear();
	if (passIndex >= descs.size())
	{
		return false;
	}
	for (const ReadDesc& read : descs[passIndex].reads)
	{
		const auto resolved = resolveSingleRead(read, descs, passIndex);
		if (!resolved.has_value())
		{
			return false;
		}
		outReads.push_back(resolved.value());
	}
	return true;
}

RenderPassContext buildRenderPassContext(const std::vector<RenderPassDesc>& descs, size_t passIndex)
{
	std::vector<ResolvedRead> resolvedReads;
	resolvePassReads(descs, passIndex, resolvedReads);
	return RenderPassContext(descs[passIndex], std::move(resolvedReads));
}

RenderPassContext buildRenderPassContext(const CompiledPass& compiledPass)
{
	return RenderPassContext(compiledPass.desc, compiledPass.resolvedReads);
}

bool CompiledRenderGraph::compile(std::vector<RenderPassDesc>& descs)
{
	_passes.clear();
	_passes.resize(descs.size());

	for (size_t i = 0; i < descs.size(); ++i)
	{
		_passes[i].graphIndex = i;
		_passes[i].desc = descs[i];
		_passes[i].skipped = !resolvePassDescription(descs[i]);
		if (_passes[i].skipped)
		{
			continue;
		}
		_passes[i].desc = descs[i];
		if (!resolvePassReads(descs, i, _passes[i].resolvedReads))
		{
			return false;
		}
	}

	planImageBarriers(_passes);
	return true;
}

RenderPassContext::RenderPassContext(const RenderPassDesc& desc, std::vector<ResolvedRead> resolvedReads)
	: _desc(desc)
	, _resolvedReads(std::move(resolvedReads))
{
}

abstract_texture* RenderPassContext::getRead(size_t index) const
{
	if (index >= _resolvedReads.size())
	{
		return nullptr;
	}
	return _resolvedReads[index].texture;
}

uint32_t RenderPassContext::getReadArrayLayer(size_t index) const
{
	if (index >= _resolvedReads.size())
	{
		return 0;
	}
	return _resolvedReads[index].arrayLayer;
}

abstract_texture* RenderPassContext::getPipelineSurface(PipelineSurfaceId id) const
{
	return gfx_api::context::get().getPipelineSurface(id);
}

std::pair<uint32_t, uint32_t> RenderPassContext::viewportSize() const
{
	if (_desc.viewportSize.has_value())
	{
		return _desc.viewportSize.value();
	}
	return gfx_api::context::get().getDrawableDimensions();
}

} // namespace gfx_api
