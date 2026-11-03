#include "editor/Editor.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <rlImGui.h>
#include <rlgl.h>

#include "render/ModelCache.h"
#include "render/ModelTransform.h"


namespace
{
    constexpr float PAN_SPEED_PER_UNIT_DISTANCE = 0.6f; // pan faster the further out we are
    constexpr float MIN_DISTANCE = 8.0f;
    constexpr float MAX_DISTANCE = 150.0f;

    const char* KIND_NAMES[] = { "Decor", "Platform", "Grab wall", "Slope" };

    bool IsCtrlDown()
    {
        return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    }

    constexpr float RING_PICK_PIXELS = 12.0f;
    constexpr float SCALE_HANDLE_PICK_PIXELS = 16.0f;
    constexpr float ROTATION_SNAP_DEGREES = 15.0f;
    constexpr int RING_SEGMENTS = 64;

    // each ring lies in the plane perpendicular to its axis; u x b = axis (right-handed), so
    // a positive angle along the ring is a positive rotation about that axis
    Vector3 RingPoint(int axis, Vector3 center, float radius, float theta)
    {
        static const Vector3 U[3] = { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 0 } };
        static const Vector3 B[3] = { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } };
        float c = std::cos(theta) * radius;
        float s = std::sin(theta) * radius;
        return { center.x + U[axis].x * c + B[axis].x * s, center.y + U[axis].y * c + B[axis].y * s,
                 center.z + U[axis].z * c + B[axis].z * s };
    }

    float& RotationComponent(Vector3& rotation, int axis)
    {
        return axis == 0 ? rotation.x : (axis == 1 ? rotation.y : rotation.z);
    }

    float PointSegmentDistance(Vector2 p, Vector2 a, Vector2 b, float& outT)
    {
        Vector2 ab = { b.x - a.x, b.y - a.y };
        float len2 = ab.x * ab.x + ab.y * ab.y;
        float t = len2 > 1e-6f ? std::clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2, 0.0f, 1.0f) : 0.0f;
        outT = t;
        Vector2 closest = { a.x + ab.x * t, a.y + ab.y * t };
        return std::hypot(p.x - closest.x, p.y - closest.y);
    }

    // screen-space distance from the mouse to a ring, and the ring angle of the closest point
    float DistanceToRing(const Camera3D& camera, int axis, Vector3 center, float radius, Vector2 mouse, float& outTheta)
    {
        float best = 1e30f;
        Vector2 previous = GetWorldToScreen(RingPoint(axis, center, radius, 0.0f), camera);
        for (int i = 1; i <= RING_SEGMENTS; ++i)
        {
            float theta = static_cast<float>(i) / RING_SEGMENTS * 2.0f * PI;
            Vector2 current = GetWorldToScreen(RingPoint(axis, center, radius, theta), camera);
            float t;
            float distance = PointSegmentDistance(mouse, previous, current, t);
            if (distance < best)
            {
                best = distance;
                float thetaPrev = static_cast<float>(i - 1) / RING_SEGMENTS * 2.0f * PI;
                outTheta = thetaPrev + (theta - thetaPrev) * t;
            }
            previous = current;
        }
        return best;
    }
}

Editor::Editor(Level& level, entt::registry& registry, std::string levelPath)
    : m_level(level), m_registry(registry), m_levelPath(std::move(levelPath))
{
    rlImGuiSetup(true);
    ImGui::GetIO().IniFilename = nullptr; // do not drop an imgui.ini into whatever the working directory is
    m_camera.up = { 0.0f, 1.0f, 0.0f };
    m_camera.fovy = 60.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;
}

Editor::~Editor()
{
    rlImGuiShutdown();
}

void Editor::Enter(const Camera3D& gameCamera)
{
    m_camera.target = { gameCamera.target.x, gameCamera.target.y, 0.0f };
    m_camera.position = { gameCamera.target.x, gameCamera.target.y, m_cameraDistance };
    m_dragging = false;
}

void Editor::Select(int index)
{
    if (index < 0 || index >= static_cast<int>(m_level.size()))
    {
        m_selected = -1;
        return;
    }
    m_selected = index;
    m_camera.target.x = m_level[index].position.x;
    m_camera.target.y = m_level[index].position.y;
    m_cameraDistance = std::clamp(GizmoRadius(m_level[index]) * 5.0f, MIN_DISTANCE, MAX_DISTANCE);
    m_camera.position = { m_camera.target.x, m_camera.target.y, m_cameraDistance };
}

