#pragma once
#include <vector>
#include <glad/gl.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "engine/Renderer/Shader.h"

namespace Physics {

// ============================================================================
// Real-time Cloth & Cape Simulation (Position-Based Dynamics / Verlet)
// Сетка частиц ткани с фиксацией на плечах, инерцией, ветром и коллизией с телом
// ============================================================================

struct ClothParticle {
    glm::vec3 pos{0.0f};
    glm::vec3 prevPos{0.0f};
    glm::vec3 color{0.85f, 0.12f, 0.15f}; // благородный алый цвет ткани
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec2 uv{0.0f};
    bool isPinned = false;
    float mass = 0.1f;
};

struct ClothConstraint {
    int p1 = 0;
    int p2 = 0;
    float restLength = 0.0f;
};

class ClothCape {
public:
    ClothCape(int gridX = 9, int gridY = 14, float width = 0.65f, float length = 1.15f)
        : m_gridX(gridX), m_gridY(gridY), m_width(width), m_length(length) {
        initGrid();
        initGL();
    }

    ~ClothCape() {
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
        if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    }

    void update(float dt, const glm::vec3& pinL, const glm::vec3& pinR,
                const glm::vec3& bodyCenter, float bodyRadius, const glm::vec3& windForce) {
        if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

        // Инициализация в мировых координатах за спиной персонажа
        if (!m_initialized) {
            m_initialized = true;
            for (int y = 0; y < m_gridY; ++y) {
                float vFactor = (m_gridY > 1) ? static_cast<float>(y) / static_cast<float>(m_gridY - 1) : 0.0f;
                for (int x = 0; x < m_gridX; ++x) {
                    float uFactor = (m_gridX > 1) ? static_cast<float>(x) / static_cast<float>(m_gridX - 1) : 0.5f;
                    int idx = y * m_gridX + x;
                    glm::vec3 topPos = glm::mix(pinL, pinR, uFactor);
                    m_particles[idx].pos = topPos - glm::vec3(0.0f, vFactor * m_length, 0.0f);
                    m_particles[idx].prevPos = m_particles[idx].pos;
                }
            }
        }

        float damping = 0.96f;
        glm::vec3 gravity(0.0f, -9.81f, 0.0f);

        // 1. Интеграция Верле
        for (int y = 0; y < m_gridY; ++y) {
            for (int x = 0; x < m_gridX; ++x) {
                int idx = y * m_gridX + x;
                auto& p = m_particles[idx];

                if (p.isPinned) {
                    float factor = (m_gridX > 1) ? static_cast<float>(x) / static_cast<float>(m_gridX - 1) : 0.5f;
                    p.pos = glm::mix(pinL, pinR, factor);
                    p.prevPos = p.pos;
                    continue;
                }

                glm::vec3 vel = (p.pos - p.prevPos) * damping;
                p.prevPos = p.pos;

                // Случайный турбулентный ветер
                glm::vec3 wind = windForce + glm::vec3(
                    std::sin(p.pos.y * 3.0f + p.pos.z * 2.0f) * 0.4f,
                    std::cos(p.pos.x * 2.0f) * 0.2f,
                    std::sin(p.pos.x * 2.0f + p.pos.y * 3.0f) * 0.4f
                );

                glm::vec3 accel = gravity + wind / p.mass;
                p.pos += vel + accel * (dt * dt);
            }
        }

        // 2. Решение связей (Constraints) — несколько итераций для жесткости
        const int iterations = 6;
        for (int it = 0; it < iterations; ++it) {
            // Структурные связи
            for (const auto& c : m_constraints) {
                auto& p1 = m_particles[c.p1];
                auto& p2 = m_particles[c.p2];

                glm::vec3 delta = p2.pos - p1.pos;
                float dist = glm::length(delta);
                if (dist < 1e-6f) continue;

                float diff = (dist - c.restLength) / dist;
                glm::vec3 correction = delta * 0.5f * diff;

                if (!p1.isPinned && !p2.isPinned) {
                    p1.pos += correction;
                    p2.pos -= correction;
                } else if (!p1.isPinned) {
                    p1.pos += correction * 2.0f;
                } else if (!p2.isPinned) {
                    p2.pos -= correction * 2.0f;
                }
            }

            // Коллизия с эллипсоидом тела персонажа (спина и ноги)
            for (auto& p : m_particles) {
                if (p.isPinned) continue;

                // Капсула тела
                glm::vec3 toBody = p.pos - bodyCenter;
                toBody.y *= 0.5f; // вертикальное сжатие для капсулы
                float distSq = glm::dot(toBody, toBody);
                float rSq = bodyRadius * bodyRadius;
                if (distSq < rSq) {
                    float dist = std::sqrt(distSq);
                    if (dist > 1e-5f) {
                        glm::vec3 pushDir = toBody / dist;
                        pushDir.y *= 2.0f;
                        p.pos = bodyCenter + pushDir * bodyRadius;
                    }
                }

                // Коллизия с землей (y = 0.05)
                if (p.pos.y < 0.05f) {
                    p.pos.y = 0.05f;
                }
            }
        }

        // 3. Вычисление гладких нормалей
        recalculateNormals();

        // 4. Загрузка в OpenGL VBO
        updateGL();
    }

