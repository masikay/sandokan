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
#include "render/ModelCache.h"

Application::Application(int width, int height, const char* title, const LaunchOptions& options)
    : m_options(options)
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

    // first run (no level file yet) writes the built-in test level out, so there is always a
    // file to load, and later to edit
    const std::string levelPath = std::string(SANDOKAN_LEVEL_DIR) + "/level01.json";
    if (!LoadLevel(m_level, levelPath))
    {
        m_level = BuildDefaultLevel();
        MakeDirectory(SANDOKAN_LEVEL_DIR);
        SaveLevel(m_level, levelPath);
    }

    SetupWorld(m_registry, m_level);

    m_editor = std::make_unique<Editor>(m_level, m_registry, levelPath);

    if (m_options.startInEditor)
    {
        SetEditing(true);
        m_editor->Select(m_options.selectObject);
    }
}

Application::~Application()
{
    UnloadAllModels();
    m_editor.reset(); // shuts ImGui down, which must happen while the window still exists
    CloseWindow();
}

void Application::Run()
{
    int frame = 0;
    while (!WindowShouldClose())
    {
        Update(GetFrameTime());
        Render();

        if (!m_options.screenshotPath.empty() && ++frame == 30)
        {
            TakeScreenshot(m_options.screenshotPath.c_str());
            break;
        }
    }
}

void Application::SetEditing(bool editing)
{
    m_editing = editing;

    if (editing)
    {
        m_editor->Enter(m_camera);
        return;
    }

    // back to the game: drop the player where the editor camera was looking, so an edit can
    // be tried out on the spot, and clear any climbing state left over from before
    const Camera3D& editorCamera = m_editor->GetCamera();
    auto view = m_registry.view<Transform3D, Velocity, PlayerControlled>();
    for (auto entity : view)
    {
        view.get<Transform3D>(entity).position = { editorCamera.target.x, editorCamera.target.y + 2.0f, 0.0f };
        view.get<Velocity>(entity).value = { 0.0f, 0.0f, 0.0f };
        m_registry.remove<LedgeHang>(entity);
        m_registry.remove<WallSettle>(entity);
        m_registry.remove<RecentRelease>(entity);
    }
    m_camera.target = { editorCamera.target.x, editorCamera.target.y, 0.0f };
    m_camera.position = { editorCamera.target.x, editorCamera.target.y + 0.5f, m_camera.position.z };
}

void Application::Update(float deltaTime)
{
    if (IsKeyPressed(KEY_F1)) SetEditing(!m_editing);

    if (m_editing)
    {
        m_editor->Update(deltaTime);
        return;
    }

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

    BeginMode3D(m_editing ? m_editor->GetCamera() : m_camera);
    RenderSystem(m_registry);
    if (m_editing) m_editor->DrawWorldOverlay();
    EndMode3D();

    DrawFPS(10, 10);

    // debug: the camera follows the player 1:1 on x, so the player itself never visibly
    // moves on screen; print its world position to confirm movement is actually happening
    auto view = m_registry.view<Transform3D, CameraTarget>();
    for (auto entity : view)
    {
        auto& transform = view.get<Transform3D>(entity);
        if (!m_editing) DrawText(TextFormat("player x: %.1f", transform.position.x), 10, 30, 20, DARKGRAY);
    }

    if (m_editing) m_editor->DrawUI();
    else DrawText("F1: level editor", 10, 55, 20, DARKGRAY);

    EndDrawing();
}