float Editor::Snap(float value) const
{
    return m_snap ? std::round(value / m_snapStep) * m_snapStep : value;
}

void Editor::Rebuild()
{
    ClearLevelEntities(m_registry);
    SpawnLevel(m_registry, m_level);
}

void Editor::Save()
{
    if (SaveLevel(m_level, m_levelPath))
    {
        m_dirty = false;
        m_status = "Saved " + m_levelPath;
    }
    else
    {
        m_status = "SAVE FAILED: " + m_levelPath;
    }
}

void Editor::Reload()
{
    if (LoadLevel(m_level, m_levelPath))
    {
        m_selected = -1;
        m_dirty = false;
        m_undoStack.clear();
        m_redoStack.clear();
        Rebuild();
        m_status = "Reloaded from disk";
    }
    else
    {
        m_status = "RELOAD FAILED: " + m_levelPath;
    }
}

void Editor::PushUndo(const Level& state)
{
    constexpr std::size_t MAX_UNDO = 100;
    m_undoStack.push_back(state);
    if (m_undoStack.size() > MAX_UNDO) m_undoStack.erase(m_undoStack.begin());
    m_redoStack.clear();
}

void Editor::Undo()
{
    if (m_undoStack.empty()) return;
    m_redoStack.push_back(m_level);
    m_level = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    if (m_selected >= static_cast<int>(m_level.size())) m_selected = -1;
    m_dragging = false;
    m_dirty = true;
    Rebuild();
}

void Editor::Redo()
{
    if (m_redoStack.empty()) return;
    m_undoStack.push_back(m_level);
    m_level = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    if (m_selected >= static_cast<int>(m_level.size())) m_selected = -1;
    m_dragging = false;
    m_dirty = true;
    Rebuild();
}

bool Editor::FitSizeToModel(LevelObject& object) const
{
    const Model* model = GetModel(object.model);
    if (model == nullptr) return false;

    BoundingBox bounds = TransformedModelBounds(*model, object.rotation, object.scale);
    object.size = { bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z };
    return true;
}

void Editor::PlaceModel(const std::string& name)
{
    PushUndo(m_level);

    LevelObject object;
    object.kind = ObjectKind::Decor;
    object.model = name;
    object.color = WHITE;
    if (!FitSizeToModel(object)) object.size = { 2.0f, 2.0f, 2.0f };

    // bottom of the object at the camera's focus point, so it lands on the ground you look at
    object.position = { Snap(m_camera.target.x), Snap(m_camera.target.y) + object.size.y * 0.5f, 0.0f };

    m_level.push_back(std::move(object));
    m_selected = static_cast<int>(m_level.size()) - 1;
    m_dirty = true;
    Rebuild();
}

void Editor::AddObject(ObjectKind kind)
{
    PushUndo(m_level);
    LevelObject object;
    object.kind = kind;
    float x = Snap(m_camera.target.x);
    float y = Snap(m_camera.target.y);

    switch (kind)
    {
        case ObjectKind::Decor:
            object.size = { 3.0f, 3.0f, 3.0f };
            object.position = { x, y, -3.0f };
            object.color = { 30, 90, 50, 255 };
            break;
        case ObjectKind::Platform:
            object.size = { 4.0f, 1.0f, 2.0f };
            object.position = { x, y, 0.0f };
            object.color = { 110, 90, 60, 255 };
            break;
        case ObjectKind::GrabWall:
            object.size = { 4.0f, 10.0f, 2.0f };
            object.position = { x, y, 0.0f };
            object.color = { 80, 65, 55, 255 };
            object.grabHeights = { y + 4.0f, y + 7.5f, y + 10.0f };
            break;
        case ObjectKind::Slope:
            object.size = { 10.0f, 5.0f, 2.0f };
            object.position = { x, y, 0.0f };
            object.color = { 90, 70, 40, 255 };
            object.slopeStart = { x - 5.0f, y };
            object.slopeEnd = { x + 5.0f, y + 5.0f };
            break;
    }

    m_level.push_back(std::move(object));
    m_selected = static_cast<int>(m_level.size()) - 1;
    m_dirty = true;
    Rebuild();
}

