#pragma once
#include <memory>
#include <string>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "engine/Renderer/Mesh3D.h"
#include "engine/Renderer/MeshClusters.h"
#include "engine/Renderer/Texture.h"

#include "engine/ECS/Registry.h"

// ---------- Transform ----------
struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f}; // euler degrees (pitch, yaw, roll / X, Y, Z)
    glm::vec3 scale{1.0f};
    glm::mat4 prevMatrix{1.0f};  // для motion vectors (velocity buffer)

    Transform() = default;
    Transform(const glm::vec3& pos) : position(pos) {}
    Transform(const glm::vec3& pos, const glm::vec3& rot) : position(pos), rotation(rot) {}
    Transform(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl) : position(pos), rotation(rot), scale(scl) {}

    static Transform FromPosition(const glm::vec3& pos) { return Transform{pos}; }
    static Transform FromPosScale(const glm::vec3& pos, const glm::vec3& scl) { return Transform{pos, {}, scl}; }
    static Transform FromPosScale(const glm::vec3& pos, float uniformScale) { return Transform{pos, {}, glm::vec3(uniformScale)}; }

    // Вызвать в конце кадра чтобы сохранить текущую матрицу для следующего кадра
    void updatePrevMatrix() { prevMatrix = matrix(); }

    glm::mat4 matrix() const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, position);
        m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0,0,1));
        m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0,1,0));
        m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1,0,0));
        m = glm::scale(m, scale);
        return m;
    }

    glm::vec3 forward() const {
        float yaw = glm::radians(rotation.y);
        float pitch = glm::radians(rotation.x);
        return glm::normalize(glm::vec3(
            std::cos(yaw) * std::cos(pitch),
            std::sin(pitch),
            std::sin(yaw) * std::cos(pitch)
        ));
    }
    glm::vec3 right() const {
        return glm::normalize(glm::cross(forward(), glm::vec3(0,1,0)));
    }
    glm::vec3 up() const {
        return glm::normalize(glm::cross(right(), forward()));
    }

    Transform& translate(const glm::vec3& offset) { position += offset; return *this; }
    Transform& rotate(const glm::vec3& deltaDeg) { rotation += deltaDeg; return *this; }
    Transform& setScale(float uniformScale) { scale = glm::vec3(uniformScale); return *this; }
};

struct Name { std::string value; };

// ---------- Hierarchy & Parenting ----------
struct Hierarchy {
    Entity parent = NullEntity;
};

// ---------- Material (PBR Cook-Torrance) ----------
struct Material {
    glm::vec3 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    float shininess = 32.0f; // legacy Phong compatibility
    glm::vec3 emissive{0.0f};

    std::shared_ptr<Texture> diffuseMap;   // slot 0
    std::shared_ptr<Texture> specularMap;  // slot 1
    std::shared_ptr<Texture> normalMap;    // slot 2

    bool useDiffuseMap = false;
    bool useSpecularMap = false;
    bool useNormalMap = false;

    std::string materialPath = "";
    std::string diffuseMapPath = "";
    std::string normalMapPath = "";
    std::string specularMapPath = "";

    // --- Пресеты для быстрой разработки ---
    static Material Default() { return Material{}; }
    static Material Dielectric(const glm::vec3& color = glm::vec3(0.8f), float rough = 0.5f) {
        Material m; m.albedo = color; m.roughness = rough; m.metallic = 0.0f; return m;
    }
    static Material Metal(const glm::vec3& color = glm::vec3(1.0f, 0.86f, 0.57f), float rough = 0.2f) {
        Material m; m.albedo = color; m.metallic = 1.0f; m.roughness = rough; return m;
    }
    static Material Chrome() { return Metal(glm::vec3(0.95f), 0.05f); }
    static Material Gold()   { return Metal(glm::vec3(1.00f, 0.78f, 0.28f), 0.15f); }
    static Material Copper() { return Metal(glm::vec3(0.95f, 0.64f, 0.54f), 0.25f); }
    static Material Plastic(const glm::vec3& color = glm::vec3(0.2f, 0.5f, 0.9f), float rough = 0.4f) {
        return Dielectric(color, rough);
    }
    static Material Rough(const glm::vec3& color = glm::vec3(0.5f)) { return Dielectric(color, 0.85f); }
    static Material Smooth(const glm::vec3& color = glm::vec3(0.9f)) { return Dielectric(color, 0.10f); }
    static Material Emissive(const glm::vec3& color, float intensity = 1.0f) {
        Material m; m.albedo = glm::vec3(0.05f); m.emissive = color * intensity; return m;
    }
    static Material Textured(const std::shared_ptr<Texture>& tex, const glm::vec3& tint = glm::vec3(1.0f), float rough = 0.5f, float met = 0.0f) {
        Material m;
        m.albedo = tint;
        m.roughness = rough;
        m.metallic = met;
        m.diffuseMap = tex;
        m.useDiffuseMap = (tex && tex->valid());
        return m;
    }

