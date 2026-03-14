#ifdef DEBUG

#include "steering_debug_info.h"
#include "debug_rings.h"

#include "lib/ivis_opengl/piepalette.h"
#include "lib/gamelib/gtime.h"

#include "../droiddef.h"
#include "../move.h"
#include "../map.h"
#include "../display3d.h"

namespace steering
{

// Fills out with one ring per visible droid when steering debug is enabled.
void buildSteeringRings(std::vector<SteeringRingDebug>& outRings)
{
	outRings.clear();
	outRings.reserve(128);
	if (!isDebugOverlayEnabled())
	{
		return;
	}
	for (unsigned player = 0; player < MAX_PLAYERS; ++player)
	{
		for (const DROID *psDroid : apsDroidLists[player])
		{
			if (!psDroid)
			{
				continue;
			}
			if (psDroid->died != 0 && psDroid->died < graphicsTime)
			{
				continue;
			}
			if (!clipXY(psDroid->pos.x, psDroid->pos.y))
			{
				continue;
			}
			const SteeringDebugInfo& dbg = psDroid->steeringDebug;
			if (dbg.frameNum == 0)
			{
				continue;
			}
			if (outRings.size() >= 128)
			{
				return;
			}
			SteeringRingDebug ring;
			// Terrain vertices use (x, z) = (worldX, world_coord(-tileY)).
			// Match that by flipping the droid's world Y when storing center.y.
			Spacetime st = interpolateObjectSpacetime(psDroid, graphicsTime);
			ring.center.x = st.pos.x;
			ring.center.y = -st.pos.y;
			// Outer radius matches droid collision radius.
			ring.radius = moveObjRadius(psDroid);
			ring.thickness = TILE_UNITS / 8;// TILE_UNITS / 96;
			ring.color = glm::vec4(0, 255, 0, 192);           // semi‑transparent green
			outRings.emplace_back(std::move(ring));
		}
	}
}
} // namespace steering

#endif // DEBUG
