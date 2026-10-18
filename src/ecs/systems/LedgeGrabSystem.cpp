#include "LedgeGrabSystem.h"

#include <cmath>

#include <raylib.h>

#include "../Components.h"

namespace
{
    // kept in sync with CollisionSystem/SlopeSystem's player half-extents until the player
    // has its own Collider component
    constexpr Vector3 PLAYER_HALF_EXTENTS{ 0.5f, 1.0f, 0.5f };
    constexpr float GRAB_MARGIN = 0.2f;
    // how far above the player's head a grab point can still be caught - checked throughout
    // the whole rising arc of a jump, not just at its peak, so the real reach of one jump is
    // roughly (max jump height + player height + this), comfortably more than it sounds
    constexpr float GRAB_REACH = 1.5f;
    // how fast the player climbs over the top (mantling), or down between handholds - a
    // slow, deliberate motion rather than an instant snap/teleport or a free fall
    constexpr float MANTLE_SPEED = 3.0f;
    // how close the player's feet must be to a grab point's height, while standing still, to
    // step back onto it and begin a controlled descent
    constexpr float STEP_DOWN_TOLERANCE = 0.3f;

    bool IsPressingUp()
    {
        return IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
    }

    bool IsPressingDown()
    {
        return IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);
    }

    // finds, among all GrabPoints walls, the highest *intermediate* grab point the player's
    // feet are standing at (within STEP_DOWN_TOLERANCE) and horizontally touching; used to
    // step back onto a wall from standing nearby, to begin climbing down. The wall's own top
    // point is deliberately excluded: standing right next to a ledge must not let the player
    // grab back onto it just by pressing down - walking off the edge (a real fall, caught
    // immediately by the same point) is the only way to begin a descent from a ledge.
    bool FindStandingGrabPoint(entt::registry& registry, const Transform3D& transform,
                                entt::entity& outWall, float& outHeight)
    {
        float playerFeet = transform.position.y - PLAYER_HALF_EXTENTS.y;
        bool found = false;

        auto walls = registry.view<Transform3D, Collider, GrabPoints>();
        for (auto wallEntity : walls)
        {
            auto& wallTransform = walls.get<Transform3D>(wallEntity);
            auto& collider = walls.get<Collider>(wallEntity);
            auto& grabPoints = walls.get<GrabPoints>(wallEntity);

            float xGap = std::abs(transform.position.x - wallTransform.position.x)
                - (PLAYER_HALF_EXTENTS.x + collider.halfExtents.x);
            if (xGap > GRAB_MARGIN) continue;

            float wallTop = wallTransform.position.y + collider.halfExtents.y;

            for (float height : grabPoints.heights)
            {
                if (std::abs(height - wallTop) < 0.01f) continue; // the wall's own top: skip

                if (std::abs(height - playerFeet) <= STEP_DOWN_TOLERANCE
                    && (!found || height > outHeight))
                {
                    outWall = wallEntity;
                    outHeight = height;
                    found = true;
                }
            }
        }
        return found;
    }

}