    // --- Fluent builder методы ---
    Material& withAlbedo(const glm::vec3& col) { albedo = col; return *this; }
    Material& withRoughness(float r) { roughness = r; return *this; }
    Material& withMetallic(float m) { metallic = m; return *this; }
    Material& withAO(float a) { ao = a; return *this; }
    Material& withEmissive(const glm::vec3& e) { emissive = e; return *this; }
    Material& withTexture(const std::shared_ptr<Texture>& tex) {
        diffuseMap = tex;
        useDiffuseMap = (tex && tex->valid());
        return *this;
    }
    Material& withNormalMap(const std::shared_ptr<Texture>& tex) {
        normalMap = tex;
        useNormalMap = (tex && tex->valid());
        return *this;
    }
    Material& withSpecularMap(const std::shared_ptr<Texture>& tex) {
        specularMap = tex;
        useSpecularMap = (tex && tex->valid());
        return *this;
    }
};

// ---------- MeshRenderer ----------
struct MeshRenderer {
    std::shared_ptr<Mesh3D> mesh;
    std::shared_ptr<ClusteredMesh> clusterMesh; // ClusterLOD путь (приоритет над mesh)
    bool clusterLOD = false;                    // перобъектный выключатель
    bool nanite = false;                        // alias для обратной совместимости
    Material material;
    bool visible = true;
    bool castShadow = true;
    std::string assetPath;
    std::string texturePath;

    MeshRenderer() = default;
    MeshRenderer(std::shared_ptr<Mesh3D> m, const Material& mat = {}) : mesh(std::move(m)), material(mat) {}
    MeshRenderer(std::shared_ptr<ClusteredMesh> cm, const Material& mat = {}) : clusterMesh(std::move(cm)), material(mat) {}

    MeshRenderer& withMaterial(const Material& m) { material = m; return *this; }
    MeshRenderer& setVisible(bool v) { visible = v; return *this; }
    MeshRenderer& setCastShadow(bool s) { castShadow = s; return *this; }
    MeshRenderer& setClusterLOD(bool c) { clusterLOD = c; nanite = c; return *this; }
};

// ---------- SkinnedMeshRenderer & AnimatorComponent ----------
#include "engine/Animation/SkinnedMesh.h"
#include "engine/Animation/Animator.h"

struct SkinnedMeshRenderer {
    std::shared_ptr<Animation::SkinnedMesh> mesh;
    Material material;
    bool visible = true;
    bool castShadow = true;

    SkinnedMeshRenderer() = default;
    SkinnedMeshRenderer(std::shared_ptr<Animation::SkinnedMesh> m, const Material& mat = {})
        : mesh(std::move(m)), material(mat) {}

    SkinnedMeshRenderer& withMaterial(const Material& m) { material = m; return *this; }
    SkinnedMeshRenderer& setVisible(bool v) { visible = v; return *this; }
    SkinnedMeshRenderer& setCastShadow(bool s) { castShadow = s; return *this; }
};

struct AnimatorComponent {
    std::shared_ptr<Animation::Animator> animator;

    AnimatorComponent() = default;
    explicit AnimatorComponent(std::shared_ptr<Animation::Animator> a) : animator(std::move(a)) {}
};

// ---------- Camera ----------
struct Camera {
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 120.0f;
    bool primary = true;
    bool perspective = true; // false = ortho
    float orthoSize = 10.0f;

    glm::mat4 projection(float aspect) const {
        if (perspective) return glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
        float h = orthoSize, w = h * aspect;
        return glm::ortho(-w*0.5f, w*0.5f, -h*0.5f, h*0.5f, nearPlane, farPlane);
    }
    static glm::mat4 viewFromTransform(const Transform& t) {
        glm::vec3 pos = t.position;
        float yaw = glm::radians(t.rotation.y);
        float pitch = glm::radians(t.rotation.x);
        glm::vec3 front{ std::cos(yaw)*std::cos(pitch), std::sin(pitch), std::sin(yaw)*std::cos(pitch) };
        front = glm::normalize(front);
        return glm::lookAt(pos, pos + front, glm::vec3(0,1,0));
    }
};

// ---------- Lighting ----------
struct AmbientLight {
    glm::vec3 color{0.15f, 0.15f, 0.18f};
    float intensity = 1.0f;
};

struct DirectionalLight {
    glm::vec3 direction{-0.5f, -1.0f, -0.3f}; // к свету
    glm::vec3 color{1.0f};
    float intensity = 1.0f;

