/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2020  Warzone 2100 Project

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
/** @file
 *  Definitions for the map structure
 */

#ifndef __INCLUDED_SRC_MAP_H__
#define __INCLUDED_SRC_MAP_H__

#include "lib/framework/frame.h"
#include "lib/framework/debug.h"
#include <wzmaplib/map.h>
#include <wzmaplib/terrain_type.h>
#include "objects.h"
#include "terrain.h"
#include "multiplay.h"
#include "display.h"
#include "ai.h"

#include <memory>
#include "gateway.h"

#define TALLOBJECT_YMAX		(200)
#define TALLOBJECT_ADJUST	(300)

#define BITS_MARKED             0x01    ///< Is this tile marked?
#define BITS_DECAL              0x02    ///< Does this tile has a decal? If so, the tile from "texture" is drawn on top of the terrain.
#define BITS_FPATHBLOCK         0x10    ///< Bit set temporarily by find path to mark a blocking tile
#define BITS_ON_FIRE            0x20    ///< Whether tile is burning
#define BITS_GATEWAY            0x40    ///< Bit set to show a gateway on the tile

struct GROUND_TYPE
{
	std::string textureName;
	float textureSize;
	std::string normalMapTextureName = "";
	std::string specularMapTextureName = "";
	std::string heightMapTextureName = "";
	bool highQualityTextures = false; // whether this ground_type has normal / specular / height maps
};

/* Information stored with each tile */
struct MAPTILE
{
	uint8_t         tileInfoBits;
	PlayerMask      tileExploredBits;
	PlayerMask      sensorBits;             ///< bit per player, who can see tile with sensor
	uint16_t        watchers[MAX_PLAYERS];  // player sees through fog of war here with this many objects
	uint16_t        texture;                // Which graphics texture is on this tile
	int32_t         height;                 ///< The height at the top left of the tile
	BASE_OBJECT *   psObject;               // Any object sitting on the location (e.g. building)
	uint16_t        limitedContinent;       ///< For land or sea limited propulsion types
	uint16_t        hoverContinent;         ///< For hover type propulsions
	uint16_t        fireEndTime;            ///< The (uint16_t)(gameTime / GAME_TICKS_PER_UPDATE) that BITS_ON_FIRE should be cleared.
	int32_t         waterLevel;             ///< At what height is the water for this tile
	PlayerMask      jammerBits;             ///< bit per player, who is jamming tile
	uint16_t        sensors[MAX_PLAYERS];   ///< player sees this tile with this many radar sensors
	uint16_t        jammers[MAX_PLAYERS];   ///< player jams the tile with this many objects

	// DISPLAY ONLY (NOT for use in game calculations)
	uint8_t         ground;                 ///< The ground type used for the terrain renderer
	uint8_t         illumination;           // How bright is this tile? = diffuseSunLight * ambientOcclusion
	uint8_t			ambientOcclusion;		// ambient occlusion. from 1 (max occlusion) to 254 (no occlusion), similar to illumination.
	float           level;                  ///< The visibility level of the top left of the tile, for this client. for terrain lightmap
};

extern float waterLevel;
extern char *tilesetDir;
extern MAP_TILESET currentMapTileset;

const GROUND_TYPE& getGroundType(size_t idx);
size_t getNumGroundTypes();
#define MAX_GROUND_TYPES 12

#define AIR_BLOCKED		0x01	///< Aircraft cannot pass tile
#define FEATURE_BLOCKED		0x02	///< Ground units cannot pass tile due to item in the way
#define WATER_BLOCKED		0x04	///< Units that cannot pass water are blocked by this tile
#define LAND_BLOCKED		0x08	///< The inverse of the above -- for propeller driven crafts

#define AUXBITS_NONPASSABLE     0x01    ///< Is there any building blocking here, other than a gate that would open for us?
#define AUXBITS_OUR_BUILDING	0x02	///< Do we or our allies have a building at this tile
#define AUXBITS_BLOCKING        0x04    ///< Is there any building currently blocking here?
#define AUXBITS_TEMPORARY	0x08	///< Temporary bit used in calculations
#define AUXBITS_DANGER		0x10	///< Does AI sense danger going there?
#define AUXBITS_THREAT		0x20	///< Can hostile players shoot here?
#define AUXBITS_AATHREAT	0x40	///< Can hostile players shoot at my VTOLs here?
#define AUXBITS_UNUSED          0x80    ///< Unused
#define AUXBITS_ALL		0xff

