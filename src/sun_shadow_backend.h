// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef GLM_FORCE_SWIZZLE
#define GLM_FORCE_SWIZZLE
#endif

#include "lib/ivis_opengl/piedef.h"
#include "lib/ivis_opengl/shadows.h"
#include "shadowcascades.h"

#include "lib/ivis_opengl/gfx_api_render_graph.h"

#include "lib/framework/vector.h"

#include <cstdint>
#include <memory>
#include <vector>

struct SceneDescription;
struct iView;

enum class SunShadowTechnique : uint8_t
{
	Csm,
	RayQuery,
};

struct SunShadowFrameContext
{
	ShadowMode shadowMode = ShadowMode::Shadow_Mapping;
	SunShadowTechnique technique = SunShadowTechnique::Csm;
	glm::mat4 baseViewMatrix {1.f};
	glm::vec3 lightInvDir {0.f};
	float terrainDistance = 0.f;
	const iView* player = nullptr;
	uint32_t numShadowCascades = 0;
	bool drawTerrainShadows = true;
	uint64_t currentGameFrame = 0;
	Vector3f cameraPos {0.f, 0.f, 0.f};
	ShadowCascadesInfo cascadesInfo {};
	std::vector<Cascade> cascades;
};

class SunShadowBackend
{
public:
	virtual ~SunShadowBackend() = default;

	virtual void planPreScenePasses(gfx_api::RenderGraph& graph,
	                                SunShadowFrameContext& ctx,
	                                SceneDescription scene) = 0;
	virtual void bindForForwardPass() = 0;
	virtual SunShadowTechnique technique() const = 0;
};

std::unique_ptr<SunShadowBackend> createSunShadowBackend(SunShadowTechnique technique);
SunShadowTechnique resolveSunShadowTechnique();
