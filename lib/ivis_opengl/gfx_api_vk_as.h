// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(WZ_VULKAN_ENABLED)

#include "gfx_api.h"

#include <cstdint>

struct SceneDescription;

class VkRoot;

class VkASManager
{
public:
	VkASManager() = default;
	~VkASManager();

	VkASManager(const VkASManager&) = delete;
	VkASManager& operator=(const VkASManager&) = delete;

	void init(VkRoot& root);
	void shutdown();
	void build(const SceneDescription& scene);

	gfx_api::GfxCapabilities capabilities() const { return _capabilities; }

private:
	struct Impl;
	Impl* _impl = nullptr;
	VkRoot* _root = nullptr;
	gfx_api::GfxCapabilities _capabilities {};
	bool _initialized = false;
};

#endif // defined(WZ_VULKAN_ENABLED)
