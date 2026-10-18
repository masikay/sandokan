#include "CollisionSystem.h"

#include <algorithm>

#include "../Components.h"

namespace
{
    struct AABB {
        float minX, maxX, minY, maxY;
    };

    AABB MakeAABB(const Vector3& center, const Vector3& halfExtents)
    {
        return { center.x - halfExtents.x, center.x + halfExtents.x,
                 center.y - halfExtents.y, center.y + halfExtents.y };
    }

    float OverlapX(const AABB& a, const AABB& b)
    {
        return std::min(a.maxX, b.maxX) - std::max(a.minX, b.minX);
    }

    float OverlapY(const AABB& a, const AABB& b)
    {
        return std::min(a.maxY, b.maxY) - std::max(a.minY, b.minY);
    }

    // half-extents of the player's collision box; kept here until the player has its own
    // Collider component (not needed yet, since only one entity is ever the player)
    constexpr Vector3 PLAYER_HALF_EXTENTS{ 0.5f, 1.0f, 0.5f };
}

void CollisionSystem(entt::registry& registry)
{
    auto players = registry.view<Transform3D, Velocity, PlayerControlled>();
    auto platforms = registry.view<Transform3D, Collider>();

    for (auto playerEntity : players)
    {
        auto& playerTransform = players.get<Transform3D>(playerEntity);
        auto& playerVelocity = players.get<Velocity>(playerEntity);

        for (auto platformEntity : platforms)
        {
            auto& platformTransform = platforms.get<Transform3D>(platformEntity);
            auto& collider = platforms.get<Collider>(platformEntity);

            AABB playerBox = MakeAABB(playerTransform.position, PLAYER_HALF_EXTENTS);
            AABB platformBox = MakeAABB(platformTransform.position, collider.halfExtents);

            float overlapX = OverlapX(playerBox, platformBox);
            float overlapY = OverlapY(playerBox, platformBox);

            if (overlapX <= 0.0f || overlapY <= 0.0f) continue;

            // push the player out along the axis with the smaller overlap: this resolves
            // landing on top, hitting an underside, and side contact against ledges/walls
            if (overlapX < overlapY)
            {
                float pushDir = (playerTransform.position.x < platformTransform.position.x) ? -1.0f : 1.0f;
                playerTransform.position.x += pushDir * overlapX;
                playerVelocity.value.x = 0.0f;
            }
            else
            {
                float pushDir = (playerTransform.position.y < platformTransform.position.y) ? -1.0f : 1.0f;
                playerTransform.position.y += pushDir * overlapY;
                playerVelocity.value.y = 0.0f;
            }
        }
    }
}
