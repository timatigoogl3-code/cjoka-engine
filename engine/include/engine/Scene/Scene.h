#pragma once
// Scene — универсальный контейнер сцены и ECS-оркестратор
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/Physics/Physics.h"
#include "engine/Assets/AssetManager.h"
#include <string>
#include <vector>

class Scene;

// ---------- EntityRef — объектно-ориентированный Handle для Entity ----------
class EntityRef {
public:
    EntityRef() = default;
    EntityRef(Entity e, Scene* sc) : m_e(e), m_scene(sc) {}

    Entity id() const { return m_e; }
    operator Entity() const { return m_e; }
    bool operator==(const EntityRef& o) const { return m_e == o.m_e; }
    bool operator!=(const EntityRef& o) const { return m_e != o.m_e; }
    explicit operator bool() const { return valid(); }

    bool valid() const;
    void destroy();

    template<typename T, typename... Args>
    T& add(Args&&... args);

    template<typename T>
    T& get();

    template<typename T>
    const T& get() const;

    template<typename T>
    T* try_get();

    template<typename T>
    const T* try_get() const;

    template<typename T>
    bool has() const;

    template<typename T>
    void remove();

    Transform& transform();
    const Transform& transform() const;
    MeshRenderer& renderer();
    const MeshRenderer& renderer() const;
    std::string name() const;

private:
    Entity m_e = NullEntity;
    Scene* m_scene = nullptr;
};

// ---------- Scene ----------
class Scene {
private:
    Registry m_registry;

public:
    Scene() = default;

    Registry& registry() { return m_registry; }
    const Registry& registry() const { return m_registry; }

    EntityRef get(Entity e) { return EntityRef(e, this); }

    // Универсальное создание сущностей
    EntityRef create(const std::string& name = "") {
        Entity e = m_registry.create();
        if (!name.empty()) m_registry.emplace<Name>(e, Name{name});
        return EntityRef(e, this);
    }

    EntityRef create(const std::string& name, const Transform& t) {
        EntityRef ref = create(name);
        m_registry.emplace<Transform>(ref.id(), t);
        return ref;
    }

