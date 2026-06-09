// SPDX-License-Identifier: GPL-2.0-or-later

#include "lighting_frame_planner.h"

#include "scene_description.h"

void LightingFramePlanner::planSunShadowPrePasses(gfx_api::RenderGraph& graph,
                                                  SceneDescription& scene,
                                                  SunShadowFrameContext& sunCtx)
{
	auto sunBackend = createSunShadowBackend(sunCtx.technique);
	sunBackend->planPreScenePasses(graph, sunCtx, scene);
	sunBackend->bindForForwardPass();
}

gfx_api::PassHandle LightingFramePlanner::addScenePass(gfx_api::RenderGraph& graph,
                                                       const ShadowCascadesInfo& cascadesInfo,
                                                       const ScenePassCallbacks& callbacks)
{
	return graph.addRenderPass(
		gfx_api::makeScenePass("ScenePass",
			[callbacks, cascadesInfo](const gfx_api::RenderPassContext&)
			{
				callbacks.recordScenePass(cascadesInfo);
			}));
}
