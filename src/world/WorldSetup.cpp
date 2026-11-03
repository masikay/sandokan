#include "world/WorldSetup.h"

#include "ecs/Components.h"

void SetupWorld(entt::registry& registry, const Level& level)
{
    // The collision box is a single 1x2x1 AABB (see the PLAYER_HALF_EXTENTS constants in the
    // physics systems); the body-part figure is drawn by RenderSystem. y = 1.0 so the box
    // rests with its bottom on the ground.
    auto player = registry.create();
    registry.emplace<Transform3D>(player, Vector3{ 0.0f, 1.0f, 0.0f });
    registry.emplace<Velocity>(player);
    registry.emplace<PlayerControlled>(player);
    registry.emplace<CameraTarget>(player);

    SpawnLevel(registry, level);
}