void Editor::DuplicateSelected()
{
    if (m_selected < 0) return;
    PushUndo(m_level);
    LevelObject copy = m_level[m_selected];
    copy.position.x += 2.0f;
    if (copy.kind == ObjectKind::Slope)
    {
        copy.slopeStart.x += 2.0f;
        copy.slopeEnd.x += 2.0f;
    }
    m_level.push_back(std::move(copy));
    m_selected = static_cast<int>(m_level.size()) - 1;
    m_dirty = true;
    Rebuild();
}

void Editor::DeleteSelected()
{
    if (m_selected < 0) return;
    PushUndo(m_level);
    m_level.erase(m_level.begin() + m_selected);
    m_selected = -1;
    m_dirty = true;
    Rebuild();
}

BoundingBox Editor::ObjectBounds(const LevelObject& o) const
{
    if (o.kind == ObjectKind::Slope)
    {
        float halfDepth = o.size.z * 0.5f;
        // a little vertical thickness, so flat ramps can still be picked
        return { { std::min(o.slopeStart.x, o.slopeEnd.x), std::min(o.slopeStart.y, o.slopeEnd.y) - 0.25f, -halfDepth },
                 { std::max(o.slopeStart.x, o.slopeEnd.x), std::max(o.slopeStart.y, o.slopeEnd.y) + 0.25f, halfDepth } };
    }
    Vector3 half = { o.size.x * 0.5f, o.size.y * 0.5f, o.size.z * 0.5f };
    return { { o.position.x - half.x, o.position.y - half.y, o.position.z - half.z },
             { o.position.x + half.x, o.position.y + half.y, o.position.z + half.z } };
}

bool Editor::RayPlaneHit(const Ray& ray, float planeZ, Vector3& outHit) const
{
    if (std::abs(ray.direction.z) < 1e-6f) return false;
    float t = (planeZ - ray.position.z) / ray.direction.z;
    if (t < 0.0f) return false;
    outHit = { ray.position.x + ray.direction.x * t, ray.position.y + ray.direction.y * t, planeZ };
    return true;
}

void Editor::UpdateCamera(float deltaTime)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse)
    {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
        {
            m_cameraDistance = std::clamp(m_cameraDistance * (1.0f - wheel * 0.1f), MIN_DISTANCE, MAX_DISTANCE);
        }
    }

    float speed = m_cameraDistance * PAN_SPEED_PER_UNIT_DISTANCE;
    if (!io.WantCaptureKeyboard && !IsCtrlDown())
    {
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) m_camera.target.x -= speed * deltaTime;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) m_camera.target.x += speed * deltaTime;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) m_camera.target.y += speed * deltaTime;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) m_camera.target.y -= speed * deltaTime;
    }

    // middle-mouse drag pans too, grabbing the world under the cursor
    if (!io.WantCaptureMouse && IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
    {
        Vector2 delta = GetMouseDelta();
        float worldPerPixel = 2.0f * m_cameraDistance * std::tan(m_camera.fovy * DEG2RAD * 0.5f) / GetScreenHeight();
        m_camera.target.x -= delta.x * worldPerPixel;
        m_camera.target.y += delta.y * worldPerPixel;
    }

    m_camera.position = { m_camera.target.x, m_camera.target.y, m_cameraDistance };
}

// The scale handle sits on the diagonal of the camera-facing plane, just outside the rings
Vector3 Editor::ScaleHandlePosition(const LevelObject& object) const
{
    float d = GizmoRadius(object) * 0.9f;
    return { object.position.x + d, object.position.y + d, object.position.z };
}

float Editor::GizmoRadius(const LevelObject& object) const
{
    float extent = std::max({ object.size.x, object.size.y, object.size.z });
    return std::max(1.5f, extent * 0.6f);
}

