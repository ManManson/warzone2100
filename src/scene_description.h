// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "lib/ivis_opengl/gfx_api.h"

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <vector>

struct SceneDescription
{
	struct BlasMeshSource
	{
		gfx_api::buffer* vertexBuffer = nullptr;
		gfx_api::buffer* indexBuffer = nullptr;
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;
		uint32_t vertexStride = 0;
		uint32_t positionOffset = 0;
		bool uses32BitIndices = true;
		bool alphaTested = false;
	};

	struct StaticBlasEntry
	{
		uint64_t cacheKey = 0;
		BlasMeshSource mesh;
		glm::mat4 transform {1.f};
	};

	struct DynamicInstance
	{
		uint32_t blasIndex = 0;
		glm::mat4 transform {1.f};
		uint32_t instanceCustomIndex = 0;
		uint8_t mask = 0xFF;
	};

	void beginFrame();

	void addTerrainSector(int sx, int sy, const BlasMeshSource& mesh, const glm::mat4& transform);
	void addMeshInstance(uint32_t blasIndex, const glm::mat4& transform, bool castsSunShadow);

	const std::vector<StaticBlasEntry>& staticBlas() const { return _staticBlas; }
	const std::vector<DynamicInstance>& instances() const { return _instances; }

private:
	std::vector<StaticBlasEntry> _staticBlas;
	std::vector<DynamicInstance> _instances;
};
