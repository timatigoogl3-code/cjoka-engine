#pragma once
// AssetBrowser — быстро добавлять модельки/текстурки/ассеты без кода
//   TAB — палитра, Drag&Drop .obj/.jpg прямо в окно,  QuickSpawn
//   auto e = Assets::QuickSpawn(scene, "myModel.obj"); // найдёт текстуру сам
//   auto files = Assets::ListModels(); // assets/models/*.obj
#include "engine/Scene/Scene.h"
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

namespace Assets {

inline std::vector<std::string> ListFiles(const std::string& dir, const std::vector<std::string>& exts) {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        std::string p = e.path().string();
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        for (auto& want : exts) if (ext == want) { out.push_back(p); break; }
    }
    std::sort(out.begin(), out.end());
    return out;
}
inline std::vector<std::string> ListModels(const std::string& dir = "assets/models") {
    return ListFiles(dir, {".obj", ".fbx", ".gltf", ".glb"});
}
inline std::vector<std::string> ListTextures(const std::string& dir = "assets/textures") {
    return ListFiles(dir, {".png", ".jpg", ".jpeg", ".tga", ".bmp"});
}
// ищет текстуру по имени модели: indoor_plant.obj → indoor_plant_COL.jpg / indoor_plant.png / ...
inline std::string FindTextureForModel(const std::string& modelPath, const std::string& texDir = "assets/textures") {
    std::string base = std::filesystem::path(modelPath).stem().string();
    // точное совпадение
    for (auto& ext : {".png",".jpg",".jpeg"}) {
        std::string cand = texDir + "/" + base + ext;
        if (std::filesystem::exists(cand)) return cand;
        cand = texDir + "/" + base + "_COL.jpg";
        if (std::filesystem::exists(cand)) return cand;
        cand = texDir + "/" + base + "_col.jpg";
        if (std::filesystem::exists(cand)) return cand;
    }
    // частичное: ищем файл содержащий base
    auto texs = ListTextures(texDir);
    for (auto& t : texs) {
        std::string tb = std::filesystem::path(t).stem().string();
        std::transform(tb.begin(), tb.end(), tb.begin(), ::tolower);
        std::string lb = base; std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        if (tb.find(lb)!=std::string::npos || lb.find(tb)!=std::string::npos) return t;
    }
    return {};
}
// Быстрый спавн: модель + авто-текстура (или texOverride)
//   scene.QuickSpawn("assets/models/my.obj") — одна строка
inline Entity QuickSpawn(Scene& scene, const std::string& modelPath, const Transform& t = {}, const std::string& texOverride = "") {
    std::string tex = texOverride;
    if (tex.empty()) tex = FindTextureForModel(modelPath);
    Material m; if (!tex.empty()) { m.diffuseMap = Assets::Texture(tex); m.useDiffuseMap = m.diffuseMap && m.diffuseMap->valid(); }
    std::string name = std::filesystem::path(modelPath).stem().string();
    // scale auto: indoor_plant огромный (bbox 8) → уменьшаем, остальные 1
    Transform tt = t;
    if (modelPath.find("indoor_plant")!=std::string::npos && tt.scale.x==1 && tt.scale.y==1) tt.scale = {0.18f,0.18f,0.18f};
    return scene.createModel(modelPath, tt, m, name);
}
// Одна строка — закинул текстуру на куб/сферу
inline Entity QuickTexturedCube(Scene& scene, const std::string& texPath, const Transform& t = {}, const std::string& name="CubeTex") {
    Material m; m.diffuseMap = Assets::Texture(texPath); m.useDiffuseMap = m.diffuseMap && m.diffuseMap->valid();
    return scene.createCube(t, m, name);
}

} // namespace Assets
