#pragma once
// Systems.h — ECS системы (header-only, C++20)
// Разделено на секции: рендер, камера, логика. Чистый API для разраба:
//   Systems::Render(registry, shader, window);
//   Systems::FlyCameraSystem(registry, window, dt);
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/Renderer/Shader.h"
#include "engine/Renderer/DefaultShaders.h"
#include "engine/Renderer/Batcher.h"
#include "engine/Renderer/ShadowMap.h"
#include "engine/Core/Window.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

// Глобальная карта теней (одна на кадр — направленный свет)
inline ShadowMap& GlobalShadow() { static ShadowMap s; return s; }

namespace Systems {

// ------------------------------------------------------------------
// Unlit — простой цвет + текстура, без света
// ------------------------------------------------------------------
inline void RenderUnlit(Registry& reg, const Shader& shader, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos = {}) {
    (void)viewPos;
    shader.use();
    for (Entity e : reg.view<Transform, MeshRenderer>()) {
        auto& tr = reg.get<Transform>(e);
        auto& mr = reg.get<MeshRenderer>(e);
        if (!mr.visible || !mr.mesh || mr.mesh->empty()) continue;
        glm::mat4 model = tr.matrix();
        shader.setMat4("uMVP", proj * view * model);
        shader.setMat4("uModel", model);
        if (mr.material.useDiffuseMap && mr.material.diffuseMap && mr.material.diffuseMap->valid()) {
            shader.setBool("uUseDiffuseMap", true);
            mr.material.diffuseMap->bind(0);
            shader.setInt("uDiffuseMap", 0);
        } else {
            shader.setBool("uUseDiffuseMap", false);
        }
        mr.mesh->draw();
    }
}

// ------------------------------------------------------------------
// Lit — Blinn-Phong + Fog + HDR (красивый)
// ------------------------------------------------------------------
inline void RenderLit(Registry& reg, const Shader& shader, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos, ShadowMap* shadow = nullptr) {
    shader.use();
    shader.setMat4("uView", view);
    shader.setMat4("uProj", proj);
    shader.setVec3("uViewPos", viewPos);
    shader.setFloat("uTime", static_cast<float>(glfwGetTime()));

    // Lights
    if (auto v = reg.view<AmbientLight>(); !v.empty()) {
        auto& a = reg.get<AmbientLight>(v[0]);
        shader.setVec3("uAmbientColor", a.color);
        shader.setFloat("uAmbientIntensity", a.intensity);
    } else {
        shader.setVec3("uAmbientColor", glm::vec3(0.15f));
        shader.setFloat("uAmbientIntensity", 1.0f);
    }
    if (auto v = reg.view<DirectionalLight>(); !v.empty()) {
        auto& d = reg.get<DirectionalLight>(v[0]);
        shader.setBool("uHasDirLight", true);
        shader.setVec3("uDirLight.direction", d.direction);
        shader.setVec3("uDirLight.color", d.color);
        shader.setFloat("uDirLight.intensity", d.intensity);
    } else {
        shader.setBool("uHasDirLight", false);
    }
    auto points = reg.view<PointLight, Transform>();
    int cnt = std::min<int>(static_cast<int>(points.size()), 8);
    shader.setInt("uPointLightCount", cnt);
    for (int i = 0; i < cnt; ++i) {
        Entity e = points[static_cast<size_t>(i)];
        auto& pl = reg.get<PointLight>(e);
        auto& tr = reg.get<Transform>(e);
        std::string b = "uPointLights[" + std::to_string(i) + "]";
        shader.setVec3((b + ".position").c_str(), tr.position);
        shader.setVec3((b + ".color").c_str(), pl.color);
        shader.setFloat((b + ".intensity").c_str(), pl.intensity);
        shader.setFloat((b + ".range").c_str(), pl.range);
        shader.setFloat((b + ".constant").c_str(), pl.constant);
        shader.setFloat((b + ".linear").c_str(), pl.linear);
        shader.setFloat((b + ".quadratic").c_str(), pl.quadratic);
    }

    // Fog / Sky
    glm::vec3 fogCol{0.12f, 0.14f, 0.18f};
    float fogDen = 0.025f, exposure = 1.0f;
    if (!reg.view<Fog>().empty()) { auto& f = reg.get<Fog>(reg.view<Fog>()[0]); fogCol = f.color; fogDen = f.density; }
    if (!reg.view<Sky>().empty()) exposure = reg.get<Sky>(reg.view<Sky>()[0]).exposure;
    shader.setVec3("uFogColor", fogCol);
    shader.setFloat("uFogDensity", fogDen);
    shader.setFloat("uExposure", exposure);
    // тени
    shader.setBool("uHasShadow", shadow && shadow->ready());
    if (shadow && shadow->ready()) {
        shader.setMat4("uLightMatrix", shadow->matrix());
        shader.setInt("uShadowMap", 2);
        shadow->Bind(2);
    }

    for (Entity e : reg.view<Transform, MeshRenderer>()) {
        auto& tr = reg.get<Transform>(e);
        auto& mr = reg.get<MeshRenderer>(e);
        if (!mr.visible || !mr.mesh || mr.mesh->empty()) continue;
        glm::mat4 model = tr.matrix();
        shader.setMat4("uMVP", proj * view * model);
        shader.setMat4("uModel", model);
        shader.setMat3("uNormalMat", glm::inverseTranspose(glm::mat3(model)));
        shader.setVec3("uAlbedo", mr.material.albedo);
        shader.setFloat("uShininess", mr.material.shininess);
        shader.setVec3("uEmissive", mr.material.emissive);
        if (mr.material.useDiffuseMap && mr.material.diffuseMap && mr.material.diffuseMap->valid()) {
            shader.setBool("uUseDiffuseMap", true);
            mr.material.diffuseMap->bind(0);
            shader.setInt("uDiffuseMap", 0);
        } else shader.setBool("uUseDiffuseMap", false);
        if (mr.material.useSpecularMap && mr.material.specularMap && mr.material.specularMap->valid()) {
            shader.setBool("uUseSpecularMap", true);
            mr.material.specularMap->bind(1);
            shader.setInt("uSpecularMap", 1);
        } else shader.setBool("uUseSpecularMap", false);
        mr.mesh->draw();
    }
}

// ------------------------------------------------------------------
// Sky — градиентный куб
// ------------------------------------------------------------------
inline void DrawSky(const glm::mat4& view, const glm::mat4& proj, const Sky* sky = nullptr) {
    static Shader* skyShader = nullptr;
    static GLuint skyVAO = 0, skyVBO = 0;
    static bool inited = false;
    if (!inited) {
        skyShader = new Shader(DefaultShaders::kSkyVS, DefaultShaders::kSkyFS);
        float verts[] = {-1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1, -1,-1,1, -1,1,1, 1,1,1, 1,-1,1, -1,1,-1, 1,1,-1, 1,1,1, -1,1,1, -1,-1,-1, -1,-1,1, 1,-1,1, 1,-1,-1, -1,-1,-1, -1,1,-1, -1,1,1, -1,-1,1, 1,-1,-1, 1,-1,1, 1,1,1, 1,1,-1};
        unsigned int idx[] = {0,1,2,2,3,0, 4,5,6,6,7,4, 8,9,10,10,11,8, 12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20};
        glGenVertexArrays(1, &skyVAO); glGenBuffers(1, &skyVBO); GLuint ebo; glGenBuffers(1,&ebo);
        glBindVertexArray(skyVAO);
        glBindBuffer(GL_ARRAY_BUFFER, skyVBO); glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),0);
        glBindVertexArray(0); inited=true;
    }
    glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
    skyShader->use();
    glm::mat4 vNoTrans = glm::mat4(glm::mat3(view));
    skyShader->setMat4("uView", vNoTrans); skyShader->setMat4("uProj", proj);
    if (sky) { skyShader->setVec3("uTopColor", sky->top); skyShader->setVec3("uHorizonColor", sky->horizon); skyShader->setVec3("uBottomColor", sky->bottom); }
    else { skyShader->setVec3("uTopColor", glm::vec3(0.22f,0.45f,0.85f)); skyShader->setVec3("uHorizonColor", glm::vec3(0.65f,0.78f,0.95f)); skyShader->setVec3("uBottomColor", glm::vec3(0.85f,0.88f,0.92f)); }
    skyShader->setFloat("uTime", static_cast<float>(glfwGetTime()));
    glBindVertexArray(skyVAO); glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0); glBindVertexArray(0);
    glDepthMask(GL_TRUE); glDepthFunc(GL_LESS);
}

