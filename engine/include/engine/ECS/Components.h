#pragma once
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "engine/Renderer/Mesh3D.h"
#include "engine/Renderer/Texture.h"

// ---------- Transform ----------
struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f}; // degrees
    glm::vec3 scale{1.0f};
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
        glm::mat4 m = matrix();
        return glm::normalize(glm::vec3(m[2])); // -Z
    }
};

struct Name { std::string value; };

// ---------- Material (Phong) ----------
struct Material {
    glm::vec3 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    float shininess = 32.0f;
    glm::vec3 emissive{0.0f};

    std::shared_ptr<Texture> diffuseMap;   // slot 0
    std::shared_ptr<Texture> specularMap;  // slot 1
    std::shared_ptr<Texture> normalMap;    // slot 2 (пока не используется)

    bool useDiffuseMap = false;
    bool useSpecularMap = false;
};

// ---------- MeshRenderer — теперь с материалом ----------
struct MeshRenderer {
    std::shared_ptr<Mesh3D> mesh;
    Material material;
    bool visible = true;
    bool castShadow = true;
};

// ---------- Camera ----------
struct Camera {
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    bool primary = true;
    bool perspective = true; // false = ortho
    float orthoSize = 10.0f;

    glm::mat4 projection(float aspect) const {
        if (perspective) return glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
        float h = orthoSize, w = h * aspect;
        return glm::ortho(-w*0.5f, w*0.5f, -h*0.5f, h*0.5f, nearPlane, farPlane);
    }
    // view считается из Transform владельца
    static glm::mat4 viewFromTransform(const Transform& t) {
        glm::vec3 pos = t.position;
        // yaw = rotation.y, pitch = rotation.x
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
};

struct PointLight {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
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
};

// Пост-обработка — один компонент на всю сцену (как в RAGE)
// Разраб правит поля и видит результат сразу, без кода пайплайна.
//   registry.emplace<PostProcessSettings>(scene.create("Post"), PostProcessSettings::Cinematic());
//   или scene.createPost(PostProcessSettings::Vibrant());
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
    float saturation = 1.06f;       // в шейдере 1.08

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
    float radius = 8.0f; // скругление (пока игнор)
};
