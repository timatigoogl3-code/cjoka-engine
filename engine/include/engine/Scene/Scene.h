#pragma once
// Scene — high-level обёртка над Registry с удобными хелперами для разраба
// Пример: scene.createCube({{0,1,0}}, Material{}, "Cube");
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/Assets/AssetManager.h"
#include <string>

class Scene {
public:
    Scene() = default;

    Registry& registry() { return m_registry; }
    const Registry& registry() const { return m_registry; }

    // Базовое создание
    Entity create(const std::string& name = "") {
        Entity e = m_registry.create();
        if (!name.empty()) m_registry.emplace<Name>(e, Name{name});
        return e;
    }
    Entity create(const std::string& name, const Transform& t) {
        Entity e = create(name);
        m_registry.emplace<Transform>(e, t);
        return e;
    }

    // --- Хелперы для разраба (один вызов вместо 3) ---
    Entity createCube(const Transform& t, const Material& mat = {}, const std::string& name = "Cube") {
        Entity e = create(name, t);
        m_registry.emplace<MeshRenderer>(e, MeshRenderer{Assets::Cube(1.0f), mat});
        return e;
    }
    Entity createCube(const std::string& name, const Transform& t, const Material& mat = {}) { return createCube(t, mat, name); }

    Entity createSphere(const Transform& t, const Material& mat = {}, float r = 0.5f, const std::string& name = "Sphere") {
        Entity e = create(name, t);
        m_registry.emplace<MeshRenderer>(e, MeshRenderer{Assets::Sphere(r), mat});
        return e;
    }

    Entity createQuad(const Transform& t, const Material& mat = {}, float size = 1.0f, const std::string& name = "Quad") {
        Entity e = create(name, t);
        m_registry.emplace<MeshRenderer>(e, MeshRenderer{Assets::Quad(size), mat});
        return e;
    }

    Entity createModel(const std::string& objPath, const Transform& t, const Material& mat = {}, const std::string& name = "") {
        Entity e = create(name.empty() ? objPath : name, t);
        m_registry.emplace<MeshRenderer>(e, MeshRenderer{Assets::Mesh(objPath), mat});
        return e;
    }
    // Текстурированная модель одной строкой — самый частый кейс
    Entity createTexturedModel(const std::string& objPath, const std::string& texPath, const Transform& t, const Material& extra = {}, const std::string& name = "") {
        Material m = extra;
        if (!texPath.empty()) {
            m.diffuseMap = Assets::Texture(texPath, true);
            m.useDiffuseMap = m.diffuseMap && m.diffuseMap->valid();
        }
        return createModel(objPath, t, m, name);
    }
    // Горшок — стресс тест: 25k verts / 22k tris, COL текстура 635KB (ранее bbox 8x9 → scale 0.18 ~1.5м)
    Entity createIndoorPlant(const Transform& t, const std::string& name = "Plant") {
        Material m; m.albedo = glm::vec3(1.0f); m.shininess = 48.0f;
        m.diffuseMap = Assets::Texture("assets/textures/indoor_plant_COL.jpg", true);
        m.useDiffuseMap = m.diffuseMap && m.diffuseMap->valid();
        return createModel("assets/models/indoor_plant.obj", t, m, name);
    }
    // Универсальный 1-строчный спавн любой модели+текстуры (без хардкода — просто путь)
    Entity spawn(const std::string& modelPath, const std::string& texPath, const Transform& t, const std::string& name = "") {
        return createTexturedModel(modelPath, texPath, t, {}, name.empty()?modelPath:name);
    }
    // Быстро накидать много одинаковых — для теста батчинга/инстансинга
    std::vector<Entity> createGrid(const std::string& objPath, const std::string& texPath, glm::vec3 origin, int rows, int cols, float spacing, glm::vec3 scale = {1,1,1}, const std::string& baseName = "GridObj") {
        std::vector<Entity> out; out.reserve(static_cast<size_t>(rows*cols));
        auto mesh = Assets::Mesh(objPath);
        auto tex = texPath.empty() ? nullptr : Assets::Texture(texPath);
        for (int r=0;r<rows;++r) for(int c=0;c<cols;++c){
            Transform tr{{origin.x + c*spacing, origin.y, origin.z + r*spacing}, {}, scale};
            Material m; if(tex){ m.diffuseMap=tex; m.useDiffuseMap=true; }
            Entity e = create(baseName+"_"+std::to_string(r)+"_"+std::to_string(c), tr);
            m_registry.emplace<MeshRenderer>(e, MeshRenderer{mesh, m});
            out.push_back(e);
        }
        return out;
    }
    // Дублировать существующий объект со смещением
    Entity duplicate(Entity src, glm::vec3 offset, const std::string& newName = "") {
        if (!m_registry.valid(src)) return NullEntity;
        Entity e = m_registry.create();
        if (m_registry.has<Transform>(src)) {
            Transform t = m_registry.get<Transform>(src);
            t.position += offset;
            m_registry.emplace<Transform>(e, t);
        }
        if (m_registry.has<MeshRenderer>(src)) m_registry.emplace<MeshRenderer>(e, m_registry.get<MeshRenderer>(src));
        if (!newName.empty()) m_registry.emplace<Name>(e, Name{newName});
        else if (m_registry.has<Name>(src)) m_registry.emplace<Name>(e, m_registry.get<Name>(src));
        return e;
    }

