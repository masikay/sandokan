#include "world/Level.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "ecs/Components.h"

namespace
{
    using Json = nlohmann::json;

    // an [min, max] span of x to skip when scattering ground-level decoration
    struct XRange {
        float min;
        float max;
    };

    // ranges with no ground at y = 0 (chasms, and the suspended ledge bridge): decoration
    // placed at "ground height" there would visibly float over open air
    const std::vector<XRange> NO_GROUND_RANGES = {
        { -40.0f, -34.0f }, // left chasm
        { 20.0f, 26.0f },   // right chasm
        { 52.0f, 92.0f },   // ledges and the chasm between the two handhold walls
    };

    bool IsInAnyRange(float x, const std::vector<XRange>& ranges)
    {
        for (const auto& range : ranges)
        {
            if (x >= range.min && x <= range.max) return true;
        }
        return false;
    }

    LevelObject Box(ObjectKind kind, Vector3 position, Vector3 size, Color color)
    {
        LevelObject object;
        object.kind = kind;
        object.position = position;
        object.size = size;
        object.color = color;
        return object;
    }

    // evenly spaced decor columns along x at a given depth, skipping spans with no ground
    void AddColumnRow(Level& level, float depthZ, float spacing, int count, Vector3 size, Color color,
                      const std::vector<XRange>& skip = {})
    {
        float halfSpan = spacing * static_cast<float>(count - 1) * 0.5f;
        for (int i = 0; i < count; ++i)
        {
            float x = static_cast<float>(i) * spacing - halfSpan;
            if (IsInAnyRange(x, skip)) continue;
            level.push_back(Box(ObjectKind::Decor, { x, size.y * 0.5f, depthZ }, size, color));
        }
    }

    void AddGrabWall(Level& level, Vector3 position, Vector3 size, Color color, std::vector<float> heights)
    {
        LevelObject wall = Box(ObjectKind::GrabWall, position, size, color);
        wall.grabHeights = std::move(heights);
        level.push_back(std::move(wall));
    }

