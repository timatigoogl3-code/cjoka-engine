#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"

namespace cjoka {

// 2nd-order (9 coefficients) Spherical Harmonics Light Probe
struct LightProbe {
    glm::vec3 pos{0.0f};
    glm::vec3 sh[9] = { glm::vec3(0.0f) }; // L0, L1, L2 bands
};

class LightProbeGrid {
public:
    static LightProbeGrid& Get() {
        static LightProbeGrid s_instance;
        return s_instance;
    }

    LightProbeGrid(const glm::vec3& minB = glm::vec3(-40.0f, -2.0f, -40.0f),
                   const glm::vec3& maxB = glm::vec3( 40.0f, 15.0f,  40.0f),
                   const glm::ivec3& counts = glm::ivec3(8, 4, 8)) {
        initGrid(minB, maxB, counts);
    }

    ~LightProbeGrid() {
        if (m_probeSSBO) { glDeleteBuffers(1, &m_probeSSBO); m_probeSSBO = 0; }
    }

    void initGrid(const glm::vec3& minB, const glm::vec3& maxB, const glm::ivec3& counts) {
        m_minBounds = minB;
        m_maxBounds = maxB;
        m_counts = counts;
        m_probes.clear();
        m_probes.resize(static_cast<size_t>(m_counts.x * m_counts.y * m_counts.z));

        glm::vec3 step = (m_maxBounds - m_minBounds) / glm::vec3(m_counts - glm::ivec3(1));
        for (int z = 0; z < m_counts.z; ++z) {
            for (int y = 0; y < m_counts.y; ++y) {
                for (int x = 0; x < m_counts.x; ++x) {
                    size_t idx = getIndex(x, y, z);
                    m_probes[idx].pos = m_minBounds + glm::vec3(x, y, z) * step;
                    // Default ambient clear sky SH
                    m_probes[idx].sh[0] = glm::vec3(0.35f, 0.40f, 0.48f); // L0 (ambient base)
                    m_probes[idx].sh[2] = glm::vec3(0.12f, 0.15f, 0.20f); // L1 Y (sky vs ground)
                }
            }
        }
        updateSSBO();
        std::cout << "[LightProbeGrid] Initialized " << m_probes.size() << " probes ("
                  << m_counts.x << "x" << m_counts.y << "x" << m_counts.z << ")\n";
    }

    void bake(Registry& reg, const DirectionalLight* sun = nullptr) {
        std::cout << "[LightProbeGrid] Baking " << m_probes.size() << " light probes...\n";
        glm::vec3 sunDir = sun ? normalize(-sun->direction) : glm::vec3(0.4f, 0.8f, 0.2f);
        glm::vec3 sunCol = sun ? (sun->color * sun->intensity) : glm::vec3(3.0f);

        for (auto& probe : m_probes) {
            // Direct sun contribution
            float sunVisibility = 1.0f;
            // Check if probe is under terrain/mesh
            for (Entity e : reg.view<Transform, MeshRenderer>()) {
                auto& tr = reg.get<Transform>(e);
                if (tr.position.y > probe.pos.y && glm::distance(glm::vec2(tr.position.x, tr.position.z), glm::vec2(probe.pos.x, probe.pos.z)) < 2.0f) {
                    sunVisibility = 0.25f;
                    break;
                }
            }

            // Approximate SH irradiance fitting
            probe.sh[0] = glm::vec3(0.25f, 0.28f, 0.35f) + sunCol * sunVisibility * 0.28f;
            probe.sh[1] = sunCol * sunDir.y * sunVisibility * 0.18f;
            probe.sh[2] = sunCol * sunDir.z * sunVisibility * 0.18f;
            probe.sh[3] = sunCol * sunDir.x * sunVisibility * 0.18f;
        }

        updateSSBO();
        std::cout << "[LightProbeGrid] Baking completed successfully.\n";
    }

    glm::vec3 sampleIrradiance(const glm::vec3& worldPos, const glm::vec3& N) const {
        if (m_probes.empty()) return glm::vec3(0.2f);

        glm::vec3 local = (worldPos - m_minBounds) / (m_maxBounds - m_minBounds);
        local = glm::clamp(local, glm::vec3(0.0f), glm::vec3(1.0f));

        glm::vec3 fIndex = local * glm::vec3(m_counts - glm::ivec3(1));
        glm::ivec3 i0 = glm::clamp(glm::ivec3(fIndex), glm::ivec3(0), m_counts - glm::ivec3(2));
        glm::ivec3 i1 = i0 + glm::ivec3(1);
        glm::vec3 t = glm::fract(fIndex);

        // Trilinear interpolation of 8 probe SH
        glm::vec3 sh0 = glm::mix(m_probes[getIndex(i0.x, i0.y, i0.z)].sh[0], m_probes[getIndex(i1.x, i0.y, i0.z)].sh[0], t.x);
        glm::vec3 sh1 = glm::mix(m_probes[getIndex(i0.x, i1.y, i0.z)].sh[0], m_probes[getIndex(i1.x, i1.y, i0.z)].sh[0], t.x);
        glm::vec3 shY0 = glm::mix(sh0, sh1, t.y);

        glm::vec3 sh2 = glm::mix(m_probes[getIndex(i0.x, i0.y, i1.z)].sh[0], m_probes[getIndex(i1.x, i0.y, i1.z)].sh[0], t.x);
        glm::vec3 sh3 = glm::mix(m_probes[getIndex(i0.x, i1.y, i1.z)].sh[0], m_probes[getIndex(i1.x, i1.y, i1.z)].sh[0], t.x);
        glm::vec3 shY1 = glm::mix(sh2, sh3, t.y);

        glm::vec3 finalL0 = glm::mix(shY0, shY1, t.z);
        return finalL0 * (0.5f + 0.5f * N.y);
    }