    // Универсальные базовые примитивы
    EntityRef createCube(const Transform& t, const Material& mat = {}, const std::string& name = "Cube") {
        EntityRef ref = create(name, t);
        ref.add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), mat).setClusterLOD(false));
        return ref;
    }
    EntityRef createCube(const std::string& name, const Transform& t, const Material& mat = {}) {
        return createCube(t, mat, name);
    }

    EntityRef createSphere(const Transform& t, const Material& mat = {}, float r = 0.5f, const std::string& name = "Sphere") {
        EntityRef ref = create(name, t);
        ref.add<MeshRenderer>(MeshRenderer(Assets::Sphere(r), mat).setClusterLOD(false));
        return ref;
    }

    EntityRef createQuad(const Transform& t, const Material& mat = {}, float size = 1.0f, const std::string& name = "Quad") {
        EntityRef ref = create(name, t);
        ref.add<MeshRenderer>(MeshRenderer(Assets::Quad(size), mat).setClusterLOD(false));
        return ref;
    }

    EntityRef createModel(const std::string& objPath, const Transform& t, const Material& mat = {}, const std::string& name = "") {
        EntityRef ref = create(name.empty() ? objPath : name, t);
        ref.add<MeshRenderer>(MeshRenderer(Assets::Mesh(objPath), mat));
        return ref;
    }

    EntityRef createTexturedModel(const std::string& objPath, const std::string& texPath, const Transform& t, const Material& extra = {}, const std::string& name = "") {
        Material m = extra;
        if (!texPath.empty()) {
            m.diffuseMap = Assets::Texture(texPath, true);
            m.useDiffuseMap = m.diffuseMap && m.diffuseMap->valid();
        }
        return createModel(objPath, t, m, name);
    }

    EntityRef createClusteredModel(const std::string& objPath, const std::string& texPath, const Transform& t, const Material& extra = {}, const std::string& name = "") {
        Material m = extra;
        if (!texPath.empty()) {
            m.diffuseMap = Assets::Texture(texPath, true);
            m.useDiffuseMap = m.diffuseMap && m.diffuseMap->valid();
        }
        auto cm = Assets::Clustered(objPath);
        EntityRef ref = create(name.empty() ? objPath : name, t);
        ref.add<MeshRenderer>(MeshRenderer(cm, m));
        return ref;
    }

    // Декали (проекционные и плоские)
    EntityRef createDecal(const Transform& t, const std::shared_ptr<Texture>& tex, const glm::vec3& size = {1.0f, 0.5f, 1.0f}, const glm::vec4& tint = {1.0f, 1.0f, 1.0f, 1.0f}, const std::string& name = "Decal") {
        EntityRef ref = create(name, t);
        ref.add<Decal>(Decal::Create(tex, size, tint));
        return ref;
    }

    // Освещение, камера и окружение
    EntityRef createPointLight(const Transform& t, const PointLight& light, const std::string& name = "PointLight") {
        EntityRef ref = create(name, t);
        ref.add<PointLight>(light);
        return ref;
    }

    EntityRef createSun(const glm::vec3& dir = {-0.5f, -1.0f, -0.3f}, const glm::vec3& color = {1.0f, 0.95f, 0.85f}, float intensity = 1.2f, const std::string& name = "Sun") {
        EntityRef ref = create(name);
        ref.add<DirectionalLight>(DirectionalLight{dir, color, intensity});
        return ref;
    }

    EntityRef createCamera(const Transform& t, const Camera& cam = {}, const std::string& name = "Camera") {
        EntityRef ref = create(name, t);
        ref.add<Camera>(cam);
        return ref;
    }

    EntityRef createSky(const Sky& sky = {}, const std::string& name = "Sky") {
        EntityRef ref = create(name);
        ref.add<Sky>(sky);
        return ref;
    }

    EntityRef createPost(const PostProcessSettings& post = {}, const std::string& name = "PostProcess") {
        EntityRef ref = create(name);
        ref.add<PostProcessSettings>(post);
        return ref;
    }

    EntityRef createBeautifulAtmosphere(const Sky& sky = Sky::Sunset(), const Fog& fog = Fog{{0.8f, 0.6f, 0.5f}, 0.015f}) {
        createSky(sky, "Sky");
        create("Fog").add<Fog>(fog);
        return findByName("Sky");
    }

    // Поиск
    EntityRef findByName(const std::string& name) const {
        for (Entity e : m_registry.view<Name>()) {
            if (m_registry.get<Name>(e).value == name) return EntityRef(e, const_cast<Scene*>(this));
        }
        return EntityRef();
    }

    // Камера
    Entity primaryCamera() const {
        for (Entity e : m_registry.view<Camera>()) {
            if (m_registry.get<Camera>(e).primary) return e;
        }
        auto v = m_registry.view<Camera>();
        return (v.begin() == v.end()) ? NullEntity : *v.begin();
    }
    void setPrimaryCamera(Entity e) {
        for (Entity c : m_registry.view<Camera>()) m_registry.get<Camera>(c).primary = false;
        if (m_registry.has<Camera>(e)) m_registry.get<Camera>(e).primary = true;
    }

    void destroy(Entity e) { m_registry.destroy(e); }
    void clear() { m_registry.clear(); }
    size_t alive() const { return m_registry.aliveCount(); }
};

// ---------- EntityRef inline реализации ----------
inline bool EntityRef::valid() const {
    return m_scene && m_scene->registry().valid(m_e);
}
inline void EntityRef::destroy() {
    if (valid()) { m_scene->destroy(m_e); m_e = NullEntity; }
}
template<typename T, typename... Args>
inline T& EntityRef::add(Args&&... args) {
    return m_scene->registry().emplace_or_replace<T>(m_e, std::forward<Args>(args)...);
}
template<typename T>
inline T& EntityRef::get() {
    return m_scene->registry().get<T>(m_e);
}
template<typename T>
inline const T& EntityRef::get() const {
    return m_scene->registry().get<T>(m_e);
}
template<typename T>
inline T* EntityRef::try_get() {
    return m_scene->registry().try_get<T>(m_e);
}
template<typename T>
inline const T* EntityRef::try_get() const {
    return m_scene->registry().try_get<T>(m_e);
}
template<typename T>
inline bool EntityRef::has() const {
    return m_scene->registry().has<T>(m_e);
}
template<typename T>
inline void EntityRef::remove() {
    m_scene->registry().remove<T>(m_e);
}
inline Transform& EntityRef::transform() { return get<Transform>(); }
inline const Transform& EntityRef::transform() const { return get<Transform>(); }
inline MeshRenderer& EntityRef::renderer() { return get<MeshRenderer>(); }
inline const MeshRenderer& EntityRef::renderer() const { return get<MeshRenderer>(); }
inline std::string EntityRef::name() const {
    if (has<Name>()) return get<Name>().value;
    return "Entity_" + std::to_string(static_cast<uint32_t>(m_e));
}