    void draw() const {
        if (m_vao && !m_indices.empty()) {
            glDisable(GL_CULL_FACE); // двусторонний плащ
            glBindVertexArray(m_vao);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }
    }

    const glm::vec3& color() const { return m_color; }
    void setColor(const glm::vec3& col) { m_color = col; }

private:
    void initGrid() {
        m_particles.resize(m_gridX * m_gridY);
        float stepX = m_width / static_cast<float>(m_gridX - 1);
        float stepY = m_length / static_cast<float>(m_gridY - 1);

        for (int y = 0; y < m_gridY; ++y) {
            for (int x = 0; x < m_gridX; ++x) {
                int idx = y * m_gridX + x;
                float px = (static_cast<float>(x) - static_cast<float>(m_gridX - 1) * 0.5f) * stepX;
                float py = -static_cast<float>(y) * stepY;
                float pz = -0.15f - static_cast<float>(y) * 0.02f; // сзади спины

                auto& p = m_particles[idx];
                p.pos = {px, py, pz};
                p.prevPos = p.pos;
                p.uv = {static_cast<float>(x) / static_cast<float>(m_gridX - 1),
                        static_cast<float>(y) / static_cast<float>(m_gridY - 1)};
                p.isPinned = (y == 0); // верхний ряд зафиксирован на плечах
            }
        }

        // Связи: структурные и диагональные
        m_constraints.clear();
        for (int y = 0; y < m_gridY; ++y) {
            for (int x = 0; x < m_gridX; ++x) {
                int idx = y * m_gridX + x;

                // По горизонтали
                if (x < m_gridX - 1) {
                    addConstraint(idx, idx + 1);
                }
                // По вертикали
                if (y < m_gridY - 1) {
                    addConstraint(idx, idx + m_gridX);
                }
                // По диагонали (для жесткости на сдвиг)
                if (x < m_gridX - 1 && y < m_gridY - 1) {
                    addConstraint(idx, idx + m_gridX + 1);
                    addConstraint(idx + 1, idx + m_gridX);
                }
            }
        }

        // Индексы треугольников
        m_indices.clear();
        for (int y = 0; y < m_gridY - 1; ++y) {
            for (int x = 0; x < m_gridX - 1; ++x) {
                int i0 = y * m_gridX + x;
                int i1 = i0 + 1;
                int i2 = i0 + m_gridX;
                int i3 = i2 + 1;

                m_indices.push_back(i0);
                m_indices.push_back(i2);
                m_indices.push_back(i1);

                m_indices.push_back(i1);
                m_indices.push_back(i2);
                m_indices.push_back(i3);
            }
        }
    }

    void addConstraint(int i1, int i2) {
        float len = glm::length(m_particles[i1].pos - m_particles[i2].pos);
        m_constraints.push_back({i1, i2, len});
    }

    void recalculateNormals() {
        for (auto& p : m_particles) p.normal = glm::vec3(0.0f);

        for (size_t i = 0; i < m_indices.size(); i += 3) {
            uint32_t i0 = m_indices[i];
            uint32_t i1 = m_indices[i + 1];
            uint32_t i2 = m_indices[i + 2];

            glm::vec3 v0 = m_particles[i0].pos;
            glm::vec3 v1 = m_particles[i1].pos;
            glm::vec3 v2 = m_particles[i2].pos;

            glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
            m_particles[i0].normal += n;
            m_particles[i1].normal += n;
            m_particles[i2].normal += n;
        }

        for (auto& p : m_particles) {
            if (glm::length(p.normal) > 1e-4f) {
                p.normal = glm::normalize(p.normal);
            } else {
                p.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            }
        }
    }

    void initGL() {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_particles.size() * sizeof(ClothParticle), nullptr, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(uint32_t), m_indices.data(), GL_STATIC_DRAW);

        // Position: location 0
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ClothParticle), (void*)offsetof(ClothParticle, pos));

        // Color: location 1
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ClothParticle), (void*)offsetof(ClothParticle, color));

        // Normal: location 2
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ClothParticle), (void*)offsetof(ClothParticle, normal));

        // UV: location 3
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(ClothParticle), (void*)offsetof(ClothParticle, uv));

        glBindVertexArray(0);
    }

    void updateGL() {
        if (!m_vbo) return;
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_particles.size() * sizeof(ClothParticle), m_particles.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    int m_gridX;
    int m_gridY;
    float m_width;
    float m_length;
    glm::vec3 m_color{0.82f, 0.15f, 0.18f}; // благородный алый/бордовый плащ

    std::vector<ClothParticle> m_particles;
    std::vector<ClothConstraint> m_constraints;
    std::vector<uint32_t> m_indices;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    bool m_initialized = false;
};

} // namespace Physics
