// SPDX-License-Identifier: GPL-2.0-or-later

#include "sun_shadow_backend.h"

#include "scene_description.h"
#include "lib/ivis_opengl/gfx_api.h"
#include "lib/ivis_opengl/piedraw.h"
#include "profiling.h"
#include "terrain.h"

namespace
{

glm::mat4 getBiasedShadowMapMVPMatrix(glm::mat4 lightOrthoMatrix, const glm::mat4& lightViewMatrix)
{
	const glm::mat4 biasMatrix(
		0.5f, 0.f, 0.f, 0.f,
		0.f, 0.5f, 0.f, 0.f,
		0.f, 0.f, 0.5f, 0.f,
		0.5f, 0.5f, 0.5f, 1.f
	);

	const glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
	return biasMatrix * shadowMatrix;
}

class CsmSunShadowBackend final : public SunShadowBackend
{
public:
	void planPreScenePasses(gfx_api::RenderGraph& graph,
	                        SunShadowFrameContext& ctx,
	                        SceneDescription& /*scene*/) override
	{
		if (ctx.shadowMode != ShadowMode::Shadow_Mapping)
		{
			return;
		}

		calculateShadowCascades(ctx.player, ctx.terrainDistance, ctx.baseViewMatrix, ctx.lightInvDir,
		                        ctx.numShadowCascades, ctx.cascades);

		ctx.cascadesInfo.shadowMapSize = static_cast<int>(gfx_api::context::get().getDepthPassDimensions(0));
		for (size_t i = 0; i < std::min(ctx.cascades.size(), static_cast<size_t>(WZ_MAX_SHADOW_CASCADES)); ++i)
		{
			ctx.cascadesInfo.shadowMVPMatrix[i] =
				getBiasedShadowMapMVPMatrix(ctx.cascades[i].projectionMatrix, ctx.cascades[i].viewMatrix);
			ctx.cascadesInfo.shadowCascadeSplit[i] = ctx.cascades[i].splitDepth;
		}

		for (size_t i = 0; i < ctx.numShadowCascades; ++i)
		{
			WZ_PROFILE_SCOPE(ShadowMapping);
			graph.addRenderPass(
				gfx_api::makeDepthCascadePass(i, "ShadowCascade" + std::to_string(i),
					[cascadeProjMatrix = ctx.cascades[i].projectionMatrix,
					 cascadeViewMatrix = ctx.cascades[i].viewMatrix,
					 cameraPos = ctx.cameraPos,
					 shadowCascadesInfo = ctx.cascadesInfo,
					 currentGameFrame = ctx.currentGameFrame,
					 drawTerrainShadows = ctx.drawTerrainShadows](const gfx_api::RenderPassContext&)
					{
						if (drawTerrainShadows)
						{
							drawTerrainDepthOnly(cascadeProjMatrix * cascadeViewMatrix);
						}
						pie_DrawAllMeshes(currentGameFrame, cascadeProjMatrix, cascadeViewMatrix, cameraPos,
						                  shadowCascadesInfo, MeshDepthPassMode::ShadowMap);
					}));
		}
	}

	void bindForForwardPass() override
	{
		// CSM shadow maps are bound via existing PSO texture slots.
	}

	SunShadowTechnique technique() const override
	{
		return SunShadowTechnique::Csm;
	}
};

class RayQuerySunShadowBackend final : public SunShadowBackend
{
public:
	void planPreScenePasses(gfx_api::RenderGraph& graph,
	                        SunShadowFrameContext& ctx,
	                        SceneDescription& scene) override
	{
		if (ctx.shadowMode != ShadowMode::Shadow_Mapping)
		{
			return;
		}

		graph.addRenderPass(gfx_api::makeCommandPass("ASBuild",
			[&scene](const gfx_api::RenderPassContext&)
			{
				gfx_api::context::get().buildAccelerationStructures(scene);
			}));
	}

	void bindForForwardPass() override
	{
		// Phase 2: bind TLAS descriptor set.
	}

	SunShadowTechnique technique() const override
	{
		return SunShadowTechnique::RayQuery;
	}
};

} // namespace

std::unique_ptr<SunShadowBackend> createSunShadowBackend(SunShadowTechnique technique)
{
	switch (technique)
	{
	case SunShadowTechnique::Csm:
		return std::make_unique<CsmSunShadowBackend>();
	case SunShadowTechnique::RayQuery:
		return std::make_unique<RayQuerySunShadowBackend>();
	}
	return std::make_unique<CsmSunShadowBackend>();
}

SunShadowTechnique resolveSunShadowTechnique()
{
	if (pie_getShadowMode() != ShadowMode::Shadow_Mapping)
	{
		return SunShadowTechnique::Csm;
	}

	const auto caps = gfx_api::context::get().capabilities();
	if (caps.rayQuery && caps.accelerationStructure)
	{
		// Phase 4: check user setting for RayQuery tier
	}

	return SunShadowTechnique::Csm;
}
