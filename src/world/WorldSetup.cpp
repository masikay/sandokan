#include "WorldSetup.h"

#include <vector>

#include "../ecs/Components.h"

namespace
{
    // an [min, max] span of x to skip when scattering ground-level decoration
    struct XRange {
        float min;
        float max;
    };

    // ranges with no ground at y = 0 (chasms, and the suspended wall/ledge bridge): a
    // decoration placed at "ground height" there would visibly float over open air
    const std::vector<XRange> kNoGroundRanges = {
        { -40.0f, -34.0f }, // left chasm
        { 20.0f, 26.0f },   // right chasm
        { 52.0f, 92.0f },   // ledges/chasm bridge between the two handhold walls (their own
                            // footprints at 48-52 and 92-96 now have real ground beneath them)
    };

    bool IsInAnyRange(float x, const std::vector<XRange>& ranges)
    {
        for (const auto& range : ranges)
        {
            if (x >= range.min && x <= range.max) return true;
        }
        return false;
    }

    // creates a static placeholder backdrop at a fixed depth; backdrops further from the
    // gameplay plane (z = 0) will appear to move slower under the perspective camera
    void CreateParallaxLayer(entt::registry& registry, float depthZ, Vector3 size, Color color)
    {
        auto entity = registry.create();
        registry.emplace<Transform3D>(entity, Vector3{ 0.0f, size.y * 0.5f, depthZ });
        registry.emplace<BoxRenderable>(entity, size, color);
    }

    // creates a row of evenly spaced placeholder columns along x, at a given depth. Used both
    // as parallax reference markers (behind the player) and as foreground dressing (in front
    // of the player): a column placed at z > 0, between the camera and the gameplay plane,
    // will cover the player whenever its world x lines up with the player's. Columns whose x
    // falls in `skip` are omitted - used to keep "ground height" decoration out of chasms and
    // other spots where there is no actual ground to stand on.
    void CreateColumnRow(entt::registry& registry, float depthZ, float spacing, int count,
                          Vector3 size, Color color, const std::vector<XRange>& skip = {})
    {
        float halfSpan = spacing * static_cast<float>(count - 1) * 0.5f;
        for (int i = 0; i < count; ++i)
        {
            float x = static_cast<float>(i) * spacing - halfSpan;
            if (IsInAnyRange(x, skip)) continue;

            auto column = registry.create();
            registry.emplace<Transform3D>(column, Vector3{ x, size.y * 0.5f, depthZ });
            registry.emplace<BoxRenderable>(column, size, color);
        }
    }

    // creates a solid platform on the gameplay plane (z = 0): visible and collidable, so the
    // player can stand on, jump onto, or bump into it
    void CreatePlatform(entt::registry& registry, Vector3 position, Vector3 size, Color color)
    {
        auto platform = registry.create();
        registry.emplace<Transform3D>(platform, position);
        registry.emplace<BoxRenderable>(platform, size, color);
        registry.emplace<Collider>(platform, Vector3{ size.x * 0.5f, size.y * 0.5f, size.z * 0.5f });
    }

    // creates a walkable ramp between two ground-surface points; SlopeSystem follows it
    // automatically, so the player climbs/descends it just by walking, no jump required
    void CreateSlope(entt::registry& registry, Vector2 start, Vector2 end, Color color)
    {
        auto slope = registry.create();
        registry.emplace<Slope>(slope, start, end, 2.0f, color);
    }

    // creates a wall scaled only by jumping (LedgeGrabSystem), never by holding a direction.
    // `grabHeights` lists its grabbable points bottom to top; the last one must equal the
    // wall's own top surface (position.y + size.y / 2) so the player can mantle onto it.
    // A short/medium wall needs only that one entry; a tall "handhold" wall lists several,
    // each within a jump's reach of the previous one.
    void CreateGrabWall(entt::registry& registry, Vector3 position, Vector3 size, Color color,
                         std::vector<float> grabHeights)
    {
        auto wall = registry.create();
        registry.emplace<Transform3D>(wall, position);
        registry.emplace<BoxRenderable>(wall, size, color);
        registry.emplace<Collider>(wall, Vector3{ size.x * 0.5f, size.y * 0.5f, size.z * 0.5f });
        registry.emplace<GrabPoints>(wall, std::move(grabHeights));
    }
}

