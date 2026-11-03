cmake_minimum_required(VERSION 3.20)
project(sandokan_game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)

FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.2
)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.7
)

FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG Raylib_5_5
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib entt json imgui rlimgui)

# Dear ImGui and rlImGui ship no CMake targets, so build them as a small static library
add_library(imgui_raylib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(imgui_raylib PUBLIC ${imgui_SOURCE_DIR} ${rlimgui_SOURCE_DIR})
target_link_libraries(imgui_raylib PUBLIC raylib)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/core/Application.cpp
    src/ecs/systems/InputSystem.cpp
    src/ecs/systems/MovementSystem.cpp
    src/ecs/systems/CollisionSystem.cpp
    src/ecs/systems/SlopeSystem.cpp
    src/ecs/systems/LedgeGrabSystem.cpp
    src/ecs/systems/CameraSystem.cpp
    src/ecs/systems/RenderSystem.cpp
    src/world/WorldSetup.cpp
    src/world/Level.cpp
    src/editor/Editor.cpp
    src/render/ModelCache.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SANDOKAN_LEVEL_DIR="${CMAKE_SOURCE_DIR}/levels"
    SANDOKAN_MODEL_DIR="${CMAKE_SOURCE_DIR}/assets/glb")
target_link_libraries(${PROJECT_NAME} PRIVATE raylib EnTT::EnTT nlohmann_json::nlohmann_json imgui_raylib)
