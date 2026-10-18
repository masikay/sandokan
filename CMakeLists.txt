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

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib entt)

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
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib EnTT::EnTT)
