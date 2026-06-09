// SPDX-License-Identifier: GPL-2.0-or-later

#include "scene_description.h"

void SceneDescription::beginFrame()
{
	_staticBlas.clear();
	_instances.clear();
}

void SceneDescription::addTerrainSector(int sx, int sy, const BlasMeshSource& mesh, const glm::mat4& transform)
{
	StaticBlasEntry entry;
	entry.cacheKey = (static_cast<uint64_t>(sx) << 32) | static_cast<uint32_t>(sy);
	entry.mesh = mesh;
	entry.transform = transform;
	entry.includeInTlas = true;
	_staticBlas.push_back(entry);
}

uint32_t SceneDescription::registerMeshBlas(uint64_t cacheKey, const BlasMeshSource& mesh)
{
	StaticBlasEntry entry;
	entry.cacheKey = cacheKey;
	entry.mesh = mesh;
	entry.transform = glm::mat4(1.f);
	entry.includeInTlas = false;
	_staticBlas.push_back(entry);
	return static_cast<uint32_t>(_staticBlas.size() - 1);
}

void SceneDescription::addMeshInstance(uint32_t blasIndex, const glm::mat4& transform, bool castsSunShadow)
{
	if (!castsSunShadow)
	{
		return;
	}

	DynamicInstance instance;
	instance.blasIndex = blasIndex;
	instance.transform = transform;
	_instances.push_back(instance);
}
