#include "Application.h"

#include "../ecs/Components.h"
#include "../ecs/systems/CameraSystem.h"
#include "../ecs/systems/CollisionSystem.h"
#include "../ecs/systems/InputSystem.h"
#include "../ecs/systems/LedgeGrabSystem.h"
#include "../ecs/systems/MovementSystem.h"
#include "../ecs/systems/RenderSystem.h"
#include "../ecs/systems/SlopeSystem.h"
#include "../world/WorldSetup.h"

Application::Application(int width, int height, const char* title)
{
    InitWindow(width, height, title);
    SetTargetFPS(60);

    // perspective camera placed behind the gameplay plane; background layers at
    // different depths will move at different speeds on screen as the camera pans,
    // producing the parallax effect without manually scaling per-layer scroll speed.
    // Kept close to the gameplay plane (z = 0) so the player reads as close to the screen,
    // and only slightly elevated above the target so the view stays mostly side-on instead
    // of looking down at the player (a steeper tilt visually foreshortens the character,
    // making the framing read as centered on the head rather than the body).
    m_camera.position = { 0.0f, 1.5f, 9.0f };
    m_camera.target = { 0.0f, 1.0f, 0.0f };
    m_camera.up = { 0.0f, 1.0f, 0.0f };
    // wide FoV so the player doesn't read as too zoomed-in/close, without having to move
    // the camera back (which would also change its distance to every other depth layer)
    m_camera.fovy = 60.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;

    SetupWorld(m_registry);
}

Application::~Application()
{
    CloseWindow();
}

void Application::Run()
{
    while (!WindowShouldClose())
    {
        Update(GetFrameTime());
        Render();
    }
}

void Application::Update(float deltaTime)
{
    LedgeGrabSystem(m_registry, deltaTime);
    InputSystem(m_registry);
    MovementSystem(m_registry, deltaTime);
    CollisionSystem(m_registry);
    SlopeSystem(m_registry);
    CameraSystem(m_registry, m_camera, deltaTime);
}

void Application::Render()
{
    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(m_camera);
    RenderSystem(m_registry);
    EndMode3D();

    DrawFPS(10, 10);

    // debug: the camera follows the player 1:1 on x, so the player itself never visibly
    // moves on screen; print its world position to confirm movement is actually happening
    auto view = m_registry.view<Transform3D, CameraTarget>();
    for (auto entity : view)
    {
        auto& transform = view.get<Transform3D>(entity);
        DrawText(TextFormat("player x: %.1f", transform.position.x), 10, 30, 20, DARKGRAY);
    }

    EndDrawing();
}
