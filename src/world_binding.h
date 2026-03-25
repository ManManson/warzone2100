#pragma once

struct GameWorld;

GameWorld& activeGameWorld();
const GameWorld& activeGameWorldConst();

void bindActiveWorld(GameWorld& world);
bool hasActiveWorld();