void LedgeGrabSystem(entt::registry& registry, float deltaTime)
{
    auto players = registry.view<Transform3D, Velocity, PlayerControlled>();

    for (auto playerEntity : players)
    {
        auto& transform = players.get<Transform3D>(playerEntity);
        auto& velocity = players.get<Velocity>(playerEntity);
        auto& control = players.get<PlayerControlled>(playerEntity);

        if (auto* hang = registry.try_get<LedgeHang>(playerEntity))
        {
            // hanging: the topmost grab point can be mantled with up; any point (including
            // the topmost) can be released downward with down, or jumped onward from with
            // space from an intermediate one. Only jumping/releasing actually detaches into
            // a real fall - and once that fall starts, nothing can pause it mid-air between
            // two handholds; it always ends at the next lower grab point or on the ground.
            if (hang->canMantle && (hang->mantling || IsPressingUp()))
            {
                // climb straight up, slowly. The moment standing height is reached, the arms
                // lower right away (LedgeHang removed) - any last small sideways correction
                // onto solid footing is purely cosmetic and handled separately afterwards
                // (WallSettle below), so it never delays lowering the arms. Once started
                // (mantling = true), the climb always runs to completion even if up is
                // released early, instead of leaving the player stuck hanging mid-climb with
                // no other control working.
                hang->mantling = true;
                float targetY = hang->grabbedHeight + PLAYER_HALF_EXTENTS.y;
                float newY = transform.position.y + MANTLE_SPEED * deltaTime;
                velocity.value = { 0.0f, 0.0f, 0.0f };

                if (newY >= targetY)
                {
                    transform.position.y = targetY;

                    if (registry.all_of<Transform3D, Collider>(hang->wall))
                    {
                        auto& wallTransform = registry.get<Transform3D>(hang->wall);
                        auto& wallCollider = registry.get<Collider>(hang->wall);
                        // the wall's near edge: the start of its top, right where the climb
                        // crests, not the far edge facing the ledge
                        float outerSide = (transform.position.x < wallTransform.position.x) ? -1.0f : 1.0f;
                        float targetX = wallTransform.position.x
                            + outerSide * (wallCollider.halfExtents.x - PLAYER_HALF_EXTENTS.x - 0.1f);
                        registry.emplace<WallSettle>(playerEntity, targetX);
                    }

                    registry.remove<LedgeHang>(playerEntity);
                }
                else
                {
                    transform.position.y = newY;
                }
            }
            else if (!hang->canMantle && IsKeyPressed(KEY_SPACE))
            {
                // jump towards the next grab point up; remember the one just released so
                // it isn't instantly re-caught before the player has actually moved away
                registry.emplace_or_replace<RecentRelease>(playerEntity, hang->grabbedHeight);
                velocity.value = { 0.0f, control.jumpSpeed, 0.0f };
                registry.remove<LedgeHang>(playerEntity);
            }
            else if (IsKeyPressed(KEY_SPACE))
            {
                // deliberately jumping from the topmost point: detach and actually fall,
                // unlike the careful climb-down below
                registry.emplace_or_replace<RecentRelease>(playerEntity, hang->grabbedHeight);
                registry.remove<LedgeHang>(playerEntity);
            }
            else if (IsPressingDown())
            {
                // let go downward: remembering this point stops it being instantly re-caught,
                // so real gravity carries the player down to the next lower grab point (if
                // any) or to the ground - an actual fall between handholds, never a pause
                registry.emplace_or_replace<RecentRelease>(playerEntity, hang->grabbedHeight);
                registry.remove<LedgeHang>(playerEntity);
            }
            else
            {
                velocity.value = { 0.0f, 0.0f, 0.0f };
            }
            continue;
        }

        if (auto* settle = registry.try_get<WallSettle>(playerEntity))
        {
            // last small cosmetic correction after mantling: slide onto solid footing. The
            // move direction is derived from the actual gap to the target, not assumed from
            // which side of the wall the player is on - the grab position can already be
            // slightly past the target (within GRAB_MARGIN), and assuming the wrong sign
            // there made the "reached" check true on the very first frame, snapping instead
            // of sliding.
            float moveDir = (transform.position.x < settle->targetX) ? 1.0f : -1.0f;
            float newX = transform.position.x + moveDir * MANTLE_SPEED * deltaTime;
            bool reachedX = (moveDir > 0.0f) ? (newX >= settle->targetX) : (newX <= settle->targetX);

            transform.position.x = reachedX ? settle->targetX : newX;
            velocity.value.x = 0.0f;

            if (reachedX) registry.remove<WallSettle>(playerEntity);
            continue;
        }

        if (velocity.value.y == 0.0f)
        {
            // grounded: step back onto one of a wall's *intermediate* grab points from
            // standing right next to it (never the wall's own top - see
            // FindStandingGrabPoint), to begin a controlled descent from partway up, instead
            // of only being able to climb down by walking off an edge and falling
            entt::entity wall;
            float height;
            if (IsPressingDown() && FindStandingGrabPoint(registry, transform, wall, height))
            {
                transform.position.y = height - PLAYER_HALF_EXTENTS.y;
                velocity.value = { 0.0f, 0.0f, 0.0f };
                registry.emplace<LedgeHang>(playerEntity, height, false, wall);
            }

            registry.remove<RecentRelease>(playerEntity);
            continue;
        }

        const RecentRelease* recentRelease = registry.try_get<RecentRelease>(playerEntity);

        float playerFeet = transform.position.y - PLAYER_HALF_EXTENTS.y;
        float playerHead = transform.position.y + PLAYER_HALF_EXTENTS.y;

        auto walls = registry.view<Transform3D, Collider, GrabPoints>();
        bool grabbed = false;
        for (auto wallEntity : walls)
        {
            if (grabbed) break;

            auto& wallTransform = walls.get<Transform3D>(wallEntity);
            auto& collider = walls.get<Collider>(wallEntity);
            auto& grabPoints = walls.get<GrabPoints>(wallEntity);

            float xGap = std::abs(transform.position.x - wallTransform.position.x)
                - (PLAYER_HALF_EXTENTS.x + collider.halfExtents.x);
            if (xGap > GRAB_MARGIN) continue;

            float wallTop = wallTransform.position.y + collider.halfExtents.y;

            // catch the lowest grab point that is both above the player's feet (otherwise
            // they would already be standing on or below it) and within reach
            for (std::size_t i = 0; i < grabPoints.heights.size(); ++i)
            {
                float height = grabPoints.heights[i];
                // skip only the exact point just released, not every point below it (that
                // bug made the player fall straight through every lower handhold at once)
                if (recentRelease != nullptr && std::abs(height - recentRelease->height) < 0.01f) continue;

                if (height > playerFeet && height <= playerHead + GRAB_REACH)
                {
                    transform.position.y = height - PLAYER_HALF_EXTENTS.y;
                    velocity.value = { 0.0f, 0.0f, 0.0f };

                    bool isLast = (i == grabPoints.heights.size() - 1);
                    bool matchesTop = std::abs(height - wallTop) < 0.01f;
                    registry.emplace<LedgeHang>(playerEntity, height, isLast && matchesTop, wallEntity);
                    registry.remove<RecentRelease>(playerEntity);

                    grabbed = true;
                    break;
                }
            }
        }
    }
}