#define AUX_MAP		0
#define AUX_ASTARMAP	1
#define AUX_DANGERMAP	2
#define AUX_MAX		3

struct WorldScrollLimits
{
	int32_t minX = 0;
	int32_t minY = 0;
	int32_t maxX = 0;
	int32_t maxY = 0;
};

struct WorldMapState
{
	std::unique_ptr<MAPTILE[]> tiles;
	int32_t width = 0;
	int32_t height = 0;
	std::unique_ptr<uint8_t[]> blockMap[AUX_MAX];
	std::unique_ptr<uint8_t[]> auxMap[MAX_PLAYERS + AUX_MAX];
	WorldScrollLimits scroll;
	GATEWAY_LIST gateways;
};

struct GameWorld;

/** Session active world; defined in gamesessionworlds.cpp. Used with map APIs at call sites that do not thread GameWorld &. */
GameWorld &activeGameWorld();

MAPTILE *mapTile(GameWorld &world, int32_t x, int32_t y);
MAPTILE *mapTile(GameWorld &world, Vector2i const &v);
MAPTILE *worldTile(GameWorld &world, int32_t x, int32_t y);
MAPTILE *worldTile(GameWorld &world, Vector2i const &v);

uint8_t auxTile(const GameWorld &world, int x, int y, int player);
uint8_t blockTile(const GameWorld &world, int x, int y, int slot);

void auxMapStore(GameWorld &world, int player, int slot);
void auxMapRestore(GameWorld &world, int player, int slot, int mask);
void auxSet(GameWorld &world, int x, int y, int player, int state);
void auxSetAll(GameWorld &world, int x, int y, int state);
void auxSetAllied(GameWorld &world, int x, int y, int player, int state);
void auxSetEnemy(GameWorld &world, int x, int y, int player, int state);
void auxClear(GameWorld &world, int x, int y, int player, int state);
void auxClearAll(GameWorld &world, int x, int y, int state);
void auxSetBlocking(GameWorld &world, int x, int y, int state);
void auxClearBlocking(GameWorld &world, int x, int y, int state);

bool TileIsOccupied(const MAPTILE *tile);
bool TileIsKnownOccupied(MAPTILE const *tile, unsigned player);
bool TileHasStructure(const MAPTILE *tile);
bool TileHasFeature(const MAPTILE *tile);
bool TileHasWall(const MAPTILE *tile);
bool TileHasWall_raycast(const MAPTILE *tile);
bool TileIsBurning(const MAPTILE *tile);
bool tileIsExplored(const MAPTILE *psTile);
bool tileIsClearlyVisible(const MAPTILE *psTile);
bool TileHasSmallStructure(const MAPTILE *tile);

#define SET_TILE_DECAL(x)	((x)->tileInfoBits |= BITS_DECAL)
#define CLEAR_TILE_DECAL(x)	((x)->tileInfoBits &= ~BITS_DECAL)
#define TILE_HAS_DECAL(x)	((x)->tileInfoBits & BITS_DECAL)

/* Allows us to do if(TRI_FLIPPED(psTile)) */
#define TRI_FLIPPED(x)		((x)->texture & TILE_TRIFLIP)
/* Flips the triangle partition on a tile pointer */
#define TOGGLE_TRIFLIP(x)	((x)->texture ^= TILE_TRIFLIP)

/* Can player number p has explored tile t? */
#define TEST_TILE_VISIBLE(p,t)	((t)->tileExploredBits & (1<<(p)))

bool TEST_TILE_VISIBLE_TO_SELECTEDPLAYER(MAPTILE *pTile);

/* Set a tile to be visible for a player */
#define SET_TILE_VISIBLE(p,t) ((t)->tileExploredBits |= alliancebits[p])

/* Arbitrary maximum number of terrain textures - used in look up table for terrain type */
#define MAX_TILE_TEXTURES	255

extern UBYTE terrainTypes[MAX_TILE_TEXTURES];

unsigned char terrainType(const MAPTILE *tile);

Vector2i world_coord(Vector2i const &mapCoord);
Vector2i map_coord(Vector2i const &worldCoord);
Vector2i round_to_nearest_tile(Vector2i const &worldCoord);

