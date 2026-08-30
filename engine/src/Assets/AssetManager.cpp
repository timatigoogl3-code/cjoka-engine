#include "engine/Assets/AssetManager.h"
#include "engine/Renderer/MeshLoader.h"
#include "engine/Animation/FBXLoader.h"
#include <unordered_map>
#include <iostream>

namespace Assets {

static std::unordered_map<std::string, std::shared_ptr<::Texture>> s_texCache;
static std::unordered_map<std::string, std::shared_ptr<::Mesh3D>> s_meshCache;
static std::unordered_map<std::string, std::shared_ptr<Animation::SkinnedMesh>> s_skinnedCache;
static bool s_hotReload = false;

std::shared_ptr<::Texture> Texture(const std::string& path, bool srgb) {
    auto it = s_texCache.find(path);
    if (it != s_texCache.end() && it->second && it->second->valid()) {
        return it->second;
    }
    auto tex = std::make_shared<::Texture>(path, true, srgb);
    if (!tex->valid()) {
        std::cerr << "[Assets] Texture fallback white: " << path << "\n";
        return ::Texture::White();
    }
    std::cout << "[Assets] Texture loaded: " << path << " (" << tex->width() << "x" << tex->height() << ")\n";
    s_texCache[path] = tex;
    return tex;
}

std::shared_ptr<Mesh3D> Mesh(const std::string& path) {
    auto it = s_meshCache.find(path);
    if (it != s_meshCache.end() && it->second) {
        return it->second;
    }
    auto mesh = MeshLoader::LoadOBJ(path);
    s_meshCache[path] = mesh;
    return mesh;
}

std::shared_ptr<Animation::SkinnedMesh> Skinned(const std::string& path) {
    auto it = s_skinnedCache.find(path);
    if (it != s_skinnedCache.end() && it->second) {
        return it->second;
    }
    auto skinned = Animation::FBXLoader::LoadFBX(path);
    s_skinnedCache[path] = skinned;
    return skinned;
}

std::shared_ptr<Mesh3D> Cube(float s) {
    std::string key = "cube:" + std::to_string(s);
    auto it = s_meshCache.find(key);
    if (it != s_meshCache.end() && it->second) return it->second;
    auto sp = MeshLoader::Cube(s);
    s_meshCache[key] = sp;
    return sp;
}
std::shared_ptr<Mesh3D> Quad(float s) {
    std::string key = "quad:" + std::to_string(s);
    auto it = s_meshCache.find(key);
    if (it != s_meshCache.end() && it->second) return it->second;
    auto sp = MeshLoader::Quad(s);
    s_meshCache[key] = sp;
    return sp;
}
std::shared_ptr<Mesh3D> Plane(float w, float d, int gx, int gz, float uvX, float uvZ) {
    std::string key = "plane:" + std::to_string(w) + "x" + std::to_string(d) + ":" + std::to_string(gx) + "x" + std::to_string(gz) + ":" + std::to_string(uvX) + "x" + std::to_string(uvZ);
    auto it = s_meshCache.find(key);
    if (it != s_meshCache.end() && it->second) return it->second;
    auto sp = MeshLoader::Plane(w, d, gx, gz, uvX, uvZ);
    s_meshCache[key] = sp;
    return sp;
}
std::shared_ptr<Mesh3D> Sphere(float r, int seg, int rings) {
    std::string key = "sphere:" + std::to_string(r) + ":" + std::to_string(seg);
    auto it = s_meshCache.find(key);
    if (it != s_meshCache.end() && it->second) return it->second;
    auto sp = MeshLoader::Sphere(r, seg, rings);
    s_meshCache[key] = sp;
    return sp;
}

void Clear() { 
    s_texCache.clear(); 
    s_meshCache.clear(); 
    s_skinnedCache.clear();
    std::cout << "[Assets] cache cleared\n"; 
}
void SetHotReload(bool on) { s_hotReload = on; std::cout << "[Assets] hotReload=" << on << "\n"; }
bool HotReloadEnabled() { return s_hotReload; }
size_t CacheSize() { return s_texCache.size() + s_meshCache.size() + s_skinnedCache.size(); }
void Stats() {
    std::cout << "[Assets] cache tex=" << s_texCache.size()
              << " mesh=" << s_meshCache.size()
              << " skinned=" << s_skinnedCache.size() << " total=" << CacheSize() << "\n";
}

} // namespace Assets
