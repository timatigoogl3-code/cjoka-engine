#include "engine/Renderer/Decal.h"
#include "engine/Renderer/Shader.h"
#include "engine/Renderer/DefaultShaders.h"
#include <vector>
#include <iostream>

namespace Renderer {

GLuint DecalSystem::s_cubeVAO = 0;
GLuint DecalSystem::s_cubeVBO = 0;
GLuint DecalSystem::s_cubeEBO = 0;
std::unique_ptr<Shader> DecalSystem::s_decalShader = nullptr;
bool DecalSystem::s_initialized = false;

void DecalSystem::InitUnitBox() {
    if (s_cubeVAO != 0) return;

    // Unit Cube vertices [-0.5, 0.5]
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };

    uint32_t indices[] = {
        0, 1, 2, 2, 3, 0, // front
        1, 5, 6, 6, 2, 1, // right
        5, 4, 7, 7, 6, 5, // back
        4, 0, 3, 3, 7, 4, // left
        3, 2, 6, 6, 7, 3, // top
        4, 5, 1, 1, 0, 4  // bottom
    };

    glGenVertexArrays(1, &s_cubeVAO);
    glGenBuffers(1, &s_cubeVBO);
    glGenBuffers(1, &s_cubeEBO);

    glBindVertexArray(s_cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, s_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}

void DecalSystem::Init() {
    if (s_initialized) return;
    InitUnitBox();
    s_decalShader = std::make_unique<Shader>(DefaultShaders::kDecalVS, DefaultShaders::kDecalFS);
    s_initialized = true;
}

void DecalSystem::Shutdown() {
    if (!s_initialized) return;
    if (s_cubeVAO) { glDeleteVertexArrays(1, &s_cubeVAO); s_cubeVAO = 0; }
    if (s_cubeVBO) { glDeleteBuffers(1, &s_cubeVBO); s_cubeVBO = 0; }
    if (s_cubeEBO) { glDeleteBuffers(1, &s_cubeEBO); s_cubeEBO = 0; }
    s_decalShader.reset();
    s_initialized = false;
}

void DecalSystem::Render(Registry& reg, const glm::mat4& view, const glm::mat4& proj, GLuint depthTexture) {
    if (!s_initialized) Init();
    if (!s_decalShader) return;

    auto viewDecals = reg.view<Transform, Decal>();
    if (viewDecals.begin() == viewDecals.end()) return;

    // OpenGL Render State for Decals
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Декали не перезаписывают глубину
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); // Рендерим задние грани чтобы камера могла быть внутри бокса декали

    s_decalShader->use();
    s_decalShader->setInt("uDecalTexture", 0);
    s_decalShader->setInt("uDepthTexture", 1);

    glm::mat4 invViewProj = glm::inverse(proj * view);
    s_decalShader->setMat4("uInvViewProj", invViewProj);

    if (depthTexture != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
    }

    glBindVertexArray(s_cubeVAO);

    for (Entity e : viewDecals) {
        auto& d = reg.get<Decal>(e);
        if (!d.visible || !d.texture || !d.texture->valid()) continue;

        auto& tr = reg.get<Transform>(e);
        glm::mat4 model = tr.matrix() * glm::scale(glm::mat4(1.0f), d.size);
        glm::mat4 mvp = proj * view * model;
        glm::mat4 invModel = glm::inverse(model);

        s_decalShader->setMat4("uModel", model);
        s_decalShader->setMat4("uMVP", mvp);
        s_decalShader->setMat4("uInvModel", invModel);
        s_decalShader->setVec4("uDecalColor", d.color);
        s_decalShader->setBool("uIsProjected", d.projected && (depthTexture != 0));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, d.texture->id());

        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glCullFace(GL_BACK);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} // namespace Renderer
