#include "engine/Renderer/Batcher.h"
#include "engine/ECS/Registry.h"
#include "engine/Renderer/DefaultShaders.h"
#include "engine/Renderer/Shader.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_inverse.hpp>

void Batcher::begin() { m_batches.clear(); }

void Batcher::submit(const Transform& tr, const MeshRenderer& mr) {
    if (!mr.mesh || mr.mesh->empty() || !mr.visible) return;
    GLuint tex = 0;
    if (mr.material.useDiffuseMap && mr.material.diffuseMap && mr.material.diffuseMap->valid())
        tex = mr.material.diffuseMap->id();
    Key k{ mr.mesh.get(), tex };
    auto it = m_batches.find(k);
    if (it == m_batches.end()) {
        Batch b;
        b.mesh = mr.mesh.get();
        b.material = mr.material;
        auto [newIt, _] = m_batches.emplace(k, std::move(b));
        it = newIt;
    }
    InstanceData inst;
    inst.model = tr.matrix();
    inst.albedo = glm::vec4(mr.material.albedo, mr.material.shininess);
    inst.emissive = glm::vec4(mr.material.emissive, 1.0f);
    it->second.instances.push_back(inst);
}

size_t Batcher::totalInstances() const {
    size_t n = 0;
    for (auto& [k, b] : m_batches) n += b.instances.size();
    return n;
}

void Batcher::flush(Registry& reg, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos) {
    if (m_batches.empty()) return;
    static Shader* instanced = nullptr;
    if (!instanced) instanced = new Shader(DefaultShaders::kLitInstancedVS, DefaultShaders::kLitInstancedFS);

    instanced->use();
    instanced->setMat4("uView", view);
    instanced->setMat4("uProj", proj);
    instanced->setVec3("uViewPos", viewPos);

    // Fog / Sky
    glm::vec3 fogCol{0.12f, 0.14f, 0.18f};
    float fogDen = 0.025f;
    float exposure = 1.0f;
    if (!reg.view<Fog>().empty()) {
        auto& f = reg.get<Fog>(reg.view<Fog>()[0]);
        fogCol = f.color; fogDen = f.density;
    }
    if (!reg.view<Sky>().empty()) {
        auto& s = reg.get<Sky>(reg.view<Sky>()[0]);
        exposure = s.exposure;
    }
    instanced->setVec3("uFogColor", fogCol);
    instanced->setFloat("uFogDensity", fogDen);
    instanced->setFloat("uExposure", exposure);
    instanced->setFloat("uTime", (float)glfwGetTime());

    // Lights
    auto ambients = reg.view<AmbientLight>();
    if (!ambients.empty()) {
        auto& a = reg.get<AmbientLight>(ambients[0]);
        instanced->setVec3("uAmbientColor", a.color);
        instanced->setFloat("uAmbientIntensity", a.intensity);
    } else {
        instanced->setVec3("uAmbientColor", glm::vec3(0.15f));
        instanced->setFloat("uAmbientIntensity", 1.0f);
    }
    auto dirs = reg.view<DirectionalLight>();
    if (!dirs.empty()) {
        auto& d = reg.get<DirectionalLight>(dirs[0]);
        instanced->setBool("uHasDirLight", true);
        instanced->setVec3("uDirLight.direction", d.direction);
        instanced->setVec3("uDirLight.color", d.color);
        instanced->setFloat("uDirLight.intensity", d.intensity);
    } else {
        instanced->setBool("uHasDirLight", false);
    }
    auto points = reg.view<PointLight, Transform>();
    int cnt = std::min<int>(static_cast<int>(points.size()), 8);
    instanced->setInt("uPointLightCount", cnt);
    for (int i = 0; i < cnt; ++i) {
        Entity e = points[static_cast<size_t>(i)];
        auto& pl = reg.get<PointLight>(e);
        auto& tr = reg.get<Transform>(e);
        std::string base = "uPointLights[" + std::to_string(i) + "]";
        instanced->setVec3((base + ".position").c_str(), tr.position);
        instanced->setVec3((base + ".color").c_str(), pl.color);
        instanced->setFloat((base + ".intensity").c_str(), pl.intensity);
        instanced->setFloat((base + ".range").c_str(), pl.range);
        instanced->setFloat((base + ".constant").c_str(), pl.constant);
        instanced->setFloat((base + ".linear").c_str(), pl.linear);
        instanced->setFloat((base + ".quadratic").c_str(), pl.quadratic);
    }

    for (auto& [key, batch] : m_batches) {
        if (batch.instances.empty()) continue;
        if (batch.material.useDiffuseMap && batch.material.diffuseMap && batch.material.diffuseMap->valid()) {
            instanced->setBool("uUseDiffuseMap", true);
            batch.material.diffuseMap->bind(0);
            instanced->setInt("uDiffuseMap", 0);
        } else {
            instanced->setBool("uUseDiffuseMap", false);
        }
        if (batch.material.useSpecularMap && batch.material.specularMap && batch.material.specularMap->valid()) {
            instanced->setBool("uUseSpecularMap", true);
            batch.material.specularMap->bind(1);
            instanced->setInt("uSpecularMap", 1);
        } else {
            instanced->setBool("uUseSpecularMap", false);
        }
        batch.mesh->drawInstanced(batch.instances.data(), batch.instances.size());
    }
}