bool Editor::UpdateGizmo()
{
    bool hasTarget = m_selected >= 0 && m_selected < static_cast<int>(m_level.size()) && !m_level[m_selected].model.empty();
    if (!hasTarget)
    {
        m_hoverAxis = -1;
        m_gizmoAxis = -1;
        m_scaleHover = false;
        m_scaleDragging = false;
        return false;
    }

    LevelObject& object = m_level[m_selected];
    Vector3 center = object.position;
    float radius = GizmoRadius(object);

    if (m_scaleDragging)
    {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            m_scaleDragging = false;
            return true;
        }

        // scale follows how far the mouse is from the object center, relative to where it was
        // when grabbed; measured on the camera-facing plane through the center
        Vector3 hit;
        if (!RayPlaneHit(GetScreenToWorldRay(GetMousePosition(), m_camera), center.z, hit)) return true;
        float distance = std::hypot(hit.x - center.x, hit.y - center.y);
        float scale = m_scaleStart * distance / std::max(m_scaleStartDistance, 0.01f);
        scale = std::clamp(m_snap ? std::round(scale / 0.05f) * 0.05f : scale, 0.05f, 50.0f);
        if (scale == object.scale) return true;

        if (!m_scaleMoved)
        {
            PushUndo(m_level);
            m_scaleMoved = true;
        }
        object.scale = scale;
        FitSizeToModel(object);
        m_dirty = true;
        Rebuild();
        return true;
    }

    if (m_gizmoAxis >= 0)
    {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            m_gizmoAxis = -1;
            return true;
        }

        // how many screen pixels the grabbed point moves per radian of rotation; dragging the
        // mouse along that screen direction turns the ring. Working in screen space keeps
        // edge-on rings (which the front-facing camera sees as lines) draggable, where
        // intersecting the mouse ray with the ring's plane would be degenerate.
        Vector2 here = GetWorldToScreen(RingPoint(m_gizmoAxis, center, radius, m_gizmoTheta), m_camera);
        Vector2 ahead = GetWorldToScreen(RingPoint(m_gizmoAxis, center, radius, m_gizmoTheta + 0.05f), m_camera);
        Vector2 perRadian = { (ahead.x - here.x) / 0.05f, (ahead.y - here.y) / 0.05f };
        float length2 = perRadian.x * perRadian.x + perRadian.y * perRadian.y;
        if (length2 < 1.0f) return true;

        Vector2 mouseDelta = GetMouseDelta();
        float delta = std::clamp((mouseDelta.x * perRadian.x + mouseDelta.y * perRadian.y) / length2, -0.5f, 0.5f);
        if (delta == 0.0f) return true;

        if (!m_gizmoMoved)
        {
            PushUndo(m_level);
            m_gizmoMoved = true;
        }

        m_gizmoTheta += delta;
        m_gizmoAccum += delta * RAD2DEG;
        float applied = m_rotationSnap ? std::round(m_gizmoAccum / ROTATION_SNAP_DEGREES) * ROTATION_SNAP_DEGREES : m_gizmoAccum;

        object.rotation = m_gizmoStartRotation;
        RotationComponent(object.rotation, m_gizmoAxis) += applied;
        FitSizeToModel(object);
        m_dirty = true;
        Rebuild();
        return true;
    }

    if (ImGui::GetIO().WantCaptureMouse)
    {
        m_hoverAxis = -1;
        return false;
    }

    Vector2 mouse = GetMousePosition();

    Vector2 handleScreen = GetWorldToScreen(ScaleHandlePosition(object), m_camera);
    m_scaleHover = std::hypot(mouse.x - handleScreen.x, mouse.y - handleScreen.y) <= SCALE_HANDLE_PICK_PIXELS;
    if (m_scaleHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        Vector3 hit;
        if (RayPlaneHit(GetScreenToWorldRay(mouse, m_camera), center.z, hit))
        {
            m_scaleDragging = true;
            m_scaleMoved = false;
            m_scaleStart = object.scale;
            m_scaleStartDistance = std::hypot(hit.x - center.x, hit.y - center.y);
            return true;
        }
    }

    m_hoverAxis = -1;
    float bestDistance = RING_PICK_PIXELS;
    float bestTheta = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
    {
        float theta;
        float distance = DistanceToRing(m_camera, axis, center, radius, mouse, theta);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestTheta = theta;
            m_hoverAxis = axis;
        }
    }

    if (m_hoverAxis >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        m_gizmoAxis = m_hoverAxis;
        m_gizmoTheta = bestTheta;
        m_gizmoAccum = 0.0f;
        m_gizmoStartRotation = object.rotation;
        m_gizmoMoved = false;
        return true;
    }
    return false;
}