    static DirectionalLight Sun(const glm::vec3& dir = {-0.55f, -1.0f, -0.35f}, const glm::vec3& col = {1.0f, 0.96f, 0.88f}, float intens = 1.35f) {
        return DirectionalLight{glm::normalize(dir), col, intens};
    }
};

struct PointLight {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;

    static PointLight Warm(const glm::vec3& pos = {}, float range = 12.0f, float intensity = 1.5f) {
        (void)pos;
        return PointLight{{1.0f, 0.75f, 0.42f}, intensity, range, 1.0f, 0.09f, 0.032f};
    }
    static PointLight Cool(const glm::vec3& pos = {}, float range = 12.0f, float intensity = 1.5f) {
        (void)pos;
        return PointLight{{0.60f, 0.80f, 1.00f}, intensity, range, 1.0f, 0.09f, 0.032f};
    }
};

struct SpotLight {
    glm::vec3 direction{0, -1, 0};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float cutoff = 12.5f;      // градусов
    float outerCutoff = 17.5f;
};

// ---------- Атмосфера / красота ----------
struct Fog {
    glm::vec3 color{0.12f, 0.14f, 0.18f};
    float density = 0.02f; // 0 — без тумана
};

struct Sky {
    glm::vec3 top{0.22f, 0.45f, 0.82f};
    glm::vec3 horizon{0.65f, 0.78f, 0.95f};
    glm::vec3 bottom{0.85f, 0.88f, 0.92f};
    float exposure = 1.0f;

    static Sky ClearDay() {
        return Sky{{0.16f, 0.40f, 0.86f}, {0.60f, 0.74f, 0.94f}, {0.88f, 0.90f, 0.95f}, 1.08f};
    }
    static Sky Sunset() {
        return Sky{{0.15f, 0.18f, 0.45f}, {0.92f, 0.45f, 0.20f}, {0.95f, 0.75f, 0.40f}, 1.15f};
    }
    static Sky Night() {
        return Sky{{0.02f, 0.03f, 0.08f}, {0.05f, 0.08f, 0.15f}, {0.08f, 0.10f, 0.18f}, 0.95f};
    }
};

// Пост-обработка — один компонент на всю сцену (как в RAGE / UE)
struct PostProcessSettings {
    bool hdr = true;
    bool bloom = true;
    float bloomThreshold = 0.85f;   // ниже = больше свечения
    float bloomIntensity = 0.62f;   // сила bloom
    int bloomBlurPasses = 2;
    bool fxaa = true;
    float vignette = 0.26f;         // 0 — без, 0.5 — сильно
    float exposure = 1.08f;
    float gamma = 2.2f;
    float saturation = 1.06f;

    static PostProcessSettings Cinematic() { return {.bloomThreshold=0.90f,.bloomIntensity=0.55f,.vignette=0.34f,.exposure=1.05f}; }
    static PostProcessSettings Vibrant()   { return {.bloomThreshold=0.80f,.bloomIntensity=0.75f,.vignette=0.22f,.exposure=1.12f}; }
    static PostProcessSettings Soft()      { return {.bloomThreshold=1.00f,.bloomIntensity=0.42f,.vignette=0.28f,.exposure=1.00f}; }
    static PostProcessSettings Night()     { return {.bloomThreshold=0.75f,.bloomIntensity=0.85f,.vignette=0.40f,.exposure=1.18f}; }
};

// ---------- GUI (kGUI) ----------
struct Text2D {
    std::string text;
    glm::vec2 position{10,10}; // px, origin top-left
    float scale = 1.0f; // 1 = ~24px
    glm::vec4 color{1,1,1,1};
    bool screenSpace = true;
};

struct Panel2D {
    glm::vec2 pos{0}, size{200,100};
    glm::vec4 color{0.1f,0.1f,0.12f,0.85f};
    float radius = 8.0f;
};

// ---------- Decal (Projected OBB / Surface Decal) ----------
struct Decal {
    std::shared_ptr<Texture> texture;
    std::shared_ptr<Texture> normalMap;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec3 size{1.0f, 0.5f, 1.0f}; // box half-extents (X: width, Y: projection depth, Z: length)
    float normalFade = 0.5f;
    bool projected = true; // true = OBB volume projection, false = planar quad
    bool visible = true;

    static Decal Create(const std::shared_ptr<Texture>& tex, const glm::vec3& size = {1.0f, 0.5f, 1.0f}, const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f}) {
        Decal d;
        d.texture = tex;
        d.size = size;
        d.color = color;
        return d;
    }
};

using DecalComponent = Decal;

// ---------- CharacterController ----------
struct CharacterController {
    float radius = 0.4f;
    float height = 1.8f;
    float speed = 8.0f;
    float jumpForce = 5.0f;
    glm::vec3 velocity{0.0f};
    bool onGround = false;
    void* pxController = nullptr; // PxCapsuleController* at runtime
};
