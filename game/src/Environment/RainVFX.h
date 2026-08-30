#pragma once
#include <glm/glm.hpp>
#include <glad/gl.h>
#include <vector>
#include <random>
#include <algorithm>
#include "engine/Renderer/Shader.h"
#include "engine/Environment/WorldEnvironment.h"
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"

namespace game {

struct RainDrop {
    glm::vec3 pos;
    glm::vec3 vel;
    float length;
    float alpha;
    float thickness;
};

struct RainSplash {
    glm::vec3 pos;
    float radius;
    float maxRadius;
    float alpha;
    float lifetime;
    float maxLifetime;
};

struct SurfaceObstacle {
    float minX, maxX;
    float minZ, maxZ;
    float topY;
    float bottomY;
};

// ------------------------------------------------------------------
// RainVFX — игровой VFX-компонент/система для рендера дождя и всплесков.
// Читает precipitation и wind из шины WorldEnvironment движка.
// Осуществляет проверку крыш (roof occlusion) и динамическую высоту всплесков.
// ------------------------------------------------------------------
class RainVFX {
public:
    static RainVFX& Get() {
        static RainVFX s_instance;
        return s_instance;
    }

    RainVFX(size_t dropCount = 2500, size_t splashCount = 300) {
        initRain(dropCount);
        initSplashes(splashCount);
    }

    ~RainVFX() {
        if (m_rainVAO) { glDeleteVertexArrays(1, &m_rainVAO); m_rainVAO = 0; }
        if (m_rainVBO) { glDeleteBuffers(1, &m_rainVBO); m_rainVBO = 0; }
        if (m_splashVAO) { glDeleteVertexArrays(1, &m_splashVAO); m_splashVAO = 0; }
        if (m_splashVBO) { glDeleteBuffers(1, &m_splashVBO); m_splashVBO = 0; }
        if (m_rainShader) { delete m_rainShader; m_rainShader = nullptr; }
        if (m_splashShader) { delete m_splashShader; m_splashShader = nullptr; }
    }

