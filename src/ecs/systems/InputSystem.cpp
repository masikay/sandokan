#include "InputSystem.h"

#include <raylib.h>

#include "../Components.h"

void InputSystem(entt::registry& registry)
{
    auto view = registry.view<PlayerControlled, Velocity>();
    for (auto entity : view)
    {
        auto& control = view.get<PlayerControlled>(entity);
        auto& velocity = view.get<Velocity>(entity);

        // LedgeGrabSystem (which runs before InputSystem) owns all input while hanging from
        // a grab point - jumping onward, pulling up, letting go, or just staying put - and
        // while sliding onto solid footing right after mantling (WallSettle)
        if (registry.all_of<LedgeHang>(entity) || registry.all_of<WallSettle>(entity)) continue;

        velocity.value.x = 0.0f;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) velocity.value.x += control.moveSpeed;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) velocity.value.x -= control.moveSpeed;

        // simple jump: only trigger while grounded, gravity handled by MovementSystem
        if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP)) && velocity.value.y == 0.0f)
        {
            velocity.value.y = control.jumpSpeed;
        }
    }
}
