#include "gfx_api_render_graph_compile.h"

#include "gfx_api.h"
#include "gfx_api_pass_resolve.h"
#include "gfx_api_pipeline_surfaces.h"

#include <unordered_map>
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

using LayoutStateMap = std::unordered_map<abstract_texture*, CompileImageLayout>;

CompileImageLayout getReadTargetLayout(const ResolvedRead& read)
{
	return read.isDepth ? CompileImageLayout::DepthReadOnly : CompileImageLayout::ShaderReadOnly;
}

void populateCompiledPassLayoutMetadata(CompiledPass& compiledPass)
{
	const RenderPassDesc& pass = compiledPass.desc;
	CompiledPassLayoutMetadata& metadata = compiledPass.renderPassLayouts;
	metadata.colorFinalLayouts.clear();
	metadata.resolveFinalLayout.reset();
	metadata.depthFinalLayout = CompileImageLayout::DepthAttachment;

	for (size_t i = 0; i < pass.colorAttachments.size(); ++i)
	{
		const AttachmentDesc& colorAttachment = pass.colorAttachments[i];
		const auto layout = getAttachmentPostPassLayout(pass, colorAttachment, PostPassAttachmentKind::Color, i);
		metadata.colorFinalLayouts.push_back(layout.value_or(CompileImageLayout::ColorAttachment));
	}

	if (pass.resolveAttachment.has_value())
	{
		const auto layout = getAttachmentPostPassLayout(pass, pass.resolveAttachment.value(),
			PostPassAttachmentKind::Resolve);
		if (layout.has_value())
		{
			metadata.resolveFinalLayout = layout.value();
		}
	}

	if (pass.depthAttachment.has_value())
	{
		const auto layout = getAttachmentPostPassLayout(pass, pass.depthAttachment.value(),
			PostPassAttachmentKind::Depth);
		if (layout.has_value())
		{
			metadata.depthFinalLayout = layout.value();
		}
	}
}

void applyPostPassLayoutUpdates(CompiledPass& compiledPass, LayoutStateMap& layoutState)
{
	const RenderPassDesc& pass = compiledPass.desc;

	if (passNeedsMsaaResolve(pass) && pass.resolveAttachment.has_value())
	{
		const AttachmentDesc& resolveAttachment = pass.resolveAttachment.value();
		const auto layout = getAttachmentPostPassLayout(pass, resolveAttachment, PostPassAttachmentKind::Resolve);
		if (layout.has_value())
		{
			compiledPass.postPassLayoutUpdates.push_back({resolveAttachment.texture, layout.value()});
			layoutState[resolveAttachment.texture] = layout.value();
		}
	}
	else
	{
		for (size_t i = 0; i < pass.colorAttachments.size(); ++i)
		{
			const AttachmentDesc& colorAttachment = pass.colorAttachments[i];
			const auto layout = getAttachmentPostPassLayout(pass, colorAttachment,
				PostPassAttachmentKind::Color, i);
			if (layout.has_value())
			{
				compiledPass.postPassLayoutUpdates.push_back({colorAttachment.texture, layout.value()});
				layoutState[colorAttachment.texture] = layout.value();
			}
		}
	}

	if (pass.depthAttachment.has_value())
	{
		const AttachmentDesc& depthAttachment = pass.depthAttachment.value();
		const auto layout = getAttachmentPostPassLayout(pass, depthAttachment, PostPassAttachmentKind::Depth);
		if (layout.has_value())
		{
			compiledPass.postPassLayoutUpdates.push_back({depthAttachment.texture, layout.value()});
			layoutState[depthAttachment.texture] = layout.value();
		}
	}
}

bool planLayoutTimeline(std::vector<CompiledPass>& passes)
{
	LayoutStateMap layoutState;

	for (CompiledPass& compiledPass : passes)
	{
		compiledPass.prePassBarriers.clear();
		compiledPass.postPassLayoutUpdates.clear();
		compiledPass.renderPassLayouts = CompiledPassLayoutMetadata {};
		if (compiledPass.skipped)
		{
			continue;
		}

		const RenderPassDesc& pass = compiledPass.desc;
		const std::unordered_set<abstract_texture*> passAttachments = getPassAttachmentTextures(pass);

		std::unordered_set<abstract_texture*> seenReadTextures;
		for (const ResolvedRead& read : compiledPass.resolvedReads)
		{
			if (read.texture == nullptr || !seenReadTextures.insert(read.texture).second)
			{
				continue;
			}
			if (passAttachments.count(read.texture) > 0)
			{
				continue;
			}

			const CompileImageLayout targetLayout = getReadTargetLayout(read);
			const auto layoutIt = layoutState.find(read.texture);
			if (layoutIt == layoutState.end())
			{
				ASSERT_OR_RETURN(false, false,
					"Pass \"%s\": read texture not produced by an earlier pass in the layout timeline",
					pass.debugName.c_str());
			}

			const CompileImageLayout currentLayout = layoutIt->second;
			if (currentLayout != targetLayout)
			{
				ImageBarrierOp barrier;
				barrier.texture = read.texture;
				barrier.isDepth = read.isDepth;
				barrier.oldLayout = currentLayout;
				barrier.newLayout = targetLayout;
				compiledPass.prePassBarriers.push_back(barrier);
				layoutState[read.texture] = targetLayout;
			}
		}

		applyPostPassLayoutUpdates(compiledPass, layoutState);
		populateCompiledPassLayoutMetadata(compiledPass);
	}

	return true;
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

nonstd::optional<CompileImageLayout> getAttachmentPostPassLayout(
	const RenderPassDesc& pass,
	const AttachmentDesc& attachment,
	PostPassAttachmentKind kind,
	size_t colorIndex)
{
	if (attachment.texture == nullptr)
	{
		return nonstd::nullopt;
	}

	switch (kind)
	{
	case PostPassAttachmentKind::Color:
		if (attachment.isSwapchain())
		{
			return nonstd::nullopt;
		}
		if (passNeedsMsaaResolve(pass) && colorIndex == 0)
		{
			return nonstd::nullopt;
		}
		return CompileImageLayout::ShaderReadOnly;
	case PostPassAttachmentKind::Resolve:
		if (!passNeedsMsaaResolve(pass))
		{
			return nonstd::nullopt;
		}
		return CompileImageLayout::ShaderReadOnly;
	case PostPassAttachmentKind::Depth:
		if (isDepthShaderSampledSurface(attachment.texture) && passIsDepthOnly(pass))
		{
			return CompileImageLayout::DepthReadOnly;
		}
		return CompileImageLayout::DepthAttachment;
	}

	return nonstd::nullopt;
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

	if (!planLayoutTimeline(_passes))
	{
		return false;
	}
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