void Editor::UpdatePicking()
{
    if (UpdateGizmo()) return;

    ImGuiIO& io = ImGui::GetIO();
    Ray ray = GetScreenToWorldRay(GetMousePosition(), m_camera);

    if (m_dragging)
    {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) || m_selected < 0)
        {
            m_dragging = false;
            return;
        }

        LevelObject& object = m_level[m_selected];
        Vector3 hit;
        if (!RayPlaneHit(ray, object.position.z, hit)) return;

        Vector3 target = { Snap(hit.x - m_dragOffset.x), Snap(hit.y - m_dragOffset.y), object.position.z };
        float dx = target.x - object.position.x;
        float dy = target.y - object.position.y;
        if (dx == 0.0f && dy == 0.0f) return;

        if (!m_dragMoved)
        {
            PushUndo(m_level); // once per drag, taken just before the first actual movement
            m_dragMoved = true;
        }

        object.position.x = target.x;
        object.position.y = target.y;
        object.slopeStart.x += dx;
        object.slopeStart.y += dy;
        object.slopeEnd.x += dx;
        object.slopeEnd.y += dy;
        // grab heights are absolute world y values, so they travel with the wall
        for (float& h : object.grabHeights) h += dy;

        m_dirty = true;
        Rebuild();
        return;
    }

    if (io.WantCaptureMouse || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    int best = -1;
    float bestDistance = 1e30f;
    for (int i = 0; i < static_cast<int>(m_level.size()); ++i)
    {
        if (!m_pickDecor && m_level[i].kind == ObjectKind::Decor) continue;
        RayCollision hit = GetRayCollisionBox(ray, ObjectBounds(m_level[i]));
        if (hit.hit && hit.distance < bestDistance)
        {
            bestDistance = hit.distance;
            best = i;
        }
    }

    m_selected = best;
    if (best >= 0)
    {
        Vector3 hit;
        if (RayPlaneHit(ray, m_level[best].position.z, hit))
        {
            m_dragOffset = { hit.x - m_level[best].position.x, hit.y - m_level[best].position.y, 0.0f };
            m_dragging = true;
            m_dragMoved = false;
        }
    }
}

void Editor::Update(float deltaTime)
{
    UpdateCamera(deltaTime);
    UpdatePicking();

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    if (IsKeyPressed(KEY_DELETE)) DeleteSelected();
    if (IsKeyPressed(KEY_ESCAPE)) m_selected = -1;
    if (IsKeyPressed(KEY_F) && m_selected >= 0) Select(m_selected);
    if (IsCtrlDown() && IsKeyPressed(KEY_S)) Save();
    if (IsCtrlDown() && IsKeyPressed(KEY_D)) DuplicateSelected();
    if (IsCtrlDown() && IsKeyPressed(KEY_Z))
    {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) Redo();
        else Undo();
    }
    if (IsCtrlDown() && IsKeyPressed(KEY_Y)) Redo();
}

