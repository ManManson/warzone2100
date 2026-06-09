// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "sun_shadow_backend.h"

#include "lib/ivis_opengl/gfx_api_render_graph.h"
#include "lib/ivis_opengl/shadows.h"

#include <functional>

struct SceneDescription;

struct ScenePassCallbacks
{
	std::function<void(const ShadowCascadesInfo& shadowCascadesInfo)> recordScenePass;
};

class LightingFramePlanner
{
public:
	/// Shadow cascade passes (or future AS build). Fills sunCtx.cascadesInfo.
	void planSunShadowPrePasses(gfx_api::RenderGraph& graph,
	                            SceneDescription scene,
	                            SunShadowFrameContext& sunCtx);

	/// Main forward scene pass. Call after optional SSAO depth prepass.
	gfx_api::PassHandle addScenePass(gfx_api::RenderGraph& graph,
	                                 const ShadowCascadesInfo& cascadesInfo,
	                                 const ScenePassCallbacks& callbacks);

private:
	SunShadowTechnique _sunTechnique = SunShadowTechnique::Csm;
};