    struct alignas(16) GPULightProbe {
        glm::vec4 pos{0.0f};
        glm::vec4 sh[9] = { glm::vec4(0.0f) };
    };

    struct alignas(16) GPUProbeGridHeader {
        glm::vec4 minBounds{0.0f};
        glm::vec4 maxBounds{0.0f};
        glm::ivec4 counts{0};
    };

    void updateSSBO() {
        if (!m_probeSSBO) {
            glGenBuffers(1, &m_probeSSBO);
        }

        GPUProbeGridHeader header;
        header.minBounds = glm::vec4(m_minBounds, 0.0f);
        header.maxBounds = glm::vec4(m_maxBounds, 0.0f);
        header.counts = glm::ivec4(m_counts.x, m_counts.y, m_counts.z, static_cast<int>(m_probes.size()));

        std::vector<GPULightProbe> gpuProbes(m_probes.size());
        for (size_t i = 0; i < m_probes.size(); ++i) {
            gpuProbes[i].pos = glm::vec4(m_probes[i].pos, 1.0f);
            for (int k = 0; k < 9; ++k) {
                gpuProbes[i].sh[k] = glm::vec4(m_probes[i].sh[k], 1.0f);
            }
        }

        size_t totalBytes = sizeof(GPUProbeGridHeader) + gpuProbes.size() * sizeof(GPULightProbe);
        std::vector<uint8_t> buffer(totalBytes);
        std::memcpy(buffer.data(), &header, sizeof(GPUProbeGridHeader));
        std::memcpy(buffer.data() + sizeof(GPUProbeGridHeader), gpuProbes.data(), gpuProbes.size() * sizeof(GPULightProbe));

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_probeSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(totalBytes), buffer.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    void bindSSBO(int slot = 6) const {
        if (m_probeSSBO) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(slot), m_probeSSBO);
        }
    }

    bool enabled() const { return m_enabled; }
    void setEnabled(bool e) { m_enabled = e; }
    float intensity() const { return m_intensity; }
    void setIntensity(float i) { m_intensity = i; }

    const glm::vec3& minBounds() const { return m_minBounds; }
    const glm::vec3& maxBounds() const { return m_maxBounds; }
    const glm::ivec3& counts() const { return m_counts; }
    const std::vector<LightProbe>& probes() const { return m_probes; }

    bool saveToFile(const std::string& path) const {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(&m_minBounds), sizeof(glm::vec3));
        out.write(reinterpret_cast<const char*>(&m_maxBounds), sizeof(glm::vec3));
        out.write(reinterpret_cast<const char*>(&m_counts), sizeof(glm::ivec3));
        size_t num = m_probes.size();
        out.write(reinterpret_cast<const char*>(&num), sizeof(size_t));
        out.write(reinterpret_cast<const char*>(m_probes.data()), static_cast<std::streamsize>(num * sizeof(LightProbe)));
        out.close();
        return true;
    }

    bool loadFromFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;
        in.read(reinterpret_cast<char*>(&m_minBounds), sizeof(glm::vec3));
        in.read(reinterpret_cast<char*>(&m_maxBounds), sizeof(glm::vec3));
        in.read(reinterpret_cast<char*>(&m_counts), sizeof(glm::ivec3));
        size_t num = 0;
        in.read(reinterpret_cast<char*>(&num), sizeof(size_t));
        m_probes.resize(num);
        in.read(reinterpret_cast<char*>(m_probes.data()), static_cast<std::streamsize>(num * sizeof(LightProbe)));
        in.close();
        updateSSBO();
        return true;
    }

private:
    size_t getIndex(int x, int y, int z) const {
        return static_cast<size_t>(z * m_counts.x * m_counts.y + y * m_counts.x + x);
    }

    glm::vec3 m_minBounds{-40.0f, -2.0f, -40.0f};
    glm::vec3 m_maxBounds{ 40.0f, 15.0f,  40.0f};
    glm::ivec3 m_counts{8, 4, 8};
    std::vector<LightProbe> m_probes;
    GLuint m_probeSSBO = 0;
    bool m_enabled = true;
    float m_intensity = 1.0f;
};

} // namespace cjoka