    void AddSlope(Level& level, Vector2 start, Vector2 end, Color color)
    {
        LevelObject slope = Box(ObjectKind::Slope, { (start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f, 0.0f },
                                { end.x - start.x, end.y - start.y, 2.0f }, color);
        slope.slopeStart = start;
        slope.slopeEnd = end;
        level.push_back(std::move(slope));
    }

    const char* KindName(ObjectKind kind)
    {
        switch (kind)
        {
            case ObjectKind::Decor: return "decor";
            case ObjectKind::Platform: return "platform";
            case ObjectKind::GrabWall: return "grab_wall";
            case ObjectKind::Slope: return "slope";
        }
        return "platform";
    }

    ObjectKind KindFromName(const std::string& name)
    {
        if (name == "decor") return ObjectKind::Decor;
        if (name == "grab_wall") return ObjectKind::GrabWall;
        if (name == "slope") return ObjectKind::Slope;
        return ObjectKind::Platform;
    }

    Json ToJson(const LevelObject& o)
    {
        Json j;
        j["kind"] = KindName(o.kind);
        j["position"] = { o.position.x, o.position.y, o.position.z };
        j["size"] = { o.size.x, o.size.y, o.size.z };
        j["color"] = { o.color.r, o.color.g, o.color.b, o.color.a };
        if (!o.model.empty())
        {
            j["model"] = o.model;
            j["rotation"] = { o.rotation.x, o.rotation.y, o.rotation.z };
            j["scale"] = o.scale;
        }
        if (o.kind == ObjectKind::GrabWall) j["grabHeights"] = o.grabHeights;
        if (o.kind == ObjectKind::Slope)
        {
            j["slopeStart"] = { o.slopeStart.x, o.slopeStart.y };
            j["slopeEnd"] = { o.slopeEnd.x, o.slopeEnd.y };
        }
        return j;
    }

    LevelObject FromJson(const Json& j)
    {
        LevelObject o;
        o.kind = KindFromName(j.value("kind", "platform"));
        o.position = { j.at("position")[0], j.at("position")[1], j.at("position")[2] };
        o.size = { j.at("size")[0], j.at("size")[1], j.at("size")[2] };
        o.color = { j.at("color")[0], j.at("color")[1], j.at("color")[2], j.at("color")[3] };
        o.model = j.value("model", "");
        if (j.contains("rotation")) o.rotation = { j["rotation"][0], j["rotation"][1], j["rotation"][2] };
        else o.rotation = { 0.0f, j.value("rotationY", 0.0f), 0.0f }; // files saved before 3-axis rotation
        o.scale = j.value("scale", 1.0f);
        if (j.contains("grabHeights")) o.grabHeights = j["grabHeights"].get<std::vector<float>>();
        if (j.contains("slopeStart")) o.slopeStart = { j["slopeStart"][0], j["slopeStart"][1] };
        if (j.contains("slopeEnd")) o.slopeEnd = { j["slopeEnd"][0], j["slopeEnd"][1] };
        return o;
    }
}

Level BuildDefaultLevel()
{
    Level level;
    const Color ground{ 90, 70, 40, 255 };
    const Color stone{ 110, 90, 60, 255 };
    const Color wallColor{ 80, 65, 55, 255 };
    const Color plainWall{ 100, 95, 90, 255 };
    auto P = [&](Vector3 pos, Vector3 size, Color color) { level.push_back(Box(ObjectKind::Platform, pos, size, color)); };

    // ground segments with real chasms between them: with jumpSpeed 14, moveSpeed 8 and
    // gravity -30 a level jump reaches ~7.5 units, so every 6 unit chasm is safely jumpable
    P({ -145.0f, 0.25f, 0.0f }, { 210.0f, 0.5f, 2.0f }, ground); // far left
    P({ -22.0f, 0.25f, 0.0f }, { 24.0f, 0.5f, 2.0f }, ground);   // left of spawn (chasm x = -40..-34)
    P({ 5.0f, 0.25f, 0.0f }, { 30.0f, 0.5f, 2.0f }, ground);     // spawn area (chasm x = 20..26)
    P({ 37.0f, 0.25f, 0.0f }, { 22.0f, 0.5f, 2.0f }, ground);    // up to the first wall (x = 48)
    P({ 176.0f, 0.25f, 0.0f }, { 160.0f, 0.5f, 2.0f }, ground);  // resumes past the second wall (x = 96)

    // a low wall that a normal jump clears, and a taller one needing a ledge grab
    P({ -60.0f, 1.25f, 0.0f }, { 2.0f, 2.5f, 2.0f }, plainWall);
    AddGrabWall(level, { -90.0f, 2.5f, 0.0f }, { 2.0f, 5.0f, 2.0f }, plainWall, { 5.0f });

    // ground patches under each handhold wall so a full descent lands on solid ground
    P({ 50.0f, 0.25f, 0.0f }, { 4.0f, 0.5f, 2.0f }, ground);
    P({ 94.0f, 0.25f, 0.0f }, { 4.0f, 0.5f, 2.0f }, ground);

    // handhold walls (collider bottom at y = 0.5, top at 10) and the suspended ledges between
    AddGrabWall(level, { 50.0f, 5.25f, 0.0f }, { 4.0f, 9.5f, 2.0f }, wallColor, { 4.0f, 7.5f, 10.0f });
    P({ 60.0f, 9.5f, 0.0f }, { 16.0f, 1.0f, 2.0f }, stone); // ledge 1, x = 52..68
    P({ 83.0f, 9.5f, 0.0f }, { 18.0f, 1.0f, 2.0f }, stone); // ledge 2, x = 74..92
    AddGrabWall(level, { 94.0f, 5.25f, 0.0f }, { 4.0f, 9.5f, 2.0f }, wallColor, { 4.0f, 7.5f, 10.0f });

    // a walkable hill
    AddSlope(level, { 130.0f, 0.5f }, { 145.0f, 5.5f }, ground);
    AddSlope(level, { 145.0f, 5.5f }, { 160.0f, 0.5f }, ground);

    // backdrops and parallax decoration at increasing depth
    level.push_back(Box(ObjectKind::Decor, { 0.0f, 10.0f, -20.0f }, { 600.0f, 20.0f, 4.0f }, { 40, 110, 60, 255 }));
    level.push_back(Box(ObjectKind::Decor, { 0.0f, 17.5f, -45.0f }, { 700.0f, 35.0f, 4.0f }, { 25, 70, 90, 255 }));
    AddColumnRow(level, -3.0f, 15.0f, 23, { 0.5f, 3.0f, 0.5f }, { 230, 180, 40, 255 }, NO_GROUND_RANGES);
    AddColumnRow(level, -18.0f, 25.0f, 15, { 2.0f, 9.0f, 2.0f }, { 30, 90, 50, 255 });
    AddColumnRow(level, -40.0f, 45.0f, 9, { 3.0f, 14.0f, 3.0f }, { 35, 65, 80, 255 });
    AddColumnRow(level, 2.0f, 18.0f, 19, { 1.5f, 6.0f, 0.3f }, { 20, 50, 28, 255 }, NO_GROUND_RANGES);

    return level;
}

bool SaveLevel(const Level& level, const std::string& path)
{
    Json objects = Json::array();
    for (const auto& object : level) objects.push_back(ToJson(object));

    Json root;
    root["version"] = 1;
    root["objects"] = objects;

    std::ofstream file(path);
    if (!file) return false;
    file << root.dump(1) << '\n';
    return file.good();
}

bool LoadLevel(Level& level, const std::string& path)
{
    std::ifstream file(path);
    if (!file) return false;

    try
    {
        Json root = Json::parse(file);
        Level loaded;
        for (const auto& j : root.at("objects")) loaded.push_back(FromJson(j));
        level = std::move(loaded);
        return true;
    }
    catch (const Json::exception&)
    {
        return false;
    }
}

void SpawnLevel(entt::registry& registry, const Level& level)
{
    for (const auto& object : level)
    {
        auto entity = registry.create();
        registry.emplace<LevelEntity>(entity);

        if (object.kind == ObjectKind::Slope)
        {
            registry.emplace<Slope>(entity, object.slopeStart, object.slopeEnd, object.size.z, object.color);
            continue;
        }

        registry.emplace<Transform3D>(entity, object.position);
        registry.emplace<BoxRenderable>(entity, object.size, object.color);
        if (!object.model.empty()) registry.emplace<ModelRenderable>(entity, object.model, object.rotation, object.scale);

        if (object.kind == ObjectKind::Decor) continue;

        registry.emplace<Collider>(entity, Vector3{ object.size.x * 0.5f, object.size.y * 0.5f, object.size.z * 0.5f });
        if (object.kind == ObjectKind::GrabWall) registry.emplace<GrabPoints>(entity, object.grabHeights);
    }
}

void ClearLevelEntities(entt::registry& registry)
{
    auto view = registry.view<LevelEntity>();
    registry.destroy(view.begin(), view.end());
}
