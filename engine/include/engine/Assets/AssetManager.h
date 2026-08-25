#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include "engine/Renderer/Texture.h"
#include "engine/Renderer/Mesh3D.h"

// AssetManager — кэш текстур/мешей, hot-reload для dev
//   auto tex = Assets::Texture("assets/textures/checker.png");
//   auto mesh = Assets::Mesh("assets/models/cube.obj");
//   auto plant = Assets::Texture("assets/textures/indoor_plant_COL.jpg");
namespace Assets {

std::shared_ptr<Texture> Texture(const std::string& path, bool srgb = false);
std::shared_ptr<Mesh3D> Mesh(const std::string& path);
std::shared_ptr<Mesh3D> Cube(float size = 1.0f);
std::shared_ptr<Mesh3D> Quad(float size = 1.0f);
std::shared_ptr<Mesh3D> Sphere(float r = 0.5f, int seg = 32, int rings = 16);

void Clear(); // очистить кэш
void SetHotReload(bool on);
bool HotReloadEnabled();
size_t CacheSize(); // сколько в кэше
void Stats(); // лог

// Удобства: прелоад горшка одной строкой
inline void PreloadIndoorPlant() { Texture("assets/textures/indoor_plant_COL.jpg"); Mesh("assets/models/indoor_plant.obj"); }

} // namespace Assets