    Entity createPointLight(const Transform& t, const PointLight& pl = {}, const std::string& name = "PointLight") {
        Entity e = create(name, t);
        m_registry.emplace<PointLight>(e, pl);
        return e;
    }
    Entity createDirectionalLight(const glm::vec3& dir, const glm::vec3& color = {1,1,1}, float intensity = 1.0f, const std::string& name = "DirLight") {
        Entity e = create(name);
        m_registry.emplace<DirectionalLight>(e, DirectionalLight{dir, color, intensity});
        return e;
    }
    Entity createCamera(const Transform& t, const Camera& cam = {}, const std::string& name = "Camera") {
        Entity e = create(name, t);
        m_registry.emplace<Camera>(e, cam);
        setPrimaryCamera(e);
        return e;
    }
    // Красота одной строкой
    Entity createPost(const PostProcessSettings& s = PostProcessSettings::Cinematic(), const std::string& name = "Post") {
        Entity e = create(name);
        m_registry.emplace<PostProcessSettings>(e, s);
        return e;
    }
    Entity createBeautifulAtmosphere(const Sky& sky = {.top{0.18f,0.42f,0.88f}, .horizon{0.62f,0.76f,0.96f}, .bottom{0.90f,0.92f,0.96f}, .exposure=1.08f},
                                     const Fog& fog = {.color{0.10f,0.12f,0.16f}, .density=0.014f}, const std::string& name="Atmosphere") {
        (void)name;
        registry().emplace<Sky>(create("Sky"), sky);
        registry().emplace<Fog>(create("Fog"), fog);
        return findByName("Sky");
    }

    // Поиск
    Entity findByName(const std::string& name) const {
        for (Entity e : m_registry.view<Name>()) {
            if (m_registry.get<Name>(e).value == name) return e;
        }
        return NullEntity;
    }

    // Камера
    Entity primaryCamera() const {
        for (Entity e : m_registry.view<Camera>()) {
            if (m_registry.get<Camera>(e).primary) return e;
        }
        auto v = m_registry.view<Camera>();
        return v.empty() ? NullEntity : v[0];
    }
    void setPrimaryCamera(Entity e) {
        for (Entity c : m_registry.view<Camera>()) m_registry.get<Camera>(c).primary = false;
        if (m_registry.has<Camera>(e)) m_registry.get<Camera>(e).primary = true;
    }

    void destroy(Entity e) { m_registry.destroy(e); }
    void clear() { m_registry.clear(); }
    size_t alive() const { return m_registry.aliveCount(); }

private:
    Registry m_registry;
};
