#pragma once

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

// What an authored level object turns into when spawned
enum class ObjectKind {
    Decor,     // visual only, no collision (backdrops, columns, foliage)
    Platform,  // visual + solid collider (ground, ledges, plain walls)
    GrabWall,  // platform whose face has grab points the player can jump to and hang from
    Slope,     // walkable ramp between slopeStart and slopeEnd (x, ground-surface y)
};

// One editable object of a level. The level is plain data so the editor can change it and
// re-spawn entities from it; entities are never the source of truth.
struct LevelObject {
    ObjectKind kind = ObjectKind::Platform;
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 size{ 1.0f, 1.0f, 1.0f };
    Color color{ 128, 128, 128, 255 };
    std::vector<float> grabHeights; // GrabWall only: ascending, last = the wall's own top
    Vector2 slopeStart{ 0.0f, 0.0f }; // Slope only
    Vector2 slopeEnd{ 0.0f, 0.0f };   // Slope only; size.z is the ramp's depth
    // Optional GLB model drawn instead of the box (Decor, Platform, GrabWall). The box stays
    // as the collider and the editor's pick/selection volume, centered on `position`.
    std::string model;
    Vector3 rotation{ 0.0f, 0.0f, 0.0f }; // degrees around X, Y, Z (the box stays axis-aligned)
    float scale = 1.0f;     // uniform model scale
};

using Level = std::vector<LevelObject>;

// The hand-built test level, used when no level file exists yet
Level BuildDefaultLevel();

bool SaveLevel(const Level& level, const std::string& path);
bool LoadLevel(Level& level, const std::string& path);

// Creates one entity per object, all tagged LevelEntity so they can be cleared again
void SpawnLevel(entt::registry& registry, const Level& level);
void ClearLevelEntities(entt::registry& registry);
