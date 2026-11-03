#pragma once

#include <memory>
#include <string>

#include <entt/entt.hpp>
#include <raylib.h>

#include "editor/Editor.h"
#include "world/Level.h"

struct LaunchOptions {
    bool startInEditor = false;
    int selectObject = -1;
    std::string screenshotPath; // when set: screenshot after a few frames, then exit
};

class Application
{
public:
    Application(int width, int height, const char* title, const LaunchOptions& options = {});
    ~Application();

    void Run();

private:
    void Update(float deltaTime);
    void Render();

    entt::registry m_registry;
    Level m_level;
    void SetEditing(bool editing);

    Camera3D m_camera{};
    std::unique_ptr<Editor> m_editor;
    bool m_editing = false;
    LaunchOptions m_options;
};
