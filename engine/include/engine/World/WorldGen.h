#pragma once
// WorldGen — процедурная генерация мира + массовый спавн моделек.
// Движок не ограничивает количество объектов: тысячи — ок (batching + GL4.6).
//   auto ids = World::Grid(scene, "cube", 20, 20, 1.2f);          // 400 объектов
//   World::Terrain(scene, 64, 64, 40.0f);                          // холмистый ландшафт
//   World::Forest(scene, "assets/models/tree.obj", 150, area);     // лес из моделек
#include "engine/Scene/Scene.h"
#include "engine/Assets/AssetBrowser.h"
#include <random>
#include <vector>

namespace World {

using Factory = std::function<Entity(Scene&, glm::vec3, float yaw, int i)>;

// Сетка объектов NxM — база для городов/улиц
inline std::vector<Entity> Grid(Scene& scene, Factory f, int nx, int nz, float spacing = 2.0f,
                                glm::vec3 origin = {0,-0.9f,0}, float jitter = 0.0f) {
    std::vector<Entity> out; out.reserve(size_t(nx)*size_t(nz));
    std::mt19937 rng{1234};
    std::uniform_real_distribution<float> j(-jitter, jitter);
    for (int x = 0; x < nx; ++x)
        for (int z = 0; z < nz; ++z) {
            glm::vec3 p{origin.x + static_cast<float>(x)*spacing + j(rng), origin.y, origin.z + static_cast<float>(z)*spacing + j(rng)};
            out.push_back(f(scene, p, static_cast<float>(rng()%360), static_cast<int>(out.size())));
        }
    return out;
}

// Круговое кольцо (арена, толпа вокруг центра)
inline std::vector<Entity> Ring(Scene& scene, Factory f, int count, float radius, glm::vec3 center = {0,-0.9f,0}) {
    std::vector<Entity> out; out.reserve(size_t(count));
    for (int i = 0; i < count; ++i) {
        float a = static_cast<float>(i)/static_cast<float>(count) * 6.2831853f;
        glm::vec3 p{center.x + std::cos(a)*radius, center.y, center.z + std::sin(a)*radius};
        out.push_back(f(scene, p, -glm::degrees(a)+90.0f, i));
    }
    return out;
}

// Ландшафт из квада с вершинными высотами (проще некуда, но свой mesh можно)
inline Entity Terrain(Scene& scene, int nx, int nz, float heightScale = 8.0f, float size = 60.0f) {
    // генерируем меш вручную через Mesh3D API
    std::vector<Vertex> verts; std::vector<uint32_t> idx;
    verts.reserve(size_t(nx*nz));
    std::mt19937 rng{42};
    for (int z = 0; z < nz; ++z)
        for (int x = 0; x < nx; ++x) {
            float fx = static_cast<float>(x)/static_cast<float>(nx-1);
            float fz = static_cast<float>(z)/static_cast<float>(nz-1);
            float h = std::sin(fx*6.28f)*std::cos(fz*4.5f)*heightScale*0.15f
                    + std::sin((fx+fz)*9.0f)*heightScale*0.05f;
            glm::vec3 pos{(fx-0.5f)*size, h - 1.0f, (fz-0.5f)*size};
            verts.push_back({pos, {0.55f+0.1f*h*0.06f,0.62f,0.45f}, {0,1,0}, {fx*8,fz*8}});
        }
    for (int z = 0; z < nz-1; ++z)
        for (int x = 0; x < nx-1; ++x) {
            uint32_t a = uint32_t(z*nx+x), b = a+1, c = a+uint32_t(nx), d = c+1;
            idx.insert(idx.end(), {a,c,b, b,c,d});
        }
    // нормали усредним по треугольникам
    for (size_t i = 0; i+2 < idx.size(); i += 3) {
        auto& A = verts[idx[i]]; auto& B = verts[idx[i+1]]; auto& C = verts[idx[i+2]];
        glm::vec3 n = glm::normalize(glm::cross(B.position-A.position, C.position-A.position));
        A.normal += n; B.normal += n; C.normal += n;
    }
    for (auto& v : verts) v.normal = glm::normalize(v.normal);
    auto mesh = std::make_shared<Mesh3D>(std::move(verts), std::move(idx));
    Entity e = scene.create("Terrain").id();
    Material m = Material::Dielectric({1,1,1}, 0.8f);
    scene.registry().emplace<MeshRenderer>(e, MeshRenderer(mesh, m));
    return e;
}

// Разбросать модельки по области (камни, деревья, пропсы)
inline std::vector<Entity> Scatter(Scene& scene, const std::string& modelPath, int count,
                                   glm::vec3 areaMin, glm::vec3 areaMax, const std::string& texPath = "") {
    std::vector<Entity> out; out.reserve(size_t(count));
    std::mt19937 rng{std::hash<std::string>{}(modelPath)};
    std::uniform_real_distribution<float> dx(areaMin.x, areaMax.x), dy(areaMin.y, areaMax.y), dz(areaMin.z, areaMax.z);
    for (int i = 0; i < count; ++i)
        out.push_back(Assets::QuickSpawn(scene, modelPath, {{dx(rng),dy(rng),dz(rng)}, {0,float(rng()%360),0}, {1,1,1}}, texPath));
    return out;
}

} // namespace World