// ------------------------------------------------------------------
// Shadow pass — глубина сцены с точки солнца
// ------------------------------------------------------------------
inline void RenderShadowPass(Registry& reg, const glm::mat4& lightMatrix) {
    static Shader* sh = nullptr;
    if (!sh) sh = new Shader(DefaultShaders::kShadowDepthVS, DefaultShaders::kShadowDepthFS);
    sh->use();
    sh->setMat4("uLightMatrix", lightMatrix);
    for (Entity e : reg.view<Transform, MeshRenderer>()) {
        auto& mr = reg.get<MeshRenderer>(e);
        if (!mr.visible || !mr.mesh || mr.mesh->empty()) continue;
        if (reg.get<Transform>(e).scale.x <= 0.f) continue;
        // большие статические объекты тоже отбрасывают — castShadow по умолчанию true
        sh->setMat4("uModel", reg.get<Transform>(e).matrix());
        // uLightMatrix уже содержит view*proj; модель отдельно
        // но шейдер один uniform — домножим на CPU: передадим LM*model
        sh->setMat4("uLightMatrix", lightMatrix * reg.get<Transform>(e).matrix());
        mr.mesh->draw();
    }
}

inline glm::mat4 SunLightMatrix(const glm::vec3& camPos, const DirectionalLight& sun) {
    glm::vec3 dir = glm::normalize(sun.direction); // к свету -> вниз; позиция света = -dir
    glm::vec3 center = camPos + glm::vec3(0,0,0) - dir * 10.0f; // чуть вперёд по взгляду не знаем — центр у камеры
    center = camPos;
    glm::vec3 eye = center + (-dir) * 40.0f;   // свет сверху против направления
    glm::mat4 view = glm::lookAt(eye, center, glm::vec3(0,1,0));
    float r = 30.0f;
    glm::mat4 proj = glm::ortho(-r,r,-r,r, 1.0f, 90.0f);
    return proj * view;
}

