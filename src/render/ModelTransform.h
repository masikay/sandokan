#pragma once

#include <raylib.h>
#include <raymath.h>

// Transform applied to a model's own vertices: uniform scale, then rotation around X, Y and
// Z (degrees, raylib's XYZ order). Shared by rendering and the editor so the collider box
// always matches what is drawn.
inline Matrix ModelMatrix(Vector3 rotationDegrees, float scale)
{
    Vector3 radians = { rotationDegrees.x * DEG2RAD, rotationDegrees.y * DEG2RAD, rotationDegrees.z * DEG2RAD };
    return MatrixMultiply(MatrixScale(scale, scale, scale), MatrixRotateXYZ(radians));
}

// Axis-aligned bounds of the model after ModelMatrix, in the model's own (unplaced) space
inline BoundingBox TransformedModelBounds(const Model& model, Vector3 rotationDegrees, float scale)
{
    BoundingBox bounds = GetModelBoundingBox(model);
    Matrix matrix = ModelMatrix(rotationDegrees, scale);

    Vector3 min = { 1e30f, 1e30f, 1e30f };
    Vector3 max = { -1e30f, -1e30f, -1e30f };
    for (int i = 0; i < 8; ++i)
    {
        Vector3 corner = { (i & 1) ? bounds.max.x : bounds.min.x, (i & 2) ? bounds.max.y : bounds.min.y,
                           (i & 4) ? bounds.max.z : bounds.min.z };
        Vector3 t = Vector3Transform(corner, matrix);
        min = { std::min(min.x, t.x), std::min(min.y, t.y), std::min(min.z, t.z) };
        max = { std::max(max.x, t.x), std::max(max.y, t.y), std::max(max.z, t.z) };
    }
    return { min, max };
}