    void update(float dt, const glm::vec3& camPos, Registry* reg = nullptr) {
        auto& env = cjoka::WorldEnvironment::Get();
        if (env.precipitation <= 0.01f) return;

        std::vector<SurfaceObstacle> obstacles;
        float defaultGroundY = 0.0f;

        if (reg) {
            for (auto e : reg->view<Transform, MeshRenderer>()) {
                auto& mr = reg->get<MeshRenderer>(e);
                if (!mr.visible) continue;
                auto& tr = reg->get<Transform>(e);

                if (std::abs(tr.position.x - camPos.x) > 60.0f || std::abs(tr.position.z - camPos.z) > 60.0f) {
                    continue;
                }

                SurfaceObstacle obs{};
                if (mr.assetPath == "primitive:plane") {
                    float halfX = 5.0f * tr.scale.x;
                    float halfZ = 5.0f * tr.scale.z;
                    obs.minX = tr.position.x - halfX;
                    obs.maxX = tr.position.x + halfX;
                    obs.minZ = tr.position.z - halfZ;
                    obs.maxZ = tr.position.z + halfZ;
                    obs.topY = tr.position.y;
                    obs.bottomY = tr.position.y - 0.2f;
                    obstacles.push_back(obs);
                    defaultGroundY = std::max(defaultGroundY, tr.position.y);
                } else if (mr.assetPath == "primitive:cube") {
                    float halfX = 0.5f * tr.scale.x;
                    float halfY = 0.5f * tr.scale.y;
                    float halfZ = 0.5f * tr.scale.z;
                    obs.minX = tr.position.x - halfX;
                    obs.maxX = tr.position.x + halfX;
                    obs.minZ = tr.position.z - halfZ;
                    obs.maxZ = tr.position.z + halfZ;
                    obs.topY = tr.position.y + halfY;
                    obs.bottomY = tr.position.y - halfY;
                    obstacles.push_back(obs);
                } else if (mr.mesh && !mr.mesh->empty()) {
                    glm::vec3 minE = mr.mesh->minExtents() * tr.scale;
                    glm::vec3 maxE = mr.mesh->maxExtents() * tr.scale;
                    obs.minX = tr.position.x + std::min(minE.x, maxE.x);
                    obs.maxX = tr.position.x + std::max(minE.x, maxE.x);
                    obs.minZ = tr.position.z + std::min(minE.z, maxE.z);
                    obs.maxZ = tr.position.z + std::max(minE.z, maxE.z);
                    obs.topY = tr.position.y + std::max(minE.y, maxE.y);
                    obs.bottomY = tr.position.y + std::min(minE.y, maxE.y);
                    obstacles.push_back(obs);
                }
            }
        }

        auto getStopHeight = [&](float x, float z, float currentY) -> float {
            float stopY = defaultGroundY;
            for (const auto& obs : obstacles) {
                if (x >= obs.minX && x <= obs.maxX && z >= obs.minZ && z <= obs.maxZ) {
                    if (obs.topY > stopY && obs.topY <= currentY + 1.5f) {
                        stopY = obs.topY;
                    }
                }
            }
            return stopY;
        };

        auto isUnderRoof = [&](float x, float z, float y) -> bool {
            for (const auto& obs : obstacles) {
                if (x >= obs.minX && x <= obs.maxX && z >= obs.minZ && z <= obs.maxZ) {
                    if (obs.topY > y + 0.3f) {
                        return true;
                    }
                }
            }
            return false;
        };

        float speed = 32.0f;
        glm::vec3 vel = glm::vec3(env.wind.x * 5.0f, -speed, env.wind.z * 5.0f);

        for (auto& drop : m_drops) {
            drop.pos += vel * dt;
            drop.vel = vel;

            // 1. Occlusion under roof or ceiling: no rain drops allowed under roofs
            if (isUnderRoof(drop.pos.x, drop.pos.z, drop.pos.y)) {
                drop.pos.y = camPos.y + 25.0f + static_cast<float>(rand() % 100) / 10.0f;
                drop.pos.x = camPos.x + (static_cast<float>(rand() % 600) / 10.0f - 30.0f);
                drop.pos.z = camPos.z + (static_cast<float>(rand() % 600) / 10.0f - 30.0f);
                continue;
            }

            // 2. Dynamic surface impact splash (exact height where drops actually hit)
            float stopY = getStopHeight(drop.pos.x, drop.pos.z, drop.pos.y);
            if (drop.pos.y <= stopY + 0.05f) {
                if (rand() % 10 < 4) {
                    spawnSplash(glm::vec3(drop.pos.x, stopY + 0.01f, drop.pos.z));
                }
                drop.pos.y = camPos.y + 25.0f + static_cast<float>(rand() % 100) / 10.0f;
                drop.pos.x = camPos.x + (static_cast<float>(rand() % 600) / 10.0f - 30.0f);
                drop.pos.z = camPos.z + (static_cast<float>(rand() % 600) / 10.0f - 30.0f);
            }

            if (drop.pos.y < camPos.y - 12.0f) {
                drop.pos.y = camPos.y + 25.0f + static_cast<float>(rand() % 100) / 10.0f;
                drop.pos.x = camPos.x + (static_cast<float>(rand() % 600) / 10.0f - 30.0f);
                drop.pos.z = camPos.z + (static_cast<float>(rand() % 600) / 10.0f - 30.0f);
            }
            if (std::abs(drop.pos.x - camPos.x) > 35.0f) {
                drop.pos.x = camPos.x + (static_cast<float>(rand() % 600) / 10.0f - 30.0f);
            }
            if (std::abs(drop.pos.z - camPos.z) > 35.0f) {
                drop.pos.z = camPos.z + (static_cast<float>(rand() % 600) / 10.0f - 30.0f);
            }
        }

        // Update ground splash ripples
        for (auto& sp : m_splashes) {
            if (sp.alpha <= 0.01f) continue;
            sp.lifetime += dt;
            float progress = sp.lifetime / sp.maxLifetime;
            if (progress >= 1.0f) {
                sp.alpha = 0.0f;
            } else {
                sp.radius = glm::mix(0.05f, sp.maxRadius, progress);
                sp.alpha = (1.0f - progress) * 0.7f;
            }
        }
    }

    void render(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos) {
        auto& env = cjoka::WorldEnvironment::Get();
        if (env.precipitation <= 0.01f || m_drops.empty()) return;
        ensureShaders();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        // 1. Rain Streaks
        m_rainShader->use();
        m_rainShader->setMat4("uView", view);
        m_rainShader->setMat4("uProj", proj);
        m_rainShader->setVec3("uCamPos", camPos);
        m_rainShader->setFloat("uIntensity", env.precipitation);

        std::vector<float> vdata;
        vdata.reserve(m_drops.size() * 8);
        for (const auto& drop : m_drops) {
            glm::vec3 top = drop.pos;
            glm::vec3 bot = drop.pos + glm::normalize(drop.vel) * drop.length;
            vdata.push_back(top.x); vdata.push_back(top.y); vdata.push_back(top.z);
            vdata.push_back(drop.alpha * env.precipitation);
            vdata.push_back(bot.x); vdata.push_back(bot.y); vdata.push_back(bot.z);
            vdata.push_back(0.0f);
        }

        glBindVertexArray(m_rainVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_rainVBO);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vdata.size() * sizeof(float)), vdata.data(), GL_DYNAMIC_DRAW);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vdata.size() / 4));

        // 2. Ground Splash Ripples
        std::vector<float> sdata;
        sdata.reserve(m_splashes.size() * 16 * 4);
        for (const auto& sp : m_splashes) {
            if (sp.alpha <= 0.01f) continue;
            const int segments = 8;
            for (int i = 0; i < segments; ++i) {
                float a0 = (static_cast<float>(i) / segments) * 6.283185f;
                float a1 = (static_cast<float>(i + 1) / segments) * 6.283185f;
                glm::vec3 p0 = sp.pos + glm::vec3(std::cos(a0) * sp.radius, 0.02f, std::sin(a0) * sp.radius);
                glm::vec3 p1 = sp.pos + glm::vec3(std::cos(a1) * sp.radius, 0.02f, std::sin(a1) * sp.radius);
                sdata.push_back(p0.x); sdata.push_back(p0.y); sdata.push_back(p0.z); sdata.push_back(sp.alpha * env.precipitation);
                sdata.push_back(p1.x); sdata.push_back(p1.y); sdata.push_back(p1.z); sdata.push_back(sp.alpha * env.precipitation);
            }
        }

        if (!sdata.empty()) {
            m_splashShader->use();
            m_splashShader->setMat4("uView", view);
            m_splashShader->setMat4("uProj", proj);
            m_splashShader->setFloat("uIntensity", env.precipitation);

            glBindVertexArray(m_splashVAO);
            glBindBuffer(GL_ARRAY_BUFFER, m_splashVBO);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sdata.size() * sizeof(float)), sdata.data(), GL_DYNAMIC_DRAW);
            glLineWidth(1.5f);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(sdata.size() / 4));
        }

        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