// ------------------------------------------------------------------
// High-level Render — находит камеру, рисует sky + meshes (с батчингом если выгодно)
// ------------------------------------------------------------------
inline void Render(Registry& reg, const Shader& shader, const Window& window) {
    int w,h; window.getFramebufferSize(w,h);
    float aspect = static_cast<float>(w) / static_cast<float>(h ? h : 1);
    Entity cam = NullEntity;
    for (Entity e : reg.view<Camera, Transform>()) if (reg.get<Camera>(e).primary) { cam=e; break; }
    if (cam==NullEntity) { auto v=reg.view<Camera, Transform>(); if(!v.empty()) cam=v[0]; }
    glm::mat4 view(1), proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    glm::vec3 viewPos{0,0,5};
    Sky* skyPtr=nullptr;
    if (!reg.view<Sky>().empty()) skyPtr=&reg.get<Sky>(reg.view<Sky>()[0]);
    if (cam!=NullEntity) { auto& c=reg.get<Camera>(cam); auto& t=reg.get<Transform>(cam); view=Camera::viewFromTransform(t); proj=c.projection(aspect); viewPos=t.position; }

    // --- SHADOW PASS ---
    bool shadowOn=false;
    glm::mat4 lightMat(1);
    if (auto v = reg.view<DirectionalLight>(); !v.empty()) {
        auto& sun = reg.get<DirectionalLight>(v[0]);
        lightMat = SunLightMatrix(viewPos, sun);
        GlobalShadow().Begin(lightMat);
        RenderShadowPass(reg, lightMat);
        GlobalShadow().End(w,h);
        shadowOn=true;
    }

    DrawSky(view, proj, skyPtr);

    // Динамический батчинг: если много одинаковых мешей — инстансим
    auto viewTrMesh = reg.view<Transform, MeshRenderer>();
    if (viewTrMesh.size() > 6) {
        Batcher batcher; batcher.begin();
        for (Entity e : viewTrMesh) batcher.submit(reg.get<Transform>(e), reg.get<MeshRenderer>(e));
        if (batcher.batchCount() < viewTrMesh.size() / 2) {
            batcher.flush(reg, view, proj, viewPos, shadowOn ? &GlobalShadow() : nullptr);
            return;
        }
    }
    bool hasLight = reg.count<DirectionalLight>()>0 || reg.count<PointLight>()>0;
    if (hasLight) RenderLit(reg, shader, view, proj, viewPos, shadowOn ? &GlobalShadow() : nullptr);
    else RenderUnlit(reg, shader, view, proj, viewPos);
}

