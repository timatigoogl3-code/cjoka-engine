#pragma once
#include <vector>
#include <random>
#include <glad/gl.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "engine/Renderer/Shader.h"

namespace ParticleFX {

enum class ParticleType {
    Fire,
    Sparks,
    Smoke,
    WaterSplash
};

struct Particle {
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    glm::vec4 color{1.0f};
    float size = 0.1f;
    float life = 0.0f;
    float maxLife = 1.0f;
    ParticleType type = ParticleType::Fire;
};

// GPU Vertex for Instanced Particles
struct ParticleVertex {
    glm::vec3 pos;
    glm::vec4 color;
    float size;
};

class ParticleSystem {
public:
    ParticleSystem(size_t maxParticles = 3000) : m_maxParticles(maxParticles) {
        m_particles.reserve(maxParticles);
        m_gpuData.resize(maxParticles);
        initGL();
    }

    ~ParticleSystem() {
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    }

    // Эмиссия огня и искр
    void emitFire(const glm::vec3& origin, int count = 2, float spread = 0.10f) {
        for (int i = 0; i < count && m_particles.size() < m_maxParticles; ++i) {
            Particle p;
            p.type = ParticleType::Fire;
            p.pos = origin + glm::vec3(randFloat(-spread, spread), randFloat(0.0f, 0.04f), randFloat(-spread, spread));
            p.vel = glm::vec3(randFloat(-0.08f, 0.08f), randFloat(0.5f, 1.1f), randFloat(-0.08f, 0.08f));
            p.size = randFloat(0.10f, 0.18f);
            p.maxLife = randFloat(0.35f, 0.65f);
            p.life = 0.0f;
            p.color = glm::vec4(1.0f, 0.60f, 0.15f, 0.75f); // мягкий янтарно-золотой
            m_particles.push_back(p);
        }

        // Искры
        if (randFloat(0.0f, 1.0f) < 0.35f && m_particles.size() < m_maxParticles) {
            Particle p;
            p.type = ParticleType::Sparks;
            p.pos = origin + glm::vec3(randFloat(-spread*0.5f, spread*0.5f), 0.1f, randFloat(-spread*0.5f, spread*0.5f));
            p.vel = glm::vec3(randFloat(-0.4f, 0.4f), randFloat(1.5f, 2.8f), randFloat(-0.4f, 0.4f));
            p.size = randFloat(0.03f, 0.06f);
            p.maxLife = randFloat(0.4f, 0.9f);
            p.life = 0.0f;
            p.color = glm::vec4(1.0f, 0.85f, 0.4f, 0.9f);
            m_particles.push_back(p);
        }

        // Дым
        if (randFloat(0.0f, 1.0f) < 0.25f && m_particles.size() < m_maxParticles) {
            Particle p;
            p.type = ParticleType::Smoke;
            p.pos = origin + glm::vec3(randFloat(-0.1f, 0.1f), 0.4f, randFloat(-0.1f, 0.1f));
            p.vel = glm::vec3(randFloat(-0.15f, 0.15f), randFloat(0.8f, 1.4f), randFloat(-0.15f, 0.15f));
            p.size = randFloat(0.25f, 0.55f);
            p.maxLife = randFloat(1.2f, 2.2f);
            p.life = 0.0f;
            p.color = glm::vec4(0.25f, 0.22f, 0.20f, 0.6f);
            m_particles.push_back(p);
        }
    }

    // Эмиссия брызг воды
    void emitWater(const glm::vec3& origin, int count = 5, float spread = 0.2f, float power = 3.5f) {
        for (int i = 0; i < count && m_particles.size() < m_maxParticles; ++i) {
            Particle p;
            p.type = ParticleType::WaterSplash;
            p.pos = origin + glm::vec3(randFloat(-spread, spread), 0.0f, randFloat(-spread, spread));
            p.vel = glm::vec3(randFloat(-0.6f, 0.6f), randFloat(power * 0.7f, power), randFloat(-0.6f, 0.6f));
            p.size = randFloat(0.06f, 0.14f);
            p.maxLife = randFloat(0.8f, 1.5f);
            p.life = 0.0f;
            p.color = glm::vec4(0.65f, 0.85f, 1.0f, 0.8f);
            m_particles.push_back(p);
        }
    }

