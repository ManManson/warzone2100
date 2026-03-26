#pragma once

#include <memory>
#include <stdint.h>

struct MAPTILE;
struct GameWorld;
struct WorldScrollLimits;

GameWorld& activeGameWorld();
const GameWorld& activeGameWorldConst();
GameWorld& primaryGameWorld();

void bindActiveWorld(GameWorld& world);
void unbindActiveWorld();
bool hasActiveWorld();
bool isActiveWorld(const GameWorld& world);
bool isActiveWorld(const GameWorld* world);

std::unique_ptr<MAPTILE[]>& activeMapTiles();
const std::unique_ptr<MAPTILE[]>& activeMapTilesConst();

int32_t& activeMapWidth();
const int32_t& activeMapWidthConst();

int32_t& activeMapHeight();
const int32_t& activeMapHeightConst();

std::unique_ptr<uint8_t[]>& activeBlockMap(int slot);
const std::unique_ptr<uint8_t[]>& activeBlockMapConst(int slot);

std::unique_ptr<uint8_t[]>& activeAuxMap(int slot);
const std::unique_ptr<uint8_t[]>& activeAuxMapConst(int slot);

WorldScrollLimits& activeScrollLimits();
const WorldScrollLimits& activeScrollLimitsConst();
