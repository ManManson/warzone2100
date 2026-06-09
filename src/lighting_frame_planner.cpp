// SPDX-License-Identifier: GPL-2.0-or-later

#include "lighting_frame_planner.h"

#include "scene_description.h"

void LightingFramePlanner::planSunShadowPrePasses(gfx_api::RenderGraph& graph,
                                                  SceneDescription& scene,
                                                  SunShadowFrameContext& sunCtx)
{
	_sunTechnique = sunCtx.technique;
	auto sunBackend = createSunShadowBackend(sunCtx.technique);
	sunBackend->planPreScenePasses(graph, sunCtx, scene);
}

gfx_api::PassHandle LightingFramePlanner::addScenePass(gfx_api::RenderGraph& graph,
                                                       const ShadowCascadesInfo& cascadesInfo,
                                                       const ScenePassCallbacks& callbacks)
{
	return graph.addRenderPass(
		gfx_api::makeScenePass("ScenePass",
			[this, callbacks, cascadesInfo](const gfx_api::RenderPassContext&)
			{
				if (_sunTechnique == SunShadowTechnique::RayQuery)
				{
					createSunShadowBackend(SunShadowTechnique::RayQuery)->bindForForwardPass();
				}
				callbacks.recordScenePass(cascadesInfo);
			}));
}
