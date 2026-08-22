// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project (https://github.com/Warzone2100)

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/** @file ssr.cpp
 * Screen-space reflections (SSR) game-side module (water-only v1).
 */

#include "ssr.h"

#include "display3d_render_graph.h"
#include "display3d_render_internal.h"
#include "depth_aware_blur.h"
#include "terrain.h"

#include "lib/framework/frame.h"
#include "lib/ivis_opengl/gfx_api.h"
#include "lib/ivis_opengl/piestate.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <cstdint>

namespace ssr
{

static constexpr uint32_t kFull = 1;
static constexpr uint32_t kHalf = 2;
static constexpr uint32_t kQuarter = 4;

static constexpr SsrSettings kSsrPresets[] = {
	/* OFF    */ {},
	/* LOW    */ {true, kQuarter, kQuarter, 12, 2},
	/* NORMAL */ {true, kHalf,    kQuarter, 16, 2},
	/* HIGH   */ {true, kHalf,    kHalf,    32, 4},
	/* ULTRA  */ {true, kFull,    kFull,    48, 4},
};
static_assert(sizeof(kSsrPresets) / sizeof(kSsrPresets[0]) == static_cast<size_t>(SSR_MODE::ULTRA) + 1,
	"SSR_MODE and kSsrPresets must stay in sync");

SsrSettings settingsFor(SSR_MODE mode)
{
	const size_t i = static_cast<size_t>(mode);
	ASSERT_OR_RETURN(SsrSettings{}, i < sizeof(kSsrPresets) / sizeof(kSsrPresets[0]), "bad SSR_MODE");
	const SsrSettings s = kSsrPresets[i];
	if (s.enabled)
	{
		ASSERT(s.generateDivisor >= 1, "bad generateDivisor");
		ASSERT(s.blurDivisor >= s.generateDivisor, "blur finer than generate");
		ASSERT(s.stepCount > 0 && s.stepCount <= 64, "bad stepCount");
		ASSERT(s.blurTapPairs >= 1 && s.blurTapPairs <= 4, "bad tapPairs");
	}
	return s;
}

SsrSettings activeSettings()
{
	SsrSettings s = settingsFor(war_getSsrMode());
	if (s.enabled && getTerrainShaderQuality() != TerrainShaderQuality::NORMAL_MAPPING)
	{
		s.enabled = false;
	}
	return s;
}

bool surfacesRequested()
{
	return activeSettings().enabled;
}

namespace
{

struct Tuning
{
	/// Absolute view-space march length. Map units are thousands (default zoom ~2600, fog end 8000).
	float maxRayLength;
	/// Hit slab in view-space units. Must be on the order of one march step or coarse traces miss.
	float thickness;
	/// Fraction of abs(origin.z) skipped before the first sample (avoids self-hit on the water plane).
	float minRayStart;
	/// Sigma of the blur's depth falloff, in normalized depth units
	float blurDepthSigma;
	/// Compose mix multiplier. >1 is intentional for visibility at default camera pitch.
	float intensity;
	/// Schlick F0. Physical water is ~0.02; a large value is required or facing water is invisible.
	float F0;
};

constexpr Tuning DEFAULT_TUNING = {
	.maxRayLength = 8000.f,
	.thickness = 400.f,
	.minRayStart = 0.004f,
	.blurDepthSigma = 0.0025f,
	.intensity = 1.6f,
	.F0 = 0.22f,
};

Tuning s_tuning = DEFAULT_TUNING;

void drawSSRGenerate(
	const gfx_api::RenderPassContext& passCtx,
	gfx_api::abstract_texture* depthTexture,
	gfx_api::abstract_texture* normalsTexture,
	gfx_api::abstract_texture* sceneTexture,
	const glm::mat4& projectionMatrix,
	const glm::mat4& invProjectionMatrix)
{
	gfx_api::constant_buffer_type<SHADER_SSR_GENERATE> constants {};
	constants.invProjectionMatrix = invProjectionMatrix;
	constants.projectionMatrix = projectionMatrix;
	constants.params = glm::vec4(s_tuning.maxRayLength, s_tuning.thickness, s_tuning.minRayStart, 0.f);
	display3d_fillPassReadUvScaleClamp(passCtx, 0, constants.prepassUvScaleClamp);
	display3d_fillPassReadUvScaleClamp(passCtx, 2, constants.sceneUvScaleClamp);
	constants.stepCount = static_cast<float>(activeSettings().stepCount);

	display3d_drawFullscreenTriangle<gfx_api::SSRGeneratePSO>(constants, depthTexture, normalsTexture, sceneTexture);
}

} // namespace

void init()
{
}

void shutdown()
{
}

void recordGenerate(const gfx_api::RenderPassContext& passCtx)
{
	ASSERT(passCtx.readCount() == 3, "SSR generate: 0 depth, 1 normals, 2 scene");
	if (!pie_IsInGame3DFrameContextReady())
	{
		return;
	}

	gfx_api::abstract_texture* depth = passCtx.getRead(0);
	gfx_api::abstract_texture* normals = passCtx.getRead(1);
	gfx_api::abstract_texture* scene = passCtx.getRead(2);
	if (depth == nullptr || normals == nullptr || scene == nullptr)
	{
		return;
	}

	const auto& fc = pie_GetInGame3DFrameContext();
	drawSSRGenerate(passCtx, depth, normals, scene, fc.perspectiveMatrix, glm::inverse(fc.perspectiveMatrix));
}

void recordDownsample(const gfx_api::RenderPassContext& passCtx)
{
	post_effect_blur::recordBilinearResample(passCtx, "SSR downsample");
}

void recordBlurH(const gfx_api::RenderPassContext& passCtx)
{
	post_effect_blur::recordDepthAwareBlur<SHADER_SSR_BLUR, gfx_api::SSRBlurPSO>(
		passCtx, post_effect_blur::Axis::Horizontal, s_tuning.blurDepthSigma,
		activeSettings().blurTapPairs, "SSR blur");
}

void recordBlurV(const gfx_api::RenderPassContext& passCtx)
{
	post_effect_blur::recordDepthAwareBlur<SHADER_SSR_BLUR, gfx_api::SSRBlurPSO>(
		passCtx, post_effect_blur::Axis::Vertical, s_tuning.blurDepthSigma,
		activeSettings().blurTapPairs, "SSR blur");
}

void recordCompose(const gfx_api::RenderPassContext& passCtx)
{
	ASSERT(passCtx.readCount() == 4, "SSR compose: 0 scene, 1 ssr, 2 normals, 3 depth");
	gfx_api::abstract_texture* scene = passCtx.getRead(0);
	gfx_api::abstract_texture* ssrTex = passCtx.getRead(1);
	gfx_api::abstract_texture* prepassNormals = passCtx.getRead(2);
	gfx_api::abstract_texture* prepassDepth = passCtx.getRead(3);
	if (scene == nullptr || ssrTex == nullptr || prepassNormals == nullptr || prepassDepth == nullptr
		|| !pie_IsInGame3DFrameContextReady())
	{
		return;
	}

	const auto& fc = pie_GetInGame3DFrameContext();
	gfx_api::constant_buffer_type<SHADER_SCENE_COMPOSE_SSR> constants {};
	constants.invProjectionMatrix = glm::inverse(fc.perspectiveMatrix);
	constants.intensity = s_tuning.intensity;
	constants.F0 = s_tuning.F0;
	display3d_fillPassReadUvScaleClamp(passCtx, 0, constants.sceneUvScaleClamp);
	display3d_fillPassReadUvScaleClamp(passCtx, 1, constants.ssrUvScaleClamp);
	display3d_fillPassReadUvScaleClamp(passCtx, 2, constants.normalsUvScaleClamp);
	display3d_fillPassReadUvScaleClamp(passCtx, 3, constants.depthUvScaleClamp);
	display3d_drawFullscreenTriangle<gfx_api::SceneComposeSSRPSO>(constants, scene, ssrTex, prepassNormals, prepassDepth);
}

} // namespace ssr
