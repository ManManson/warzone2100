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
/** @file ssao.h
 * Screen-space ambient occlusion (SSAO) game-side module.
 */

#pragma once

#include "lib/ivis_opengl/gfx_api.h"
#include "warzoneconfig.h"

namespace ssao
{

/// Discrete quality preset: on/off, sample/blur cost, and target-size divisors.
/// Analog look (radius, bias, intensity) stays in Tuning inside ssao.cpp.
struct SsaoSettings
{
	bool enabled = false;
	uint32_t generateDivisor = 1; // 1 = full res; 2 = half; 4 = quarter
	uint32_t blurDivisor = 1;     // >= generateDivisor (never upsample before blur)
	int sampleCount = 16;         // 8 or 16, <= gfx_api::SSAO_KERNEL_SIZE
	int blurTapPairs = 4;         // 2 → 5-tap, 4 → 9-tap
};

SsaoSettings settingsFor(SSAO_MODE mode);
SsaoSettings activeSettings();

struct LightingBind {
	gfx_api::abstract_texture* texture = nullptr;
	float intensity = 0.f;
	glm::vec4 uvScaleClamp {1.f, 1.f, 1.f, 1.f};
};

/// Dummy 1x1 white (unoccluded) used when SSAO is off or a shader must bind a texture it will not sample.
gfx_api::abstract_texture* unoccludedTexture();
/// ScenePass bind: blurred AO + intensity when `ssaoRead` is non-null, else dummy + intensity 0.
LightingBind lightingBind(const gfx_api::RenderPassContext& passCtx, gfx_api::abstract_texture* ssaoRead);

void init();
void shutdown();

void recordGenerate(const gfx_api::RenderPassContext& passCtx);
void recordDownsample(const gfx_api::RenderPassContext& passCtx);
void recordBlurH(const gfx_api::RenderPassContext& passCtx);
void recordBlurV(const gfx_api::RenderPassContext& passCtx);

} // namespace ssao
