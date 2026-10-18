#pragma once

#include <entt/entt.hpp>
#include <raylib.h>

class Application
{
public:
    Application(int width, int height, const char* title);
    ~Application();

    void Run();

private:
    void Update(float deltaTime);
    void Render();

    entt::registry m_registry;
    Camera3D m_camera{};
};
