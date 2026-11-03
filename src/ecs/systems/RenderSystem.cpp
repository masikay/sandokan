#include "RenderSystem.h"

#include <cmath>

#include <raylib.h>

#include "../Components.h"
#include "render/ModelCache.h"
#include "render/ModelTransform.h"

namespace
{
    // draws one part of the player figure as a box, with the same wireframe outline style
    // used for every other placeholder box in the scene
    void DrawBodyPart(Vector3 center, Vector3 size, Color color)
    {
        DrawCube(center, size.x, size.y, size.z, color);
        DrawCubeWires(center, size.x, size.y, size.z, Fade(BLACK, 0.3f));
    }

    // placeholder player figure: head, torso, legs and arms as separate boxes around the
    // same center used for collision (still a single AABB there - this is purely visual).
    // While hanging from a ledge, the arms are drawn raised instead of at the torso's sides,
    // as a visual readout of the grab.
    void DrawPlayerFigure(Vector3 center, bool hanging)
    {
        DrawBodyPart(Vector3{ center.x, center.y - 0.6f, center.z }, { 0.9f, 0.8f, 0.9f }, Color{ 60, 45, 35, 255 });   // legs
        DrawBodyPart(Vector3{ center.x, center.y + 0.05f, center.z }, { 0.9f, 0.7f, 0.9f }, RED);                      // torso
        DrawBodyPart(Vector3{ center.x, center.y + 0.75f, center.z }, { 0.6f, 0.5f, 0.6f }, Color{ 220, 180, 140, 255 }); // head

        float armY = hanging ? center.y + 1.1f : center.y;
        DrawBodyPart(Vector3{ center.x - 0.55f, armY, center.z }, { 0.25f, 0.7f, 0.25f }, RED); // left arm
        DrawBodyPart(Vector3{ center.x + 0.55f, armY, center.z }, { 0.25f, 0.7f, 0.25f }, RED); // right arm
    }
}

void RenderSystem(entt::registry& registry)
{
    auto boxView = registry.view<Transform3D, BoxRenderable>();
    for (auto entity : boxView)
    {
        if (registry.all_of<PlayerControlled>(entity)) continue; // drawn as a figure below

        auto& transform = boxView.get<Transform3D>(entity);
        auto& box = boxView.get<BoxRenderable>(entity);

        if (auto* modelInfo = registry.try_get<ModelRenderable>(entity))
        {
            if (const Model* model = GetModel(modelInfo->name))
            {
                // models are authored with their pivot somewhere (usually the base); draw
                // them so the middle of their transformed bounds sits at the entity position,
                // which is also the middle of the collider box
                BoundingBox bounds = TransformedModelBounds(*model, modelInfo->rotation, modelInfo->scale);
                Vector3 center = { (bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f,
                                   (bounds.min.z + bounds.max.z) * 0.5f };
                Vector3 origin = { transform.position.x - center.x, transform.position.y - center.y,
                                   transform.position.z - center.z };

                // Model is a small struct of pointers, so a by-value copy is cheap; its
                // transform is applied to the vertices before DrawModel's own translation
                Model placed = *model;
                placed.transform = ModelMatrix(modelInfo->rotation, modelInfo->scale);
                DrawModel(placed, origin, 1.0f, WHITE);
                continue;
            }
        }

        DrawCube(transform.position, box.size.x, box.size.y, box.size.z, box.color);
        DrawCubeWires(transform.position, box.size.x, box.size.y, box.size.z, Fade(BLACK, 0.3f));
    }

    // grab points: small bright markers sticking out of the wall's climbing face(s) in x -
    // the side(s) the player actually stands on while grabbing them, not an arbitrary
    // z-offset towards the camera - so the handholds LedgeGrabSystem uses are visible
    // instead of being an invisible game rule. Drawn on both faces, since a wall can be
    // climbed from either side depending on the level (e.g. a connector between two areas).
    auto grabWallView = registry.view<Transform3D, Collider, GrabPoints>();
    for (auto entity : grabWallView)
    {
        auto& wallTransform = grabWallView.get<Transform3D>(entity);
        auto& collider = grabWallView.get<Collider>(entity);
        auto& grabPoints = grabWallView.get<GrabPoints>(entity);

        float leftX = wallTransform.position.x - collider.halfExtents.x - 0.2f;
        float rightX = wallTransform.position.x + collider.halfExtents.x + 0.2f;
        for (float height : grabPoints.heights)
        {
            for (float x : { leftX, rightX })
            {
                Vector3 markerPos{ x, height, wallTransform.position.z };
                DrawCube(markerPos, 0.3f, 0.3f, 0.6f, ORANGE);
                DrawCubeWires(markerPos, 0.3f, 0.3f, 0.6f, Fade(BLACK, 0.4f));
            }
        }
    }

    auto playerView = registry.view<Transform3D, PlayerControlled>();
    for (auto entity : playerView)
    {
        auto& transform = playerView.get<Transform3D>(entity);
        DrawPlayerFigure(transform.position, registry.all_of<LedgeHang>(entity));
    }

    // placeholder ramp visual: a flat sloped panel between the two surface points, drawn
    // with both winding orders so it stays visible regardless of which side faces the camera
    auto slopeView = registry.view<Slope>();
    for (auto entity : slopeView)
    {
        auto& slope = slopeView.get<Slope>(entity);
        float halfDepth = slope.depth * 0.5f;

        Vector3 p1{ slope.start.x, slope.start.y, -halfDepth };
        Vector3 p2{ slope.end.x, slope.end.y, -halfDepth };
        Vector3 p3{ slope.end.x, slope.end.y, halfDepth };
        Vector3 p4{ slope.start.x, slope.start.y, halfDepth };

        DrawTriangle3D(p1, p2, p3, slope.color);
        DrawTriangle3D(p1, p3, p4, slope.color);
        DrawTriangle3D(p3, p2, p1, slope.color);
        DrawTriangle3D(p4, p3, p1, slope.color);
    }
}
