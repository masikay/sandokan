#pragma once

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

// World-space position of the entity; the z component is used as depth for parallax
// named Transform3D (not Transform) to avoid colliding with raylib's own Transform struct
struct Transform3D {
    Vector3 position{ 0.0f, 0.0f, 0.0f };
};

// Linear velocity integrated into the transform each frame
struct Velocity {
    Vector3 value{ 0.0f, 0.0f, 0.0f };
};

// Tags the entity controlled by player input
struct PlayerControlled {
    float moveSpeed = 8.0f;
    float jumpSpeed = 14.0f;
    // where the player is reset to if they fall into a chasm, past the bottom of the level
    Vector3 respawnPosition{ 0.0f, 1.5f, 0.0f };
};

// Axis-aligned collider used for platform collision; only x/y are resolved, since gameplay
// is constrained to a single depth lane and z overlap is not meaningful here
struct Collider {
    Vector3 halfExtents{ 0.5f, 0.5f, 0.5f };
};

// Placeholder visual: a colored box, used until real models/sprites are available
struct BoxRenderable {
    Vector3 size{ 1.0f, 1.0f, 1.0f };
    Color color = WHITE;
};

// A GLB model drawn instead of the entity's box when it can be loaded (the box then only
// acts as collider). Drawn centered on the box, uniformly scaled and rotated around X, Y, Z.
struct ModelRenderable {
    std::string name;
    Vector3 rotation{ 0.0f, 0.0f, 0.0f }; // degrees
    float scale = 1.0f;
};

// A straight walkable ramp between two ground-surface points (not player stand height,
// same convention as a platform's top face). SlopeSystem snaps the player's y to the
// terrain height at their current x, letting them walk up/down it instead of jumping.
// depth/color are only used for the placeholder visual, drawn as a flat sloped panel.
struct Slope {
    Vector2 start{ 0.0f, 0.0f };
    Vector2 end{ 0.0f, 0.0f };
    float depth = 2.0f;
    Color color = BROWN;
};

// Marks the entity the camera should follow horizontally
struct CameraTarget {};

// Heights (world y), ascending, of the grabbable points on a Collider wall. A plain wall
// has no such component at all, so it is just a solid obstacle: short enough, it is simply
// jumped over, with no special interaction. A wall with this component is scaled only by
// jumping - never by holding a direction - catching the lowest listed height within reach:
// a short/medium wall lists just its own top (one jump reaches it, or the player falls back
// down); a tall "handhold" wall lists several heights spaced within a jump's reach of each
// other, so the player jumps from one to the next, mantling once they catch the last one
// (which must equal the wall's actual top surface).
struct GrabPoints {
    std::vector<float> heights;
};

// Set by LedgeGrabSystem while the player is hanging from one of a wall's GrabPoints.
// canMantle is true only when this is the topmost point and it matches the wall's actual
// top surface, allowing the player to pull all the way up onto it; otherwise (an
// intermediate handhold) the only way onward is to jump again towards the next one.
struct LedgeHang {
    float grabbedHeight = 0.0f;
    bool canMantle = false;
    // the grabbed wall itself, used while scanning its other GrabPoints
    entt::entity wall = entt::null;
    // true once mantling has actually started (up pressed while on the topmost point); from
    // then on it runs to completion on its own even if up is released early
    bool mantling = false;
};

// Set by LedgeGrabSystem when jumping away from an intermediate grab point, so that same
// point is not instantly re-caught (its own height always still reads as "reachable" the
// instant it is released). Cleared once the player either catches a higher point or lands.
struct RecentRelease {
    float height = 0.0f;
};

// Set by LedgeGrabSystem right after mantling, once standing height is reached: slides the
// player the rest of the way onto solid footing on the wall's own near edge. Deliberately
// separate from LedgeHang, so the arms lower immediately on mantling instead of waiting for
// this last small, purely cosmetic adjustment to finish.
struct WallSettle {
    float targetX = 0.0f;
};

// Tags every entity spawned from the authored level data, so the level can be cleared and
// re-spawned (e.g. by the editor) without touching the player
struct LevelEntity {};