    void update(float dt) {
        if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

        size_t aliveCount = 0;
        for (size_t i = 0; i < m_particles.size(); ++i) {
            auto& p = m_particles[i];
            p.life += dt;
            if (p.life >= p.maxLife) continue;

            float progress = p.life / p.maxLife;

            if (p.type == ParticleType::Fire) {
                p.pos += p.vel * dt;
                p.size *= (1.0f + 0.4f * dt);
                // Плавный переход цвета: Желтый -> Оранжевый -> Темно-красный -> Прозрачный
                p.color.r = 1.0f;
                p.color.g = glm::mix(0.85f, 0.05f, progress);
                p.color.b = glm::mix(0.2f, 0.0f, progress);
                p.color.a = (1.0f - progress * progress);
            } else if (p.type == ParticleType::Sparks) {
                p.vel += glm::vec3(0.0f, -6.0f, 0.0f) * dt; // гравитация на искры
                p.pos += p.vel * dt;
                p.color.a = 1.0f - progress;
            } else if (p.type == ParticleType::Smoke) {
                p.pos += p.vel * dt;
                p.size += 0.25f * dt;
                p.color.a = glm::mix(0.5f, 0.0f, progress);
            } else if (p.type == ParticleType::WaterSplash) {
                p.vel += glm::vec3(0.0f, -9.81f, 0.0f) * dt; // гравитация воды
                p.pos += p.vel * dt;
                p.color.a = 1.0f - progress * 0.8f;
            }

            m_particles[aliveCount] = p;
            m_gpuData[aliveCount] = {p.pos, p.color, p.size};
            aliveCount++;
        }
        m_particles.resize(aliveCount);

        // Обновляем VBO
        if (aliveCount > 0 && m_vbo) {
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(aliveCount * sizeof(ParticleVertex)), m_gpuData.data());
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }

    void draw(const glm::mat4& view, const glm::mat4& proj) {
        if (m_particles.empty() || !m_vao) return;

        ensureShader();
        m_shader->use();
        m_shader->setMat4("uView", view);
        m_shader->setMat4("uProj", proj);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Аддитивное сияние
        glDepthMask(GL_FALSE);             // Не пишем в Z-буфер, чтобы частицы сияли сквозь друг друга
        glEnable(GL_PROGRAM_POINT_SIZE);

        glBindVertexArray(m_vao);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_particles.size()));
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
    }

    size_t count() const { return m_particles.size(); }

private:
    float randFloat(float minVal, float maxVal) {
        static std::mt19937 gen(1337);
        std::uniform_real_distribution<float> dis(minVal, maxVal);
        return dis(gen);
    }

    void ensureShader() {
        if (!m_shader) {
            const char* vs = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
layout(location=2) in float aSize;
out vec4 vColor;
uniform mat4 uView; uniform mat4 uProj;
void main(){
    vColor = aColor;
    vec4 viewPos = uView * vec4(aPos, 1.0);
    gl_Position = uProj * viewPos;
    // Перспективный размер точки
    gl_PointSize = clamp((aSize * 750.0) / -viewPos.z, 2.0, 180.0);
}
)";
            const char* fs = R"(#version 460 core
in vec4 vColor; out vec4 FragColor;
void main(){
    vec2 coord = gl_PointCoord - vec2(0.5);
    float distSq = dot(coord, coord);
    if(distSq > 0.25) discard;
    float alpha = clamp(1.0 - distSq * 4.0, 0.0, 1.0);
    FragColor = vec4(vColor.rgb * (1.2 + alpha * 0.8), vColor.a * alpha);
}
)";
            m_shader = std::make_unique<Shader>(vs, fs);
        }
    }

    void initGL() {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_maxParticles * sizeof(ParticleVertex)), nullptr, GL_DYNAMIC_DRAW);

        // Position: location 0
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, pos));

        // Color: location 1
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, color));

        // Size: location 2
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, size));

        glBindVertexArray(0);
    }

    size_t m_maxParticles;
    std::vector<Particle> m_particles;
    std::vector<ParticleVertex> m_gpuData;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    std::unique_ptr<Shader> m_shader;
};

} // namespace ParticleFX
