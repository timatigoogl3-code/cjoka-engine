#include "engine/Assets/AssetManager.h"
#include "engine/Renderer/MeshLoader.h"
#include <unordered_map>
#include <iostream>

namespace Assets {

static std::unordered_map<std::string, std::weak_ptr<::Texture>> s_texCache;
static std::unordered_map<std::string, std::weak_ptr<::Mesh3D>> s_meshCache;
static bool s_hotReload = false;

std::shared_ptr<::Texture> Texture(const std::string& path, bool srgb) {
    auto it = s_texCache.find(path);
    if (it != s_texCache.end()) {
        if (auto sp = it->second.lock()) return sp;
    }
    auto tex = std::make_shared<::Texture>(path, true, srgb);
    if (!tex->valid()) {
        std::cerr << "[Assets] Texture fallback white: " << path << "\n";
        tex = ::Texture::White();
    } else {
        std::cout << "[Assets] Texture loaded: " << path << " (" << tex->width() << "x" << tex->height() << ")\n";
    }
    s_texCache[path] = tex;
    return tex;
}

std::shared_ptr<Mesh3D> Mesh(const std::string& path) {
    auto it = s_meshCache.find(path);
    if (it != s_meshCache.end()) {
        if (auto sp = it->second.lock()) return sp;
    }
    auto mesh = MeshLoader::LoadOBJ(path);
    s_meshCache[path] = mesh;
    return mesh;
}

std::shared_ptr<Mesh3D> Cube(float s) {
    std::string key = "cube:" + std::to_string(s);
    auto it = s_meshCache.find(key);
    if (it != s_meshCache.end()) if (auto sp = it->second.lock()) return sp;
    auto sp = MeshLoader::Cube(s);
    s_meshCache[key] = sp;
    return sp;
}
std::shared_ptr<Mesh3D> Quad(float s) {
    std::string key = "quad:" + std::to_string(s);
    auto it = s_meshCache.find(key);
    if (it != s_meshCache.end()) if (auto sp = it->second.lock()) return sp;
    auto sp = MeshLoader::Quad(s);
    s_meshCache[key] = sp;
    return sp;
}
std::shared_ptr<Mesh3D> Sphere(float r, int seg, int rings) {
    std::string key = "sphere:" + std::to_string(r) + ":" + std::to_string(seg);
    auto it = s_meshCache.find(key);
    if (it != s_meshCache.end()) if (auto sp = it->second.lock()) return sp;
    auto sp = MeshLoader::Sphere(r, seg, rings);
    s_meshCache[key] = sp;
    return sp;
}

void Clear() { s_texCache.clear(); s_meshCache.clear(); std::cout << "[Assets] cache cleared\n"; }
void SetHotReload(bool on) { s_hotReload = on; std::cout << "[Assets] hotReload=" << on << "\n"; }
bool HotReloadEnabled() { return s_hotReload; }
size_t CacheSize() { return s_texCache.size() + s_meshCache.size(); }
void Stats() {
    size_t texAlive=0, meshAlive=0;
    for(auto& [k,w]: s_texCache) if(!w.expired()) ++texAlive;
    for(auto& [k,w]: s_meshCache) if(!w.expired()) ++meshAlive;
    std::cout << "[Assets] cache tex=" << s_texCache.size() << " alive=" << texAlive
              << " mesh=" << s_meshCache.size() << " alive=" << meshAlive << " total=" << CacheSize() << "\n";
}

} // namespace Assets
