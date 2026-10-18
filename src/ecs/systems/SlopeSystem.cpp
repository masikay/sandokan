#include "SlopeSystem.h"

#include <algorithm>

#include "../Components.h"

namespace
{
    // kept in sync with CollisionSystem's player half-extents until the player has its own
    // Collider component
    constexpr float PLAYER_HALF_HEIGHT = 1.0f;

    // on a downward slope, snapping only when the player has already sunk below the surface
    // lags behind: velocity resets to 0 on every snap, so gravity re-accelerates from rest
    // each frame while the slope's required drop keeps growing, until it finally overtakes
    // and snaps hard - a repeating "stairstep" bounce. This tolerance keeps the player glued
    // to the surface a little above it too, well under a real jump's clearance, so walking
    // down a slope tracks it continuously instead of alternately floating and catching up.
    constexpr float SNAP_TOLERANCE = 1.0f;
}

void SlopeSystem(entt::registry& registry)
{
    auto players = registry.view<Transform3D, Velocity, PlayerControlled>();
    auto slopes = registry.view<Slope>();

    for (auto playerEntity : players)
    {
        auto& transform = players.get<Transform3D>(playerEntity);
        auto& velocity = players.get<Velocity>(playerEntity);

        // only snap onto a slope while falling or resting; while actively jumping (positive
        // vertical velocity) the player must be able to leave the surface
        if (velocity.value.y > 0.0f) continue;

        for (auto slopeEntity : slopes)
        {
            auto& slope = slopes.get<Slope>(slopeEntity);

            float minX = std::min(slope.start.x, slope.end.x);
            float maxX = std::max(slope.start.x, slope.end.x);
            if (transform.position.x < minX || transform.position.x > maxX) continue;

            float t = (transform.position.x - slope.start.x) / (slope.end.x - slope.start.x);
            float groundY = slope.start.y + (slope.end.y - slope.start.y) * t;
            float standY = groundY + PLAYER_HALF_HEIGHT;

            // snap onto the slope surface whenever the player is at, below, or only slightly
            // above it (within the tolerance) - covers both climbing into an upward slope
            // and tracking a downward one without lagging behind it
            if (transform.position.y - standY <= SNAP_TOLERANCE)
            {
                transform.position.y = standY;
                velocity.value.y = 0.0f;
            }
        }
    }
}
