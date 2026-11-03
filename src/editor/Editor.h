#pragma once

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

#include "world/Level.h"

// In-game level editor: a free camera, mouse picking and dragging of level objects, and an
// ImGui inspector. It edits the Level data and re-spawns the level entities from it, so
// what is on screen is always exactly what gets saved.
class Editor
{
public:
    Editor(Level& level, entt::registry& registry, std::string levelPath);
    ~Editor();

    void Enter(const Camera3D& gameCamera);

    // Selects an object (or none with -1) and, if there is one, centers the camera on it
    void Select(int index);

    // Camera control, picking, dragging, shortcuts. Call once per frame while editing.
    void Update(float deltaTime);

    const Camera3D& GetCamera() const { return m_camera; }

    // Selection outline etc., drawn inside the 3D pass
    void DrawWorldOverlay() const;

    // ImGui panels, drawn after the 3D pass
    void DrawUI();

private:
    void Rebuild();
    void Save();
    void Reload();
    void AddObject(ObjectKind kind);
    void DuplicateSelected();
    void DeleteSelected();
    void PlaceModel(const std::string& name);
    bool FitSizeToModel(LevelObject& object) const;
    void PushUndo(const Level& state);
    void Undo();
    void Redo();
    void UpdateCamera(float deltaTime);
    void UpdatePicking();
    bool UpdateGizmo(); // returns true when the gizmo consumed this frame's mouse input
    float GizmoRadius(const LevelObject& object) const;
    Vector3 ScaleHandlePosition(const LevelObject& object) const;
    bool RayPlaneHit(const Ray& ray, float planeZ, Vector3& outHit) const;
    BoundingBox ObjectBounds(const LevelObject& object) const;
    float Snap(float value) const;

    Level& m_level;
    entt::registry& m_registry;
    std::string m_levelPath;

    Camera3D m_camera{};
    float m_cameraDistance = 40.0f;

    int m_selected = -1;
    bool m_dragging = false;
    Vector3 m_dragOffset{ 0.0f, 0.0f, 0.0f };

    // rotation gizmo: three rings around the selected model, one per world axis
    int m_hoverAxis = -1;          // ring under the mouse, or -1
    int m_gizmoAxis = -1;          // ring being dragged, or -1
    float m_gizmoTheta = 0.0f;     // angle (radians) of the grabbed point along the ring
    float m_gizmoAccum = 0.0f;     // total degrees dragged since the grab
    Vector3 m_gizmoStartRotation{ 0.0f, 0.0f, 0.0f };
    bool m_gizmoMoved = false;     // an undo snapshot was already taken for this drag
    bool m_scaleHover = false;     // mouse over the scale handle
    bool m_scaleDragging = false;
    float m_scaleStart = 1.0f;     // model scale at the moment of the grab
    float m_scaleStartDistance = 1.0f; // mouse distance from the object center at the grab
    bool m_scaleMoved = false;
    bool m_rotationSnap = true;    // snap gizmo rotation to 15 degree steps

    bool m_snap = true;
    float m_snapStep = 0.5f;
    bool m_pickDecor = true;
    // Whole-level snapshots: the level is a few hundred small objects at most, so copying it
    // per edit is far simpler (and safer) than recording per-field commands
    std::vector<Level> m_undoStack;
    std::vector<Level> m_redoStack;
    bool m_dragMoved = false;       // a snapshot was already taken for the current drag
    bool m_inspectorEditing = false; // a snapshot was already taken for the current widget edit
    std::string m_modelFilter;

    bool m_dirty = false; // level changed since the last save
    std::string m_status;
};
