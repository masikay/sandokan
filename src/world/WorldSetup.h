#pragma once

#include <entt/entt.hpp>

#include "world/Level.h"

// Creates the player and spawns the given level around it
void SetupWorld(entt::registry& registry, const Level& level);
