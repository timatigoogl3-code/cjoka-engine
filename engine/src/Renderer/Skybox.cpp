#include "engine/Renderer/Skybox.h"
#include "engine/Renderer/Shader.h"
#include "engine/Renderer/DefaultShaders.h"
#include "engine/Renderer/DepthState.h"
#include "engine/Renderer/Texture.h"
#include "stb/stb_image.h"
#include <iostream>
#include <vector>

static Shader* s_skyShader = nullptr;
static bool s_skyInit = false;

Skybox::Skybox(const std::string& hdrPath) {
    createCube();
    if (hdrPath.empty()) return;
    // Загрузка HDR equirectangular
    stbi_set_flip_vertically_on_load(true);
    int w,h,comp;
    float* data = stbi_loadf(hdrPath.c_str(), &w, &h, &comp, 3);
    if (!data) {
        std::cerr << "[Skybox] HDR load failed: " << hdrPath << " — " << stbi_failure_reason() << " → procedural\n";
        return;
    }
    std::cout << "[Skybox] HDR " << hdrPath << " " << w << "x" << h << "\n";
    // Для простоты — конвертим в cubemap 6×512 через CPU (упрощённо: оставляем как equirectangular и сэмплим в шейдере)
    // Пока используем как 2D текстуру для процедурного, но помечаем HDR
    glGenTextures(1, &m_cubemap);
    glBindTexture(GL_TEXTURE_2D, m_cubemap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);
    m_isHDR = true;
}

Skybox::~Skybox() {
    if (m_cubemap) glDeleteTextures(1, &m_cubemap);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
}
Skybox::Skybox(Skybox&& o) noexcept : m_vao(o.m_vao), m_vbo(o.m_vbo), m_ebo(o.m_ebo), m_cubemap(o.m_cubemap), m_isHDR(o.m_isHDR) { o.m_vao=o.m_vbo=o.m_ebo=o.m_cubemap=0; }
Skybox& Skybox::operator=(Skybox&& o) noexcept {
    if(this!=&o){ if(m_cubemap) glDeleteTextures(1,&m_cubemap); if(m_vao) glDeleteVertexArrays(1,&m_vao); if(m_vbo) glDeleteBuffers(1,&m_vbo); if(m_ebo) glDeleteBuffers(1,&m_ebo);
        m_vao=o.m_vao; m_vbo=o.m_vbo; m_ebo=o.m_ebo; m_cubemap=o.m_cubemap; m_isHDR=o.m_isHDR; o.m_vao=o.m_vbo=o.m_ebo=o.m_cubemap=0; } return *this;
}

void Skybox::createCube() {
    float verts[] = {-1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1, -1,-1,1, -1,1,1, 1,1,1, 1,-1,1, -1,1,-1, 1,1,-1, 1,1,1, -1,1,1, -1,-1,-1, -1,-1,1, 1,-1,1, 1,-1,-1, -1,-1,-1, -1,1,-1, -1,1,1, -1,-1,1, 1,-1,-1, 1,-1,1, 1,1,1, 1,1,-1};
    unsigned int idx[] = {0,1,2,2,3,0, 4,5,6,6,7,4, 8,9,10,10,11,8, 12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20};
    glGenVertexArrays(1,&m_vao); glGenBuffers(1,&m_vbo); glGenBuffers(1,&m_ebo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER,m_vbo); glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),0);
    glBindVertexArray(0);
}

void Skybox::draw(const glm::mat4& view, const glm::mat4& proj) const {
    if (!s_skyInit) { s_skyShader = new Shader(DefaultShaders::kSkyVS, DefaultShaders::kSkyFS); s_skyInit=true; }
    glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
    s_skyShader->use();
    glm::mat4 vNoTrans = glm::mat4(glm::mat3(view));
    s_skyShader->setMat4("uView", vNoTrans); s_skyShader->setMat4("uProj", proj);
    // HDR цвета — ярче для bloom
    s_skyShader->setVec3("uTopColor", glm::vec3(0.22f,0.45f,0.82f)*1.8f);
    s_skyShader->setVec3("uHorizonColor", glm::vec3(0.65f,0.78f,0.95f)*1.2f);
    s_skyShader->setVec3("uBottomColor", glm::vec3(0.85f,0.88f,0.92f));
    s_skyShader->setFloat("uTime", 0.0f);
    if (m_cubemap) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_cubemap); s_skyShader->setInt("uHDR",0); }
    glBindVertexArray(m_vao); glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0); glBindVertexArray(0);
    glDepthMask(GL_TRUE); glDepthFunc(DepthState::kFunc);
}

void Skybox::DrawProcedural(const glm::mat4& view, const glm::mat4& proj, float time) {
    static Skybox proc; // ленивый процедурный
    if (!proc.valid()) proc.createCube();
    if (!s_skyInit) { s_skyShader = new Shader(DefaultShaders::kSkyVS, DefaultShaders::kSkyFS); s_skyInit=true; }
    glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
    s_skyShader->use();
    glm::mat4 vNoTrans = glm::mat4(glm::mat3(view));
    s_skyShader->setMat4("uView", vNoTrans); s_skyShader->setMat4("uProj", proj);
    // HDR интенсивности
    s_skyShader->setVec3("uTopColor", glm::vec3(0.22f,0.45f,0.82f)*2.0f);
    s_skyShader->setVec3("uHorizonColor", glm::vec3(0.65f,0.78f,0.95f)*1.4f);
    s_skyShader->setVec3("uBottomColor", glm::vec3(0.85f,0.88f,0.92f));
    s_skyShader->setFloat("uTime", time);
    glBindVertexArray(proc.m_vao); glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0); glBindVertexArray(0);
    glDepthMask(GL_TRUE); glDepthFunc(DepthState::kFunc);
}
