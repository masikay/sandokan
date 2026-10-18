#include "CameraSystem.h"

#include <cmath>

#include "../Components.h"

namespace
{
    // a dead zone instead of a rigid 1:1 follow: the player can move
	// this far from the current camera focus (world units, each side)
    // before the camera starts tracking again
    constexpr float HORIZONTAL_BUFFER_HALF_WIDTH = 2.0f;
    // how quickly the vertical focus eases towards the player once grounded at a new
    // height; higher is snappier, lower is softer. A rate, not a duration, so it stays
    // frame-rate independent
    constexpr float VERTICAL_FOLLOW_SPEED = 3.0f;

    // moves `focus` towards `target` only once `target` exits the [-halfBuffer, halfBuffer]
    // window around it; otherwise the focus (and therefore the camera) stays put
    float UpdateDeadZoneFocus(float focus, float target, float halfBuffer)
    {
        float offset = target - focus;
        if (offset > halfBuffer) return target - halfBuffer;
        if (offset < -halfBuffer) return target + halfBuffer;
        return focus;
    }
}

void CameraSystem(entt::registry& registry, Camera3D& camera, float deltaTime)
{
    auto view = registry.view<Transform3D, Velocity, CameraTarget>();
    for (auto entity : view)
    {
        auto& transform = view.get<Transform3D>(entity);
        auto& velocity = view.get<Velocity>(entity);

        float newFocusX = UpdateDeadZoneFocus(camera.target.x, transform.position.x, HORIZONTAL_BUFFER_HALF_WIDTH);

        // CollisionSystem (and LedgeGrabSystem, while hanging from a grab point) zero
        // vertical velocity exactly when the player is resting/holding still; use that,
        // plus any falling velocity, as "track the camera". Only while actively rising
        // (positive velocity.y, mid-jump) is the vertical focus frozen, so a plain hop that
        // comes back down to the same ground barely moves the camera on the way up - but a
        // fall (off a ledge, a missed jump into a chasm, coming back down from a climb) is
        // followed immediately instead of waiting for it to land.
        float newFocusY = camera.target.y;
        if (velocity.value.y <= 0.0f)
        {
            float t = 1.0f - std::exp(-VERTICAL_FOLLOW_SPEED * deltaTime);
            newFocusY = camera.target.y + (transform.position.y - camera.target.y) * t;
        }

        // shift position by the same delta as target, preserving the camera's fixed
        // tilt/offset (e.g. being above and behind the gameplay plane) on both axes
        camera.position.x += newFocusX - camera.target.x;
        camera.target.x = newFocusX;

        camera.position.y += newFocusY - camera.target.y;
        camera.target.y = newFocusY;
    }
}