void SetupWorld(entt::registry& registry)
{
    // player: driven by input, followed by the camera, drawn as a body-part figure by
    // RenderSystem. The collision box is still a single 1x2x1 AABB (see the PLAYER_HALF_*
    // constants in the physics systems); y = 1.0 so it rests with its bottom on the ground
    auto player = registry.create();
    registry.emplace<Transform3D>(player, Vector3{ 0.0f, 1.0f, 0.0f });
    registry.emplace<Velocity>(player);
    registry.emplace<PlayerControlled>(player);
    registry.emplace<CameraTarget>(player);

    // ground, split into segments with real gaps (chasms) instead of one solid strip -
    // jumpSpeed = 14, moveSpeed = 8 and gravity = -30 give a level jump a reach of
    // ~7.5 units (v*(2v/g)), so every chasm below (6 units wide) stays safely within that.
    CreatePlatform(registry, Vector3{ -145.0f, 0.25f, 0.0f }, Vector3{ 210.0f, 0.5f, 2.0f }, Color{ 90, 70, 40, 255 }); // far left
    CreatePlatform(registry, Vector3{ -22.0f, 0.25f, 0.0f }, Vector3{ 24.0f, 0.5f, 2.0f }, Color{ 90, 70, 40, 255 });  // between the left chasm and spawn

    // a plain, low wall: no special mechanic needed, just clear it with a normal jump
    // (max jump height ~3.27 comfortably clears this 2.5-tall one)
    CreatePlatform(registry, Vector3{ -60.0f, 1.25f, 0.0f }, Vector3{ 2.0f, 2.5f, 2.0f }, Color{ 100, 95, 90, 255 });

    // a taller wall: too tall to jump over outright (5.0 > the ~3.27 max jump height), but
    // its single grab point is its own top (5.0) - jump towards it, the player automatically
    // grabs the edge if the jump reaches that high, then press up to pull up; otherwise they
    // fall back to the ground
    CreateGrabWall(registry, Vector3{ -90.0f, 2.5f, 0.0f }, Vector3{ 2.0f, 5.0f, 2.0f }, Color{ 100, 95, 90, 255 }, { 5.0f });
    // CHASM: x = [-40, -34]
    CreatePlatform(registry, Vector3{ 5.0f, 0.25f, 0.0f }, Vector3{ 30.0f, 0.5f, 2.0f }, Color{ 90, 70, 40, 255 });    // spawn area
    // CHASM: x = [20, 26]
    CreatePlatform(registry, Vector3{ 37.0f, 0.25f, 0.0f }, Vector3{ 22.0f, 0.5f, 2.0f }, Color{ 90, 70, 40, 255 });   // leads up to the first wall (ends at x = 48)
    CreatePlatform(registry, Vector3{ 176.0f, 0.25f, 0.0f }, Vector3{ 160.0f, 0.5f, 2.0f }, Color{ 90, 70, 40, 255 }); // resumes past the second wall (starts at x = 96)

    // small ground patches directly under each handhold wall's own footprint: the big void
    // under the ledges/chasm bridge is meant to be dangerous, but a full controlled descent
    // via a wall's own handholds should always reach real ground, not fall through into it
    CreatePlatform(registry, Vector3{ 50.0f, 0.25f, 0.0f }, Vector3{ 4.0f, 0.5f, 2.0f }, Color{ 90, 70, 40, 255 }); // under the first wall (x = 48..52)
    CreatePlatform(registry, Vector3{ 94.0f, 0.25f, 0.0f }, Vector3{ 4.0f, 0.5f, 2.0f }, Color{ 90, 70, 40, 255 }); // under the second wall (x = 92..96)

    // first handhold wall (x = 48..52, y = 0..10): too tall for one jump, so it lists three
    // grab points (4.0, 7.5, 10.0) each within a jump's reach of the one before - jump to
    // catch the lowest, jump again from there to the next, and so on up to the ledge at the
    // top. Every boundary below is exactly contiguous with its neighbour (no overlapping
    // colliders/geometry), since an overlap here previously caused z-fighting and collision
    // jitter. The collider's bottom is raised to y = 0.5 (height 9.5, not 10, same top) so
    // it does not also overlap the ground patch beneath it in y - two solid colliders
    // occupying the same space fought each other every frame, which could pin the player or
    // cancel a jump away from the wall.
    CreateGrabWall(registry, Vector3{ 50.0f, 5.25f, 0.0f }, Vector3{ 4.0f, 9.5f, 2.0f }, Color{ 80, 65, 55, 255 },
                    { 4.0f, 7.5f, 10.0f });
    CreatePlatform(registry, Vector3{ 60.0f, 9.5f, 0.0f }, Vector3{ 16.0f, 1.0f, 2.0f }, Color{ 110, 90, 60, 255 }); // ledge 1, x = [52, 68] (top = 10)

    // a chasm at height: x = [68, 74], with open air all the way down to the ground below -
    // missing this jump means a long fall, caught by MovementSystem's void-fall respawn
    CreatePlatform(registry, Vector3{ 83.0f, 9.5f, 0.0f }, Vector3{ 18.0f, 1.0f, 2.0f }, Color{ 110, 90, 60, 255 }); // ledge 2, x = [74, 92] (top = 10)

    // second handhold wall (x = 92..96, y = 0..10): same three grab points, so the ledge is
    // also reachable by climbing up from the resumed ground on its far side, not only from
    // ledge 2 - jumping down off the ledge needs no assistance, gravity handles that on its own
    // (collider bottom raised to y = 0.5 for the same reason as the first wall)
    CreateGrabWall(registry, Vector3{ 94.0f, 5.25f, 0.0f }, Vector3{ 4.0f, 9.5f, 2.0f }, Color{ 80, 65, 55, 255 },
                    { 4.0f, 7.5f, 10.0f });

    // a walkable hill further along the resumed ground: climbs and descends by simply
    // walking into the slope, no jump needed, same ground-surface height convention as the
    // platforms above (0.5 matches the flat ground's own top)
    CreateSlope(registry, Vector2{ 130.0f, 0.5f }, Vector2{ 145.0f, 5.5f }, Color{ 90, 70, 40, 255 });
    CreateSlope(registry, Vector2{ 145.0f, 5.5f }, Vector2{ 160.0f, 0.5f }, Color{ 90, 70, 40, 255 });

    // mid-ground jungle silhouette backdrop, behind the gameplay plane
    CreateParallaxLayer(registry, -20.0f, Vector3{ 600.0f, 20.0f, 4.0f }, Color{ 40, 110, 60, 255 });

    // far background backdrop, deepest layer for the strongest colour/contrast falloff
    CreateParallaxLayer(registry, -45.0f, Vector3{ 700.0f, 35.0f, 4.0f }, Color{ 25, 70, 90, 255 });

    // gameplay-plane landmarks: kept just behind the player's exact depth (not z = 0) so
    // they never visually interpenetrate the player box when their x lines up with it -
    // still close enough to move across the screen almost 1:1 with world-space motion
    CreateColumnRow(registry, -3.0f, 15.0f, 23, Vector3{ 0.5f, 3.0f, 0.5f }, Color{ 230, 180, 40, 255 }, kNoGroundRanges);

    // mid-ground trees, slightly in front of the mid-ground backdrop: shift more slowly across
    // the screen than the landmarks above as the camera pans, making the parallax visible
    CreateColumnRow(registry, -18.0f, 25.0f, 15, Vector3{ 2.0f, 9.0f, 2.0f }, Color{ 30, 90, 50, 255 });

    // far trees, slightly in front of the far backdrop: barely move at all, giving the
    // strongest sense of depth
    CreateColumnRow(registry, -40.0f, 45.0f, 9, Vector3{ 3.0f, 14.0f, 3.0f }, Color{ 35, 65, 80, 255 });

    // foreground plane: sparse vegetation/architecture placeholders between the camera and the
    // player. Kept almost flat (minimal z thickness, like a cut-out) and close to the gameplay
    // plane rather than deep in front of it, so it reads as a thin dressing layer instead of a
    // solid volume - each column's world x is fixed, so walking past one makes it pass in
    // front of, and briefly hide, the player, the same foreground-dressing trick used in
    // Flashback and Trine.
    CreateColumnRow(registry, 2.0f, 18.0f, 19, Vector3{ 1.5f, 6.0f, 0.3f }, Color{ 20, 50, 28, 255 }, kNoGroundRanges);
}