// ------------------------------------------------------------------
// FlyCamera — WASD+QE+Shift + RightMouse
// ------------------------------------------------------------------
inline void FlyCameraSystem(Registry& reg, Window& win, float dt) {
    Entity cam=NullEntity;
    for (Entity e: reg.view<Camera, Transform>()) if(reg.get<Camera>(e).primary){cam=e;break;}
    if(cam==NullEntity) return;
    auto& tr=reg.get<Transform>(cam);
    float speed=3.0f*dt * (win.isKeyPressed(GLFW_KEY_LEFT_SHIFT)?2.5f:1.0f);
    float yaw=glm::radians(tr.rotation.y), pitch=glm::radians(tr.rotation.x);
    glm::vec3 front{std::cos(yaw)*std::cos(pitch), std::sin(pitch), std::sin(yaw)*std::cos(pitch)};
    front=glm::normalize(front); glm::vec3 up{0,1,0}; glm::vec3 right=glm::normalize(glm::cross(front,up));
    if(win.isKeyPressed(GLFW_KEY_W)) tr.position+=front*speed;
    if(win.isKeyPressed(GLFW_KEY_S)) tr.position-=front*speed;
    if(win.isKeyPressed(GLFW_KEY_A)) tr.position-=right*speed;
    if(win.isKeyPressed(GLFW_KEY_D)) tr.position+=right*speed;
    if(win.isKeyPressed(GLFW_KEY_Q)) tr.position-=up*speed;
    if(win.isKeyPressed(GLFW_KEY_E)) tr.position+=up*speed;
    static bool grabbing=false; static double lastX=0,lastY=0;
    if(win.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)){
        if(!grabbing){grabbing=true; win.setCursorMode(GLFW_CURSOR_DISABLED); lastX=0;}
        double x,y; win.getCursorPos(x,y);
        if(lastX==0){lastX=x; lastY=y;}
        float dx=static_cast<float>(x-lastX), dy=static_cast<float>(y-lastY);
        lastX=x; lastY=y; tr.rotation.y+=dx*0.12f; tr.rotation.x-=dy*0.12f;
        tr.rotation.x=glm::clamp(tr.rotation.x,-89.0f,89.0f);
    } else if(grabbing){grabbing=false; win.setCursorMode(GLFW_CURSOR_NORMAL); lastX=0;}
}

} // namespace Systems
