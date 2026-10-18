#include "MovementSystem.h"

#include "../Components.h"

namespace
{
    constexpr float GRAVITY = -30.0f;
    // falling below this height means the player missed a jump into a chasm; caught here
    // instead of letting them fall forever past the bottom of the level
    constexpr float VOID_FALL_Y = -15.0f;
}

void MovementSystem(entt::registry& registry, float deltaTime)
{
    auto view = registry.view<Transform3D, Velocity>();
    for (auto entity : view)
    {
        auto& transform = view.get<Transform3D>(entity);
        auto& velocity = view.get<Velocity>(entity);

        // only entities that can jump (currently the player) are affected by gravity;
        // resting on the ground/platforms is resolved afterwards by CollisionSystem.
        // Hanging from a grab point suspends gravity too.
        if (registry.all_of<PlayerControlled>(entity) && !registry.all_of<LedgeHang>(entity))
        {
            velocity.value.y += GRAVITY * deltaTime;
        }

        transform.position.x += velocity.value.x * deltaTime;
        transform.position.y += velocity.value.y * deltaTime;

        if (auto* control = registry.try_get<PlayerControlled>(entity); control && transform.position.y < VOID_FALL_Y)
        {
            transform.position = control->respawnPosition;
            velocity.value = { 0.0f, 0.0f, 0.0f };
        }
    }
}
