#include "render/ModelCache.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>

namespace
{
    std::unordered_map<std::string, Model> models;
    std::unordered_map<std::string, bool> failed;
    Texture2D sharedAtlas{};

    std::string PathFor(const std::string& name)
    {
        return std::string(SANDOKAN_MODEL_DIR) + "/" + name + ".glb";
    }

    void ShareAtlas(Model& model)
    {
        for (int i = 0; i < model.materialCount; ++i)
        {
            Texture2D& albedo = model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture;
            if (albedo.id == 0) continue; // untextured material: leave it alone

            if (sharedAtlas.id == 0)
            {
                sharedAtlas = albedo; // first atlas becomes the shared one (never unloaded here)
                continue;
            }
            if (albedo.id != sharedAtlas.id && albedo.width == sharedAtlas.width && albedo.height == sharedAtlas.height)
            {
                UnloadTexture(albedo);
                albedo = sharedAtlas;
            }
        }
    }
}

const Model* GetModel(const std::string& name)
{
    if (name.empty()) return nullptr;
    if (auto it = models.find(name); it != models.end()) return &it->second;
    if (failed.count(name)) return nullptr;

    Model model = LoadModel(PathFor(name).c_str());
    if (model.meshCount == 0)
    {
        failed[name] = true;
        return nullptr;
    }

    ShareAtlas(model);
    return &models.emplace(name, model).first->second;
}

const std::vector<std::string>& ListModelNames()
{
    static std::vector<std::string> names;
    static bool scanned = false;
    if (!scanned)
    {
        scanned = true;
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(SANDOKAN_MODEL_DIR, error))
        {
            if (entry.path().extension() == ".glb") names.push_back(entry.path().stem().string());
        }
        std::sort(names.begin(), names.end());
    }
    return names;
}

void UnloadAllModels()
{
    for (auto& [name, model] : models)
    {
        // the atlas is shared between models: detach it so UnloadModel does not free it
        // once per model, then free it exactly once below
        for (int i = 0; i < model.materialCount; ++i)
        {
            if (model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture.id == sharedAtlas.id)
            {
                model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = Texture2D{};
            }
        }
        UnloadModel(model);
    }
    models.clear();
    failed.clear();
    if (sharedAtlas.id != 0) UnloadTexture(sharedAtlas);
    sharedAtlas = Texture2D{};
}