void clip_world_offmap(GameWorld &world, int *worldX, int *worldY);

bool mapShutdown(GameWorld &world);

bool mapLoad(GameWorld &world, char const *filename);
struct ScriptMapData;
bool loadTerrainTypeMap(const std::shared_ptr<WzMap::TerrainTypeData>& ttypeData);
bool mapLoadFromWzMapData(GameWorld &world, std::shared_ptr<WzMap::MapData> mapData);

bool mapReloadGroundTypes(GameWorld &world);

class WzMapPhysFSIO : public WzMap::IOProvider
{
public:
	WzMapPhysFSIO() { }
	WzMapPhysFSIO(const std::string& baseMountPath)
	: m_basePath(baseMountPath)
	{ }
public:
	virtual std::unique_ptr<WzMap::BinaryIOStream> openBinaryStream(const std::string& filename, WzMap::BinaryIOStream::OpenMode mode) override;
	virtual WzMap::IOProvider::LoadFullFileResult loadFullFile(const std::string& filename, std::vector<char>& fileData, uint32_t maxFileSize = 0, bool appendNullCharacter = false) override;
	virtual bool writeFullFile(const std::string& filename, const char *ppFileData, uint32_t fileSize) override;
	virtual bool makeDirectory(const std::string& directoryPath) override;
	virtual const char* pathSeparator() const override;
	virtual bool fileExists(const std::string& filename) override;

	bool folderExists(const std::string& dirPath);

	virtual bool enumerateFiles(const std::string& basePath, const std::function<bool (const char* file)>& enumFunc) override;
	virtual bool enumerateFolders(const std::string& basePath, const std::function<bool (const char* file)>& enumFunc) override;
private:
	std::string m_basePath;
};

class WzMapDebugLogger : public WzMap::LoggingProtocol
{
public:
	virtual ~WzMapDebugLogger();
	virtual void printLog(WzMap::LoggingProtocol::LogLevel level, const char *function, int line, const char *str) override;
};

bool mapSaveToWzMapData(GameWorld &world, WzMap::MapData& output);

int32_t map_TileHeight(const GameWorld &world, int32_t x, int32_t y);
int32_t map_WaterHeight(const GameWorld &world, int32_t x, int32_t y);
int32_t map_TileHeightSurface(const GameWorld &world, int32_t x, int32_t y);

void setTileHeight(GameWorld &world, int32_t x, int32_t y, int32_t height);

bool tileOnMap(const GameWorld &world, SDWORD x, SDWORD y);
bool tileOnMap(const GameWorld &world, Vector2i pos);

bool worldOnMap(const GameWorld &world, int x, int y);
bool worldOnMap(const GameWorld &world, Vector2i pos);

void makeTileRubbleTexture(GameWorld &world, MAPTILE *psTile, unsigned int x, unsigned int y, unsigned int newTexture);

bool map_Intersect(GameWorld &world, int *Cx, int *Cy, int *Vx, int *Vy, int *Sx, int *Sy);

unsigned map_LineIntersect(GameWorld &world, Vector3i src, Vector3i dst, unsigned tMax);

int32_t map_Height(GameWorld &world, int x, int y);
int32_t map_Height(GameWorld &world, Vector2i const &v);

bool mapObjIsAboveGround(GameWorld &world, const SIMPLE_OBJECT *psObj);

void getTileMaxMin(GameWorld &world, int x, int y, int *pMax, int *pMin);

bool readVisibilityData(GameWorld &world, const char *fileName);
bool writeVisibilityData(GameWorld &world, const char *fileName);

void mapFloodFillContinents(GameWorld &world);

void tileSetFire(GameWorld &world, int32_t x, int32_t y, uint32_t duration);
bool fireOnLocation(GameWorld &world, unsigned int x, unsigned int y);

bool hasSensorOnTile(MAPTILE *psTile, unsigned player);

void mapInit(GameWorld &world);
void mapUpdate(GameWorld &world);

bool shouldLoadTerrainTypeOverrides(const std::string& name);
bool loadTerrainTypeMapOverride(MAP_TILESET tileSet);

//For saves to determine if loading the terrain type override should occur
extern bool builtInMap;
extern bool useTerrainOverrides;

#endif // __INCLUDED_SRC_MAP_H__
