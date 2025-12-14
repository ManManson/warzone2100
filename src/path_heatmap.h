#pragma once

#include <cstdint>
#include <vector>
#include <shared_mutex>

#include "geometry.h" // Vector2i
#include "map.h"      // map_coord


class PathHeatmap
{
public:
	static PathHeatmap &instance();

	void init(int width, int height);
	void shutdown();

	// Per-tick update (called once per simulation tick)
	void advanceOffset();

	// Core operations
	void addPath(uint32_t droidId, const std::vector<Vector2i> &path, uint32_t maxRelativeHeat = 8);
	// Add heat for a single tile (tile coordinates)
	void addPointHeat(uint32_t droidId, int tileX, int tileY, uint32_t amount = 1);
	uint32_t readRelativeHeatTile(int tileX, int tileY, uint32_t excludeOwner = UINT32_MAX) const;

	void setEnabled(bool enabled) { enabled_ = enabled; }
	bool enabled() const { return enabled_; }

private:

	struct HeatCell
	{
		uint32_t heatValue;
		uint32_t ownerDroidId;
	};

	explicit PathHeatmap() = default;
	// Non-copyable
	PathHeatmap(const PathHeatmap &) = delete;
	PathHeatmap &operator=(const PathHeatmap &) = delete;

	// Internal helpers
	inline unsigned getIndex(int x, int y) const { return (static_cast<unsigned>(y) << widthShift_) | static_cast<unsigned>(x); }
	inline bool isInBounds(int x, int y) const { return x >= 0 && x < mapWidth_ && y >= 0 && y < mapHeight_; }

	mutable std::shared_mutex mtx_;
	uint32_t offset_ = 0;
	bool enabled_ = false;
	int mapWidth_ = 0;
	int mapHeight_ = 0;
	int widthShift_ = 0;
	int stride_ = 0;
	std::vector<HeatCell> heatCells_;
};