void Editor::DrawWorldOverlay() const
{
    if (m_selected < 0 || m_selected >= static_cast<int>(m_level.size())) return;
    const LevelObject& object = m_level[m_selected];
    DrawBoundingBox(ObjectBounds(object), YELLOW);

    if (object.model.empty()) return;

    // rotation rings, drawn over everything so a model can never hide its own gizmo
    static const Color AXIS_COLORS[3] = { { 230, 60, 60, 255 }, { 60, 200, 60, 255 }, { 70, 110, 240, 255 } };
    float radius = GizmoRadius(object);
    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    for (int axis = 0; axis < 3; ++axis)
    {
        bool highlighted = axis == m_hoverAxis || axis == m_gizmoAxis;
        Color color = highlighted ? YELLOW : AXIS_COLORS[axis];
        Vector3 previous = RingPoint(axis, object.position, radius, 0.0f);
        for (int i = 1; i <= RING_SEGMENTS; ++i)
        {
            Vector3 current = RingPoint(axis, object.position, radius, static_cast<float>(i) / RING_SEGMENTS * 2.0f * PI);
            DrawLine3D(previous, current, color);
            previous = current;
        }
    }

    // scale handle: drag away from / towards the model to grow / shrink it
    Vector3 handle = ScaleHandlePosition(object);
    float handleSize = radius * 0.16f;
    Color handleColor = (m_scaleHover || m_scaleDragging) ? YELLOW : WHITE;
    DrawCube(handle, handleSize, handleSize, handleSize, handleColor);
    DrawCubeWires(handle, handleSize, handleSize, handleSize, BLACK);
    DrawLine3D(object.position, handle, Fade(handleColor, 0.5f));

    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

void Editor::DrawUI()
{
    rlImGuiBegin();

    ImGui::SetNextWindowPos({ 10, 10 }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({ 330, 640 }, ImGuiCond_FirstUseEver);
    ImGui::Begin("Level editor");

    ImGui::Text("%s%s", m_dirty ? "* " : "", m_levelPath.c_str());
    if (ImGui::Button("Save (Ctrl+S)")) Save();
    ImGui::SameLine();
    if (ImGui::Button("Reload")) Reload();
    ImGui::BeginDisabled(m_undoStack.empty());
    if (ImGui::Button("Undo (Ctrl+Z)")) Undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(m_redoStack.empty());
    if (ImGui::Button("Redo (Ctrl+Y)")) Redo();
    ImGui::EndDisabled();
    if (!m_status.empty()) ImGui::TextWrapped("%s", m_status.c_str());

    ImGui::SeparatorText("View");
    ImGui::Checkbox("Snap", &m_snap);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    ImGui::DragFloat("##step", &m_snapStep, 0.05f, 0.05f, 5.0f, "%.2f");
    ImGui::Checkbox("Pick decor", &m_pickDecor);
    ImGui::SameLine();
    ImGui::Checkbox("Snap rotation 15", &m_rotationSnap);
    ImGui::TextDisabled("WASD/arrows/middle-drag pan, wheel zoom, F1 back to game");

    ImGui::SeparatorText("Add");
    for (int k = 0; k < 4; ++k)
    {
        if (k > 0) ImGui::SameLine();
        if (ImGui::Button(KIND_NAMES[k])) AddObject(static_cast<ObjectKind>(k));
    }

    ImGui::SeparatorText("Models (click to place)");
    {
        const auto& names = ListModelNames();
        char filter[64];
        std::snprintf(filter, sizeof(filter), "%s", m_modelFilter.c_str());
        if (ImGui::InputText("Filter", filter, sizeof(filter))) m_modelFilter = filter;
        if (names.empty()) ImGui::TextDisabled("no GLB found in assets/glb");

        if (ImGui::BeginChild("models", { 0, 140 }, ImGuiChildFlags_Borders))
        {
            for (const auto& name : names)
            {
                if (!m_modelFilter.empty() && name.find(m_modelFilter) == std::string::npos) continue;
                if (ImGui::Selectable(name.c_str())) PlaceModel(name);
            }
        }
        ImGui::EndChild();
    }

    ImGui::SeparatorText("Objects");
    if (ImGui::BeginChild("list", { 0, 160 }, ImGuiChildFlags_Borders))
    {
        for (int i = 0; i < static_cast<int>(m_level.size()); ++i)
        {
            const LevelObject& o = m_level[i];
            ImGui::PushID(i);
            if (ImGui::Selectable(TextFormat("%d  %s  (%.1f, %.1f, %.1f)", i, KIND_NAMES[static_cast<int>(o.kind)],
                                              o.position.x, o.position.y, o.position.z),
                                  i == m_selected))
            {
                m_selected = i;
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    if (m_selected >= 0 && m_selected < static_cast<int>(m_level.size()))
    {
        LevelObject& o = m_level[m_selected];
        const LevelObject before = o;
        bool changed = false;

        ImGui::SeparatorText(TextFormat("Object %d", m_selected));
        if (ImGui::Button("Duplicate (Ctrl+D)")) DuplicateSelected();
        ImGui::SameLine();
        if (ImGui::Button("Delete (Del)")) { DeleteSelected(); ImGui::End(); rlImGuiEnd(); return; }

        int kind = static_cast<int>(o.kind);
        if (ImGui::Combo("Kind", &kind, KIND_NAMES, 4))
        {
            o.kind = static_cast<ObjectKind>(kind);
            if (o.kind == ObjectKind::Slope && o.slopeStart.x == o.slopeEnd.x)
            {
                o.slopeStart = { o.position.x - 5.0f, o.position.y };
                o.slopeEnd = { o.position.x + 5.0f, o.position.y + 5.0f };
            }
            changed = true;
        }

        if (o.kind == ObjectKind::Slope)
        {
            changed |= ImGui::DragFloat2("Start (x, y)", &o.slopeStart.x, 0.1f);
            changed |= ImGui::DragFloat2("End (x, y)", &o.slopeEnd.x, 0.1f);
            changed |= ImGui::DragFloat("Depth", &o.size.z, 0.1f, 0.1f, 20.0f);
            o.position = { (o.slopeStart.x + o.slopeEnd.x) * 0.5f, (o.slopeStart.y + o.slopeEnd.y) * 0.5f, 0.0f };
        }
        else
        {
            changed |= ImGui::DragFloat3("Position", &o.position.x, 0.1f);
            changed |= ImGui::DragFloat3("Size", &o.size.x, 0.1f, 0.05f, 1000.0f);
        }

        float color[4] = { o.color.r / 255.0f, o.color.g / 255.0f, o.color.b / 255.0f, o.color.a / 255.0f };
        if (ImGui::ColorEdit4("Color", color))
        {
            o.color = { static_cast<unsigned char>(color[0] * 255.0f + 0.5f), static_cast<unsigned char>(color[1] * 255.0f + 0.5f),
                        static_cast<unsigned char>(color[2] * 255.0f + 0.5f), static_cast<unsigned char>(color[3] * 255.0f + 0.5f) };
            changed = true;
        }

        if (o.kind == ObjectKind::GrabWall)
        {
            float top = o.position.y + o.size.y * 0.5f;
            ImGui::Text("Grab heights (world y, last must = top %.2f)", top);
            for (int i = 0; i < static_cast<int>(o.grabHeights.size()); ++i)
            {
                ImGui::PushID(i);
                changed |= ImGui::DragFloat("##h", &o.grabHeights[i], 0.05f);
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) { o.grabHeights.erase(o.grabHeights.begin() + i); changed = true; ImGui::PopID(); break; }
                ImGui::PopID();
            }
            if (ImGui::SmallButton("+ grab point"))
            {
                o.grabHeights.push_back(top);
                changed = true;
            }
            if (!o.grabHeights.empty() && std::abs(o.grabHeights.back() - top) > 0.01f)
            {
                ImGui::TextColored({ 1.0f, 0.5f, 0.2f, 1.0f }, "Warning: last point is not the wall's top,\nthe player cannot mantle it.");
            }
        }

        if (!o.model.empty())
        {
            ImGui::SeparatorText("Model");
            ImGui::TextDisabled("red/green/blue rings rotate, white cube scales (F focuses)");
            ImGui::TextWrapped("%s", o.model.c_str());
            bool fit = false;
            fit |= ImGui::DragFloat("Scale", &o.scale, 0.02f, 0.05f, 50.0f);
            fit |= ImGui::DragFloat3("Rotation XYZ", &o.rotation.x, 1.0f, -360.0f, 360.0f, "%.0f");
            if (ImGui::SmallButton("X +90")) { o.rotation.x = std::fmod(o.rotation.x + 90.0f, 360.0f); fit = true; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Y +90")) { o.rotation.y = std::fmod(o.rotation.y + 90.0f, 360.0f); fit = true; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Z +90")) { o.rotation.z = std::fmod(o.rotation.z + 90.0f, 360.0f); fit = true; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset")) { o.rotation = { 0.0f, 0.0f, 0.0f }; fit = true; }
            if (fit)
            {
                FitSizeToModel(o); // keep the collider/pick box matching the model
                changed = true;
            }
            if (ImGui::Button("Fit size to model") && FitSizeToModel(o)) changed = true;
            ImGui::SameLine();
            if (ImGui::Button("Remove model"))
            {
                o.model.clear();
                changed = true;
            }
        }

        if (changed)
        {
            // one undo step per widget interaction, not one per frame of a drag
            if (!m_inspectorEditing)
            {
                Level snapshot = m_level;
                snapshot[m_selected] = before;
                PushUndo(snapshot);
                m_inspectorEditing = true;
            }
            m_dirty = true;
            Rebuild();
        }
    }

    if (!ImGui::IsAnyItemActive()) m_inspectorEditing = false;

    ImGui::End();
    rlImGuiEnd();
}
