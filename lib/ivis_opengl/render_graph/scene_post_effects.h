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
/** @file scene_post_effects.h
 * Descriptor table for screen-space effects after opaque ScenePass and before SceneTransparent.
 *
 * This table does not describe lighting inputs. SSAO multiplies ambient in the
 * forward shaders; emitSsaoPreparePasses / ScenePass `readSsao` wire it like
 * shadow cascades. A table row cannot be prepare-only.
 *
 * Every row has an applyPass. `emitPreparePasses` is an optional subgraph
 * immediately before that row's apply (rings SDF). Fog leaves it null.
 *
 * FogApply samples prepass depth to identify the visible opaque surface only.
 * Transparent layers use their own fragment distance and must apply fog before
 * blending; a later fullscreen pass cannot recover them.
 *
 * Table order is the apply chain (fog -> rings). An enabled post-effect sitting
 * between opaque ScenePass and transparents is what splits them into
 * SceneTransparent -- never SSAO.
 */

#pragma once

#include "blueprint.h"
#include "pipeline_surfaces.h"
#include "render_pass_id.h"
#include "scene_post_effect_id.h"
#include "topology.h"

#include <array>

namespace gfx_api
{

struct ScenePostEffectDesc
{
	ScenePostEffectId id = ScenePostEffectId::Count;

	/// ScenePrepass attachments this apply needs when enabled (OR'd across the table).
	PrepassNeed prepassNeed = PrepassNeed::None;

	/// Optional subgraph immediately before this row's apply. Writes intermediates;
	/// does not write scene color.
	void (*emitPreparePasses)(BlueprintBuilder&, const RenderTopologySnapshot&) = nullptr;

	/// Fullscreen pass that applies the effect to incoming scene color. Required.
	PassId applyPass = PassId::Count;
	/// `BlueprintPass::debugName` / `beginPass` string for `applyPass`.
	const char* applyDebugName = nullptr;
	/// Color attachment the apply pass writes; becomes the next incoming scene color.
	PipelineSurfaceId applyOutput = PipelineSurfaceId::Count;

	/// Ordered samples of `applyPass`; index is the shader binding.
	std::array<ApplyInput, 4> applyInputs {};
	uint8_t applyInputCount = 0;

	/// Pass whose primary color is sampled for `ApplyInput::PreparedOutput`.
	PassId preparedColorPass = PassId::Count;
};

bool effectEnabled(const RenderTopologySnapshot& snapshot, ScenePostEffectId id);
/// True when an enabled post-effect has a color apply -- i.e. something sits between
/// opaque ScenePass and the transparents, so the blueprint separates them into
/// SceneTransparent (and the prepass must provide depth for that split).
/// SSAO does not count: it is not a table row.
bool anyScenePostEffectEnabled(const RenderTopologySnapshot& snapshot);
bool anyScenePostEffectEnabled(const SceneEffectSurfaces& cfg);
PrepassNeed prepassNeeds(const RenderTopologySnapshot& snapshot);
PrepassNeed prepassNeeds(const SceneEffectSurfaces& cfg);

void emitApplyPass(BlueprintBuilder& builder, const ScenePostEffectDesc& effect, PassId incomingColor);
/// SSAO generate/blur after ScenePrepass, before ScenePass. Lighting subgraph, not a table row.
void emitSsaoPreparePasses(BlueprintBuilder& builder, const RenderTopologySnapshot& snapshot);

extern const std::array<ScenePostEffectDesc, static_cast<size_t>(ScenePostEffectId::Count)> kScenePostEffects;

} // namespace gfx_api
