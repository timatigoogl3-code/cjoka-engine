#pragma once
#include <vector>
#include <cmath>
#include <glad/gl.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "engine/Renderer/Shader.h"

namespace Renderer {

struct WaterVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

class WaterSurface {
public:
    WaterSurface(float radius = 3.0f, int segments = 32) : m_radius(radius), m_segments(segments) {
        initMesh();
        initGL();
    }

    ~WaterSurface() {
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
        if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    }

    void update(float time) {
        // Вычисление волн Герстнера в реальном времени
        for (size_t i = 0; i < m_baseVertices.size(); ++i) {
            glm::vec3 base = m_baseVertices[i].pos;

            float w1 = std::sin(base.x * 2.5f + time * 3.0f) * 0.04f;
            float w2 = std::cos(base.z * 2.0f + time * 2.5f) * 0.035f;
            float w3 = std::sin((base.x + base.z) * 3.5f + time * 4.0f) * 0.02f;

            m_animVertices[i].pos = base + glm::vec3(0.0f, w1 + w2 + w3, 0.0f);

            // Аналитический расчет нормали
            float dx = 2.5f * std::cos(base.x * 2.5f + time * 3.0f) * 0.04f + 3.5f * std::cos((base.x + base.z) * 3.5f + time * 4.0f) * 0.02f;
            float dz = -2.0f * std::sin(base.z * 2.0f + time * 2.5f) * 0.035f + 3.5f * std::cos((base.x + base.z) * 3.5f + time * 4.0f) * 0.02f;

            m_animVertices[i].normal = glm::normalize(glm::vec3(-dx, 1.0f, -dz));
            m_animVertices[i].uv = m_baseVertices[i].uv;
        }

        // Обновление буфера на GPU
        if (m_vbo) {
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, m_animVertices.size() * sizeof(WaterVertex), m_animVertices.data());
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }

    void draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj,
              const glm::vec3& camPos, const glm::vec3& sunDir, const glm::vec3& sunColor) {
        if (!m_vao) return;

        ensureShader();
        m_shader->use();
        m_shader->setMat4("uModel", model);
        m_shader->setMat4("uView", view);
        m_shader->setMat4("uProj", proj);
        m_shader->setVec3("uCamPos", camPos);
        m_shader->setVec3("uSunDir", glm::normalize(-sunDir));
        m_shader->setVec3("uSunColor", sunColor);
        m_shader->setVec3("uWaterColor", glm::vec3(0.12f, 0.45f, 0.65f)); // Лазурный

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

private:
    void ensureShader() {
        if (!m_shader) {
            const char* vs = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
out vec3 vWorldPos; out vec3 vNormal; out vec2 vUV;
uniform mat4 uModel; uniform mat4 uView; uniform mat4 uProj;
void main(){
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vNormal = normalize(mat3(uModel) * aNormal);
    vUV = aUV;
    gl_Position = uProj * uView * wp;
}
)";
            const char* fs = R"(#version 460 core
in vec3 vWorldPos; in vec3 vNormal; in vec2 vUV;
out vec4 FragColor;
uniform vec3 uCamPos; uniform vec3 uSunDir; uniform vec3 uSunColor; uniform vec3 uWaterColor;
void main(){
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = normalize(uSunDir);
    vec3 H = normalize(V + L);

    // Fresnel
    float fresnel = 0.05 + 0.95 * pow(1.0 - max(dot(N, V), 0.0), 4.0);

    // Diffuse + Ambient
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = uWaterColor * (0.35 + 0.65 * diff * uSunColor);

    // Specular (Sun reflection)
    float spec = pow(max(dot(N, H), 0.0), 64.0);
    vec3 specular = uSunColor * spec * 1.8;

    vec3 skyColor = vec3(0.55, 0.75, 0.95);
    vec3 finalColor = mix(diffuse, skyColor, fresnel * 0.7) + specular;

    FragColor = vec4(finalColor, 0.88);
}
)";
            m_shader = std::make_unique<Shader>(vs, fs);
        }
    }

    void initMesh() {
        m_baseVertices.clear();
        m_indices.clear();

        // Центр диска
        m_baseVertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}});

        // Концентрические кольца для детальных волн
        const int rings = 6;
        for (int r = 1; r <= rings; ++r) {
            float rad = (static_cast<float>(r) / static_cast<float>(rings)) * m_radius;
            for (int s = 0; s < m_segments; ++s) {
                float angle = (static_cast<float>(s) / static_cast<float>(m_segments)) * 6.2831853f;
                float x = std::cos(angle) * rad;
                float z = std::sin(angle) * rad;
                float u = 0.5f + (x / m_radius) * 0.5f;
                float v = 0.5f + (z / m_radius) * 0.5f;

                m_baseVertices.push_back({{x, 0.0f, z}, {0.0f, 1.0f, 0.0f}, {u, v}});
            }
        }

        // Индексы первого кольца
        for (int s = 0; s < m_segments; ++s) {
            int next = (s + 1) % m_segments;
            m_indices.push_back(0);
            m_indices.push_back(1 + s);
            m_indices.push_back(1 + next);
        }

        // Индексы остальных колец
        for (int r = 1; r < rings; ++r) {
            int rStart = 1 + (r - 1) * m_segments;
            int nextRStart = 1 + r * m_segments;
            for (int s = 0; s < m_segments; ++s) {
                int next = (s + 1) % m_segments;

                int i0 = rStart + s;
                int i1 = nextRStart + s;
                int i2 = rStart + next;
                int i3 = nextRStart + next;

                m_indices.push_back(i0);
                m_indices.push_back(i1);
                m_indices.push_back(i2);

                m_indices.push_back(i2);
                m_indices.push_back(i1);
                m_indices.push_back(i3);
            }
        }

        m_animVertices = m_baseVertices;
    }

    void initGL() {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_animVertices.size() * sizeof(WaterVertex), m_animVertices.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(uint32_t), m_indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WaterVertex), (void*)offsetof(WaterVertex, pos));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(WaterVertex), (void*)offsetof(WaterVertex, normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(WaterVertex), (void*)offsetof(WaterVertex, uv));

        glBindVertexArray(0);
    }

    float m_radius;
    int m_segments;
    std::vector<WaterVertex> m_baseVertices;
    std::vector<WaterVertex> m_animVertices;
    std::vector<uint32_t> m_indices;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    std::unique_ptr<Shader> m_shader;
};

} // namespace Renderer