private:
    void spawnSplash(const glm::vec3& pos) {
        m_splashIndex = (m_splashIndex + 1) % m_splashes.size();
        auto& sp = m_splashes[m_splashIndex];
        sp.pos = pos;
        sp.radius = 0.05f;
        sp.maxRadius = 0.25f + static_cast<float>(rand() % 100) / 500.0f;
        sp.alpha = 0.65f;
        sp.lifetime = 0.0f;
        sp.maxLifetime = 0.18f + static_cast<float>(rand() % 100) / 800.0f;
    }

    void initRain(size_t count) {
        m_drops.resize(count);
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> distBox(-30.0f, 30.0f);
        std::uniform_real_distribution<float> distY(0.0f, 35.0f);
        std::uniform_real_distribution<float> distLen(0.8f, 1.8f);

        for (auto& drop : m_drops) {
            drop.pos = glm::vec3(distBox(rng), distY(rng), distBox(rng));
            drop.vel = glm::vec3(-1.5f, -28.0f, 0.6f);
            drop.length = distLen(rng);
            drop.alpha = 0.55f + static_cast<float>(rand() % 100) / 250.0f;
            drop.thickness = 1.0f + static_cast<float>(rand() % 100) / 100.0f;
        }

        glGenVertexArrays(1, &m_rainVAO);
        glGenBuffers(1, &m_rainVBO);
        glBindVertexArray(m_rainVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_rainVBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindVertexArray(0);
    }

    void initSplashes(size_t count) {
        m_splashes.resize(count);
        for (auto& sp : m_splashes) {
            sp.pos = glm::vec3(0.0f);
            sp.radius = 0.05f;
            sp.maxRadius = 0.4f;
            sp.alpha = 0.0f;
            sp.lifetime = 0.0f;
            sp.maxLifetime = 0.25f;
        }

        glGenVertexArrays(1, &m_splashVAO);
        glGenBuffers(1, &m_splashVBO);
        glBindVertexArray(m_splashVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_splashVBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindVertexArray(0);
    }

    void ensureShaders() {
        if (!m_rainShader) {
            const char* vs = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in float aAlpha;
uniform mat4 uView;
uniform mat4 uProj;
out float vAlpha;
void main(){
    vAlpha = aAlpha;
    gl_Position = uProj * uView * vec4(aPos, 1.0);
}
)";
            const char* fs = R"(#version 460 core
in float vAlpha;
uniform float uIntensity;
out vec4 FragColor;
void main(){
    FragColor = vec4(0.80, 0.90, 1.0, vAlpha * uIntensity * 0.75);
}
)";
            m_rainShader = new Shader(vs, fs);
        }

        if (!m_splashShader) {
            const char* vs = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in float aAlpha;
uniform mat4 uView;
uniform mat4 uProj;
out float vAlpha;
void main(){
    vAlpha = aAlpha;
    gl_Position = uProj * uView * vec4(aPos, 1.0);
}
)";
            const char* fs = R"(#version 460 core
in float vAlpha;
uniform float uIntensity;
out vec4 FragColor;
void main(){
    FragColor = vec4(0.75, 0.88, 1.0, vAlpha * uIntensity * 0.65);
}
)";
            m_splashShader = new Shader(vs, fs);
        }
    }

    std::vector<RainDrop> m_drops;
    std::vector<RainSplash> m_splashes;
    size_t m_splashIndex = 0;

    GLuint m_rainVAO = 0;
    GLuint m_rainVBO = 0;
    GLuint m_splashVAO = 0;
    GLuint m_splashVBO = 0;

    Shader* m_rainShader = nullptr;
    Shader* m_splashShader = nullptr;
};

} // namespace game
