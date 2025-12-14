#include "path_heatmap.h"
#include <algorithm>
#include <limits>
#include <mutex>

// Singleton accessor
PathHeatmap& PathHeatmap::instance()
{
	static PathHeatmap instance;
	return instance;
}

void PathHeatmap::init(int width, int height)
{
	std::unique_lock lock(mtx_);
	if (width <= 0 || height <= 0)
	{
		mapWidth_ = mapHeight_ = 0;
		heatCells_.clear();
		return;
	}

	mapWidth_ = width;
	mapHeight_ = height;

	widthShift_ = 0;
	while ((1u << widthShift_) < static_cast<unsigned>(mapWidth_))
	{
		++widthShift_;
	}
	stride_ = 1 << widthShift_;

	heatCells_.clear();
	heatCells_.resize(static_cast<size_t>(stride_) * static_cast<size_t>(mapHeight_));

	// Initialize cells to zero/none
	for (auto &c : heatCells_)
	{
		c.heatValue = 0;
		c.ownerDroidId = std::numeric_limits<uint32_t>::max();
	}

	offset_ = 0;
}

void PathHeatmap::shutdown()
{
	std::unique_lock lock(mtx_);
	heatCells_.clear();
	mapWidth_ = mapHeight_ = widthShift_ = stride_ = 0;
	offset_ = 0;
	enabled_ = false;
}

void PathHeatmap::advanceOffset()
{
	std::unique_lock lock(mtx_);
	++offset_;
}

void PathHeatmap::addPath(uint32_t droidId, const std::vector<Vector2i> &path, uint32_t maxRelativeHeat)
{
	if (path.empty())
	{
		return;
	}
	if (!enabled_)
	{
		return;
	}
	if (mapWidth_ == 0 || mapHeight_ == 0)
	{
		return;
	}

	std::unique_lock lock(mtx_);
	uint32_t offset = offset_;
	size_t N = path.size();
	unsigned lastIdx = static_cast<unsigned>(-1);

	for (size_t i = 0; i < N; ++i)
	{
		int tx = map_coord(path[i].x);
		int ty = map_coord(path[i].y);
		if (!isInBounds(tx, ty))
		{
			continue;
		}
		unsigned idx = getIndex(tx, ty);
		if (idx == lastIdx)
		{
			continue; // avoid duplicate writes
		}
		lastIdx = idx;

		uint32_t rel = std::max<uint32_t>(1, (maxRelativeHeat * (N - i)) / N);
		uint32_t storeVal = offset + rel;

		auto& cell = heatCells_[idx];
		if (cell.heatValue < storeVal)
		{
			cell.heatValue = storeVal;
			cell.ownerDroidId = droidId;
		}
	}
}

void PathHeatmap::addPointHeat(uint32_t droidId, int tileX, int tileY, uint32_t amount)
{
	if (!enabled_)
	{
		return;
	}
	if (!isInBounds(tileX, tileY))
	{
		return;
	}

	std::unique_lock lock(mtx_);
	unsigned idx = getIndex(tileX, tileY);
	uint32_t storeVal = offset_ + amount;
	auto& cell = heatCells_[idx];
	if (cell.heatValue < storeVal)
	{
		cell.heatValue = storeVal;
		cell.ownerDroidId = droidId;
	}
}

uint32_t PathHeatmap::readRelativeHeatTile(int tileX, int tileY, uint32_t excludeOwner) const
{
	if (!enabled_)
	{
		return 0;
	}
	if (!isInBounds(tileX, tileY))
	{
		return 0;
	}

	std::shared_lock lock(mtx_);
	unsigned idx = getIndex(tileX, tileY);
	const auto cell = heatCells_[idx]; // copy
	uint32_t offset = offset_;
	lock.unlock();

	if (cell.ownerDroidId == excludeOwner)
	{
		return 0;
	}
	if (cell.heatValue <= offset)
	{
		return 0;
	}
	return cell.heatValue - offset;
}
