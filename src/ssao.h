// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "lib/ivis_opengl/gfx_api_render_graph.h"
#include "lib/ivis_opengl/shadows.h"

#ifndef GLM_FORCE_SWIZZLE
#define GLM_FORCE_SWIZZLE
#endif

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <utility>

namespace gfx_api
{
struct buffer;
}

namespace ssao
{

struct SceneMatrices
{
	glm::mat4 perspectiveViewMatrix;
	glm::mat4 perspectiveMatrix;
	glm::mat4 viewMatrix;
	glm::mat4 invProjectionMatrix;
};

struct PassHandles
{
	gfx_api::PassHandle rawPass = gfx_api::kInvalidPassHandle;
	gfx_api::PassHandle blurHPass = gfx_api::kInvalidPassHandle;
	gfx_api::PassHandle blurVPass = gfx_api::kInvalidPassHandle;
};

bool isEnabled();

void init();
void shutdown();

/// Queue before ScenePass when SSAO is enabled.
gfx_api::PassHandle addDepthPrePass(
	gfx_api::RenderGraph& graph,
	const SceneMatrices& matrices,
	const glm::vec3& cameraPos,
	const ShadowCascadesInfo& shadowInfo,
	uint64_t currentGameFrame);

/// Queue after ScenePass. Returns SSAO intermediate pass handles for compose.
PassHandles addPostScenePasses(
	gfx_api::RenderGraph& graph,
	gfx_api::PassHandle depthPrePass,
	std::pair<uint32_t, uint32_t> sceneDims,
	const glm::mat4& projectionMatrix,
	const glm::mat4& invProjectionMatrix,
	gfx_api::buffer* fullscreenTriVBO);

void addComposePass(
	gfx_api::RenderGraph& graph,
	gfx_api::PassHandle scenePass,
	const PassHandles& ssaoPasses,
	bool backdropEnabled,
	gfx_api::buffer* fullscreenTriVBO);

} // namespace ssao
