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
#include "engine/Renderer/CascadedShadowMap.h"
#include "engine/Core/Window.h"
#include "engine/Math/Frustum.h"
#include "engine/Renderer/ForwardPlus.h"
#include "engine/Renderer/Decal.h"
#include "engine/Gameplay/TriggerZone.h"
#include "engine/Renderer/VXGI.h"
#include "engine/Renderer/LightProbeGrid.h"
#include "engine/Environment/WorldEnvironment.h"
#include "engine/Core/Profiler.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

// Глобальная карта теней (одна на кадр — направленный свет)
inline CascadedShadowMap& GlobalShadow() { static CascadedShadowMap s; return s; }

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
        bool hasCluster = mr.clusterMesh && !mr.clusterMesh->empty();
        if (!mr.visible || ((!mr.mesh || mr.mesh->empty()) && !hasCluster)) continue;
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
// Ленивая кластеризация: только для тяжелых высокополигональных мешей (>= 512 индексов)
inline void ClusterLODAutoFill(Registry& reg) {
    if (!cluster_lod::enabled) return;
    for (Entity e : reg.view<Transform, MeshRenderer>()) {
        auto& mr = reg.get<MeshRenderer>(e);
        if (mr.clusterLOD && mr.mesh && !mr.mesh->empty() && !mr.clusterMesh)
            mr.clusterMesh = Assets::ClusteredFrom(mr.mesh);
    }
}
inline void NaniteAutoFill(Registry& reg) { ClusterLODAutoFill(reg); }

// кластерный путь активен?
inline bool ClusterLODActive(const MeshRenderer& mr) {
    bool has = mr.clusterMesh && !mr.clusterMesh->empty();
    if (!has) return false;
    if (!mr.mesh || mr.mesh->empty()) return true;            // cluster-only
    return cluster_lod::enabled && mr.clusterLOD;             // обычные объекты уважают флаги
}
inline bool NaniteActive(const MeshRenderer& mr) { return ClusterLODActive(mr); }

// Обновление скелетных анимаций
inline void AnimationUpdate(Registry& reg, float dt) {
    for (Entity e : reg.view<AnimatorComponent>()) {
        auto& anim = reg.get<AnimatorComponent>(e);
        if (anim.animator) {
            anim.animator->update(dt);
        }
    }
}

inline void RenderLit(Registry& reg, const Shader& shader, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos, CascadedShadowMap* shadow = nullptr, float screenW = 1280.0f, float screenH = 720.0f, const glm::mat4& prevViewProj = glm::mat4(1.0f)) {
    (void)shader;
    // --- Instanced cluster shader (lazy init) ---
    static Shader* clusterShader = nullptr;
    if (!clusterShader) clusterShader = new Shader(DefaultShaders::kClusterInstancedVS, DefaultShaders::kLitFS);
    static ForwardPlus* forwardPlus = nullptr;
    if (!forwardPlus) {
        forwardPlus = new ForwardPlus();
        forwardPlus->init();
    }

    // --- Skinned mesh shader (lazy init) ---
    static Shader* skinnedShader = nullptr;
    if (!skinnedShader) skinnedShader = new Shader(DefaultShaders::kSkinnedLitVS, DefaultShaders::kLitFS);

    auto setupLighting = [&](const Shader& sh) {
        sh.use();
        sh.setVec3("uViewPos", viewPos);
        sh.setFloat("uTime", static_cast<float>(glfwGetTime()));
        sh.setMat4("uPrevViewProj", prevViewProj);
        if (auto v = reg.view<AmbientLight>(); v.begin() != v.end()) {
            auto& a = reg.get<AmbientLight>(*v.begin());
            sh.setVec3("uAmbientColor", a.color); sh.setFloat("uAmbientIntensity", a.intensity);
        } else { sh.setVec3("uAmbientColor", glm::vec3(0.15f)); sh.setFloat("uAmbientIntensity", 1.0f); }
        if (auto v = reg.view<DirectionalLight>(); v.begin() != v.end()) {
            auto& d = reg.get<DirectionalLight>(*v.begin());
            sh.setBool("uHasDirLight", true);
            sh.setVec3("uDirLight.direction", d.direction); sh.setVec3("uDirLight.color", d.color);
            sh.setFloat("uDirLight.intensity", d.intensity);
        } else { sh.setBool("uHasDirLight", false); }

        sh.setMat4("uView", view);
        sh.setMat4("uProj", proj);
        sh.setVec2("uScreenSize", glm::vec2(screenW, screenH));
        float nearP = proj[3][2] / (proj[2][2] - 1.0f);
        float farP = proj[3][2] / (proj[2][2] + 1.0f);
        sh.setFloat("uZNear", nearP);
        sh.setFloat("uZFar", farP);
        
        forwardPlus->bindBuffers(0, 1);

        glm::vec3 fogCol{0.12f,0.14f,0.18f}; float fogDen=0.025f;
        if (auto v = reg.view<Fog>(); v.begin() != v.end()) { auto& f=reg.get<Fog>(*v.begin()); fogCol=f.color; fogDen=f.density; }
        sh.setVec3("uFogColor", fogCol); sh.setFloat("uFogDensity", fogDen);
        
        Sky sky;
        if (auto v = reg.view<Sky>(); v.begin() != v.end()) sky = reg.get<Sky>(*v.begin());
        sh.setFloat("uExposure", sky.exposure);
        sh.setVec3("uSkyTop", sky.top);
        sh.setVec3("uSkyHorizon", sky.horizon);
        sh.setVec3("uSkyBottom", sky.bottom);
        sh.setFloat("uSkyExposure", sky.exposure);

        sh.setBool("uHasShadow", shadow && shadow->ready());
        if (shadow && shadow->ready()) {
            shadow->bind(3);
            sh.setInt("uShadowMapArray", 3);
            const auto& splits = shadow->cascadeSplits();
            const auto& matrices = shadow->lightMatrices();
            for (size_t i = 0; i < 3 && i < matrices.size(); ++i) {
                std::string name = "uLightMatrices[" + std::to_string(i) + "]";
                sh.setMat4(name.c_str(), matrices[i]);
            }
            for (size_t i = 0; i < splits.size(); ++i) {
                std::string name = "uCascadeSplits[" + std::to_string(i) + "]";
                sh.setFloat(name.c_str(), splits[i]);
            }
        }

        // VXGI Voxel Cone Tracing
        auto& vxgi = cjoka::VXGI::Get();
        sh.setBool("uUseVXGI", vxgi.enabled());
        if (vxgi.enabled()) {
            vxgi.bindVoxelTexture(7);
            sh.setInt("uVoxelGrid", 7);
            sh.setVec3("uVoxelCenter", vxgi.gridCenter());
            sh.setFloat("uVoxelExtent", vxgi.gridExtent());
            sh.setFloat("uVXGIIntensity", vxgi.giIntensity());
        }

        // Light Probe Grid (Baked Mixed GI)
        auto& probes = cjoka::LightProbeGrid::Get();
        sh.setBool("uUseLightProbes", probes.enabled());
        sh.setFloat("uLightProbeIntensity", probes.intensity());
        if (probes.enabled()) {
            probes.bindSSBO(6);
        }
    };

    struct ClusterGroup {
        const Material* material = nullptr;
        std::vector<InstanceData> instances;
    };
    std::unordered_map<ClusteredMesh*, ClusterGroup> clusterGroups;
    std::vector<Entity> nonClusterEntities;

    std::vector<PointLightData> fwLights;
    for (Entity e : reg.view<PointLight, Transform>()) {
        auto& pl = reg.get<PointLight>(e);
        auto& tr = reg.get<Transform>(e);
        fwLights.push_back({
            glm::vec4(tr.position, pl.range),
            glm::vec4(pl.color, pl.intensity)
        });
    }
    forwardPlus->cullLights(view, proj, fwLights, screenW, screenH);

    Math::Frustum frustum = Math::Frustum::createFrustumFromMatrix(proj * view);

    for (Entity e : reg.view<Transform, MeshRenderer>()) {
        auto& mr = reg.get<MeshRenderer>(e);
        if (!mr.visible) continue;
        
        auto& tr = reg.get<Transform>(e);
        glm::mat4 model = tr.matrix();
        
        // Frustum Culling
        if (mr.mesh && !mr.mesh->empty()) {
            if (!frustum.isOnFrustum(mr.mesh->minExtents(), mr.mesh->maxExtents(), model)) {
                continue;
            }
        }
        
        bool hasCluster = ClusterLODActive(mr);
        if (hasCluster) {
            auto& grp = clusterGroups[mr.clusterMesh.get()];
            if (!grp.material) grp.material = &mr.material;
            InstanceData inst;
            inst.model = model;
            inst.prevModel = tr.prevMatrix;
            inst.albedo = glm::vec4(mr.material.albedo, mr.material.roughness);
            inst.emissive = glm::vec4(mr.material.emissive, mr.material.metallic);
            grp.instances.push_back(inst);
        } else if (mr.mesh && !mr.mesh->empty()) {
            nonClusterEntities.push_back(e);
        }
    }

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, bufs);

    // 1. Instanced cluster draw
    if (!clusterGroups.empty()) {
        float tanHalf = 1.0f / proj[1][1];
        float fovYrad = 2.0f * std::atan(tanHalf);
        glm::mat4 vp = proj * view;

        setupLighting(*clusterShader);
        clusterShader->use();
        clusterShader->setMat4("uView", view);
        clusterShader->setMat4("uProj", proj);
        clusterShader->setMat4("uPrevViewProj", prevViewProj);

        for (auto& [mesh, grp] : clusterGroups) {
            const Material& mat = *grp.material;
            clusterShader->setVec3("uAlbedo", mat.albedo);
            clusterShader->setFloat("uMetallic", mat.metallic);
            clusterShader->setFloat("uRoughness", mat.roughness <= 0.02f ? 0.5f : mat.roughness);
            clusterShader->setFloat("uAO", mat.ao);
            float effectiveWetness = std::max(mat.wetness, cjoka::WorldEnvironment::Get().groundWetness);
            clusterShader->setFloat("uWetness", effectiveWetness);
            clusterShader->setVec3("uEmissive", mat.emissive);
            clusterShader->setVec2("uUVTiling", mat.uvTiling);
            if (mat.useDiffuseMap && mat.diffuseMap && mat.diffuseMap->valid()) {
                clusterShader->setBool("uUseDiffuseMap", true);
                mat.diffuseMap->bind(0); clusterShader->setInt("uDiffuseMap", 0);
            } else clusterShader->setBool("uUseDiffuseMap", false);
            if (mat.useNormalMap && mat.normalMap && mat.normalMap->valid()) {
                clusterShader->setBool("uUseNormalMap", true);
                mat.normalMap->bind(2); clusterShader->setInt("uNormalMap", 2);
                clusterShader->setFloat("uNormalScale", mat.normalStrength);
            } else clusterShader->setBool("uUseNormalMap", false);
            if (mat.useSpecularMap && mat.specularMap && mat.specularMap->valid()) {
                clusterShader->setBool("uUseSpecularMap", true);
                mat.specularMap->bind(1); clusterShader->setInt("uSpecularMap", 1);
            } else clusterShader->setBool("uUseSpecularMap", false);

            if (mat.twoSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
            int drawn = mesh->DrawInstanced(grp.instances.data(), (int)grp.instances.size(),
                                viewPos, vp, fovYrad, screenH, cluster_lod::thresholdPx);
            glEnable(GL_CULL_FACE);
            auto& pStats = cjoka::Profiler::Get().stats();
            pStats.totalDrawCalls += (drawn > 0 ? 1u : 0u);
            pStats.clusterDrawCalls += (drawn > 0 ? 1u : 0u);
            pStats.drawnClusters += static_cast<uint32_t>(drawn > 0 ? drawn : 0);
            size_t lod0Tris = mesh->totalTrisLod0() * grp.instances.size();
            pStats.clusterTrianglesLod0 += static_cast<uint32_t>(lod0Tris);
            pStats.totalTriangles += static_cast<uint32_t>((drawn > 0 ? drawn : 0) * 128);
            if (drawn > 0 && lod0Tris > static_cast<size_t>(drawn * 128)) {
                pStats.savedClusterTriangles += static_cast<uint32_t>(lod0Tris - static_cast<size_t>(drawn * 128));
            }
        }
    }

    // 2. Skinned mesh entities (Skeletal Animation)
    auto skinnedEntities = reg.view<Transform, SkinnedMeshRenderer>();
    if (skinnedEntities.begin() != skinnedEntities.end()) {
        setupLighting(*skinnedShader);
        skinnedShader->use();
        skinnedShader->setMat4("uView", view);
        skinnedShader->setMat4("uProj", proj);

        for (Entity e : skinnedEntities) {
            auto& tr = reg.get<Transform>(e);
            auto& smr = reg.get<SkinnedMeshRenderer>(e);
            if (!smr.visible || !smr.mesh || smr.mesh->empty()) continue;

            glm::mat4 model = tr.matrix();
            skinnedShader->setMat4("uMVP", proj * view * model);
            skinnedShader->setMat4("uModel", model);
            skinnedShader->setMat3("uNormalMat", glm::inverseTranspose(glm::mat3(model)));
            skinnedShader->setMat4("uPrevModel", tr.prevMatrix);
            skinnedShader->setVec3("uAlbedo", smr.material.albedo);
            skinnedShader->setFloat("uMetallic", smr.material.metallic);
            skinnedShader->setFloat("uRoughness", smr.material.roughness <= 0.02f ? 0.5f : smr.material.roughness);
            skinnedShader->setFloat("uAO", smr.material.ao);
            skinnedShader->setVec3("uEmissive", smr.material.emissive);
            if (smr.material.useDiffuseMap && smr.material.diffuseMap && smr.material.diffuseMap->valid()) {
                skinnedShader->setBool("uUseDiffuseMap", true);
                smr.material.diffuseMap->bind(0);
                skinnedShader->setInt("uDiffuseMap", 0);
            } else skinnedShader->setBool("uUseDiffuseMap", false);
            if (smr.material.useSpecularMap && smr.material.specularMap && smr.material.specularMap->valid()) {
                skinnedShader->setBool("uUseSpecularMap", true);
                smr.material.specularMap->bind(1);
                skinnedShader->setInt("uSpecularMap", 1);
            } else skinnedShader->setBool("uUseSpecularMap", false);

            // Bones
            auto* animComp = reg.try_get<AnimatorComponent>(e);
            if (animComp && animComp->animator) {
                const auto& mats = animComp->animator->finalBoneMatrices();
                int cnt = static_cast<int>(std::min(mats.size(), static_cast<size_t>(Animation::Animator::MAX_BONES)));
                skinnedShader->setMat4Array("uBones[0]", mats.data(), cnt);
            } else {
                static std::vector<glm::mat4> identBones(Animation::Animator::MAX_BONES, glm::mat4(1.0f));
                skinnedShader->setMat4Array("uBones[0]", identBones.data(), static_cast<int>(identBones.size()));
            }

            smr.mesh->draw();
            auto& pStats = cjoka::Profiler::Get().stats();
            pStats.totalDrawCalls++;
            pStats.totalTriangles += static_cast<uint32_t>(smr.mesh->indices().size() / 3);
        }
    }

    // 3. Regular non-cluster meshes (Batched & Instanced in 1 Draw Call per shared mesh)
    if (!nonClusterEntities.empty()) {
        struct MeshBatchKey {
            Mesh3D* mesh = nullptr;
            GLuint diffuse = 0;
            GLuint normal = 0;
            GLuint specular = 0;
            int uvTilingX = 100;
            int uvTilingY = 100;
            int aoInt = 100;
            int normalStrengthInt = 100;
            int wetnessInt = 0;
            bool twoSided = false;

            bool operator==(const MeshBatchKey& o) const {
                return mesh == o.mesh && diffuse == o.diffuse && normal == o.normal && specular == o.specular &&
                       uvTilingX == o.uvTilingX && uvTilingY == o.uvTilingY &&
                       aoInt == o.aoInt && normalStrengthInt == o.normalStrengthInt &&
                       wetnessInt == o.wetnessInt && twoSided == o.twoSided;
            }
        };
        struct MeshBatchKeyHash {
            size_t operator()(const MeshBatchKey& k) const noexcept {
                size_t h = std::hash<void*>{}(k.mesh);
                h = (h * 397) ^ (std::hash<GLuint>{}(k.diffuse) << 1);
                h = (h * 397) ^ (std::hash<GLuint>{}(k.normal) << 2);
                h = (h * 397) ^ (std::hash<GLuint>{}(k.specular) << 3);
                h = (h * 397) ^ (static_cast<size_t>(k.uvTilingX) << 4);
                h = (h * 397) ^ (static_cast<size_t>(k.uvTilingY) << 5);
                h = (h * 397) ^ (static_cast<size_t>(k.aoInt) << 6);
                h = (h * 397) ^ (static_cast<size_t>(k.normalStrengthInt) << 7);
                h = (h * 397) ^ (static_cast<size_t>(k.wetnessInt) << 8);
                h = (h * 397) ^ (static_cast<size_t>(k.twoSided ? 1 : 0) << 9);
                return h;
            }
        };
        struct MeshBatch {
            Mesh3D* mesh = nullptr;
            Material material;
            std::vector<InstanceData> instances;
        };

        std::unordered_map<MeshBatchKey, MeshBatch, MeshBatchKeyHash> instancedBatches;

        for (Entity e : nonClusterEntities) {
            auto& tr = reg.get<Transform>(e);
            auto& mr = reg.get<MeshRenderer>(e);
            if (!mr.mesh || mr.mesh->empty()) continue;

            GLuint diffId = (mr.material.useDiffuseMap && mr.material.diffuseMap && mr.material.diffuseMap->valid()) ? mr.material.diffuseMap->id() : 0;
            GLuint normId = (mr.material.useNormalMap && mr.material.normalMap && mr.material.normalMap->valid()) ? mr.material.normalMap->id() : 0;
            GLuint specId = (mr.material.useSpecularMap && mr.material.specularMap && mr.material.specularMap->valid()) ? mr.material.specularMap->id() : 0;

            int uvX = static_cast<int>(std::round(mr.material.uvTiling.x * 100.0f));
            int uvY = static_cast<int>(std::round(mr.material.uvTiling.y * 100.0f));
            int aoI = static_cast<int>(std::round(mr.material.ao * 100.0f));
            int normStr = static_cast<int>(std::round(mr.material.normalStrength * 100.0f));
            int wetI = static_cast<int>(std::round(mr.material.wetness * 100.0f));

            MeshBatchKey key{mr.mesh.get(), diffId, normId, specId, uvX, uvY, aoI, normStr, wetI, mr.material.twoSided};
            auto& b = instancedBatches[key];
            if (!b.mesh) {
                b.mesh = mr.mesh.get();
                b.material = mr.material;
            }
            InstanceData inst;
            inst.model = tr.matrix();
            inst.prevModel = tr.prevMatrix;
            inst.albedo = glm::vec4(mr.material.albedo, mr.material.roughness);
            inst.emissive = glm::vec4(mr.material.emissive, mr.material.metallic);
            b.instances.push_back(inst);
        }

        static Shader* instancedLitShader = nullptr;
        if (!instancedLitShader) instancedLitShader = new Shader(DefaultShaders::kLitInstancedVS, DefaultShaders::kLitInstancedFS);

        setupLighting(*instancedLitShader);
        instancedLitShader->use();
        instancedLitShader->setMat4("uView", view);
        instancedLitShader->setMat4("uProj", proj);

        for (auto& [key, batch] : instancedBatches) {
            if (batch.instances.empty()) continue;
            const Material& mat = batch.material;
            float effectiveWetness = std::max(mat.wetness, cjoka::WorldEnvironment::Get().groundWetness);
            instancedLitShader->setFloat("uAO", mat.ao);
            instancedLitShader->setFloat("uWetness", effectiveWetness);
            instancedLitShader->setVec2("uUVTiling", mat.uvTiling);
            if (mat.useDiffuseMap && mat.diffuseMap && mat.diffuseMap->valid()) {
                instancedLitShader->setBool("uUseDiffuseMap", true);
                mat.diffuseMap->bind(0);
                instancedLitShader->setInt("uDiffuseMap", 0);
            } else instancedLitShader->setBool("uUseDiffuseMap", false);

            if (mat.useNormalMap && mat.normalMap && mat.normalMap->valid()) {
                instancedLitShader->setBool("uUseNormalMap", true);
                mat.normalMap->bind(2);
                instancedLitShader->setInt("uNormalMap", 2);
                instancedLitShader->setFloat("uNormalScale", mat.normalStrength);
            } else instancedLitShader->setBool("uUseNormalMap", false);

            if (mat.useSpecularMap && mat.specularMap && mat.specularMap->valid()) {
                instancedLitShader->setBool("uUseSpecularMap", true);
                mat.specularMap->bind(1);
                instancedLitShader->setInt("uSpecularMap", 1);
            } else instancedLitShader->setBool("uUseSpecularMap", false);

            if (mat.twoSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
            batch.mesh->drawInstanced(batch.instances.data(), batch.instances.size());
            glEnable(GL_CULL_FACE);
            
            auto& pStats = cjoka::Profiler::Get().stats();
            pStats.totalDrawCalls++;
            pStats.instancedDrawCalls++;
            pStats.totalTriangles += static_cast<uint32_t>((batch.mesh->indices().size() / 3) * batch.instances.size());
        }
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
    GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, bufs);
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
    static Shader* shInst = nullptr;
    if (!shInst) shInst = new Shader(DefaultShaders::kShadowInstancedVS, DefaultShaders::kShadowDepthFS);
    static Shader* shSkinned = nullptr;
    if (!shSkinned) shSkinned = new Shader(DefaultShaders::kSkinnedShadowVS, DefaultShaders::kShadowDepthFS);

    std::unordered_map<ClusteredMesh*, std::vector<glm::mat4>> clusterGroups;

    Math::Frustum lightFrustum = Math::Frustum::createFrustumFromMatrix(lightMatrix);

    sh->use();
    for (Entity e : reg.view<Transform, MeshRenderer>()) {
        auto& mr = reg.get<MeshRenderer>(e);
        if (!mr.visible || !mr.castShadow) continue;
        auto& tr = reg.get<Transform>(e);
        if (tr.scale.x <= 0.f) continue;
        
        glm::mat4 model = tr.matrix();
        
        // Frustum Culling against light frustum
        if (mr.mesh && !mr.mesh->empty()) {
            if (!lightFrustum.isOnFrustum(mr.mesh->minExtents(), mr.mesh->maxExtents(), model)) {
                continue;
            }
            sh->setMat4("uModel", model);
            sh->setMat4("uLightMatrix", lightMatrix * model);
            mr.mesh->draw();
        } else if (ClusterLODActive(mr) && mr.clusterMesh) {
            clusterGroups[mr.clusterMesh.get()].push_back(model);
        }
    }

    if (!clusterGroups.empty()) {
        shInst->use();
        shInst->setMat4("uLightVP", lightMatrix);
        glm::vec3 approxEye = glm::vec3(0, 40, 0); // свет сверху
        for (auto& [mesh, models] : clusterGroups) {
            if (mesh) mesh->DrawInstancedShadow(models.data(), (int)models.size(), approxEye, lightMatrix, cluster_lod::thresholdPx);
        }
    }

    // Skinned shadows
    for (Entity e : reg.view<Transform, SkinnedMeshRenderer>()) {
        auto& tr = reg.get<Transform>(e);
        auto& smr = reg.get<SkinnedMeshRenderer>(e);
        if (!smr.visible || !smr.castShadow || !smr.mesh || smr.mesh->empty()) continue;
        shSkinned->use();
        shSkinned->setMat4("uLightMatrix", lightMatrix);
        shSkinned->setMat4("uModel", tr.matrix());
        auto* animComp = reg.try_get<AnimatorComponent>(e);
        if (animComp && animComp->animator) {
            const auto& mats = animComp->animator->finalBoneMatrices();
            int cnt = static_cast<int>(std::min(mats.size(), static_cast<size_t>(Animation::Animator::MAX_BONES)));
            shSkinned->setMat4Array("uBones[0]", mats.data(), cnt);
        } else {
            static std::vector<glm::mat4> identBones(Animation::Animator::MAX_BONES, glm::mat4(1.0f));
            shSkinned->setMat4Array("uBones[0]", identBones.data(), static_cast<int>(identBones.size()));
        }
        smr.mesh->draw();
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
// High-level RenderWithCamera — рендерит сцену с явной матрицей камеры view / proj / viewPos
// ------------------------------------------------------------------
inline void RenderWithCamera(Registry& reg, const Shader& shader, const Window& window, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos, const glm::mat4& prevViewProj = glm::mat4(1.0f)) {
    int w,h; window.getFramebufferSize(w,h);
    Sky* skyPtr=nullptr;
    if (auto v = reg.view<Sky>(); v.begin() != v.end()) skyPtr=&reg.get<Sky>(*v.begin());

    NaniteAutoFill(reg);

    // --- CASCADED SHADOW PASS ---
    bool shadowOn = false;
    if (auto v = reg.view<DirectionalLight>(); v.begin() != v.end()) {
        CJOKA_PROFILE_GPU("Cascaded Shadows (CSM)");
        auto& sun = reg.get<DirectionalLight>(*v.begin());
        auto& csm = GlobalShadow();
        auto matrices = csm.calculateLightMatrices(view, proj, sun.direction);

        for (size_t c = 0; c < CascadedShadowMap::CASCADE_COUNT && c < matrices.size(); ++c) {
            csm.beginCascade(static_cast<int>(c), matrices[c]);
            RenderShadowPass(reg, matrices[c]);
        }
        csm.end();
        shadowOn = csm.ready();
    }

    // --- VXGI SCENE VOXELIZATION PASS ---
    auto& vxgi = cjoka::VXGI::Get();
    if (vxgi.enabled()) {
        CJOKA_PROFILE_GPU("VXGI Voxelization");
        const DirectionalLight* sunPtr = nullptr;
        if (auto v = reg.view<DirectionalLight>(); v.begin() != v.end()) {
            sunPtr = &reg.get<DirectionalLight>(*v.begin());
        }
        vxgi.voxelizeScene(reg, viewPos, sunPtr, w, h);
    }

    {
        CJOKA_PROFILE_GPU("Sky Pass");
        DrawSky(view, proj, skyPtr);
    }

    {
        CJOKA_PROFILE_GPU("Opaque Geometry & Nanite LOD");
        bool hasLight = reg.count<DirectionalLight>()>0 || reg.count<PointLight>()>0;
        if (hasLight) RenderLit(reg, shader, view, proj, viewPos, shadowOn ? &GlobalShadow() : nullptr, (float)w, (float)h, prevViewProj);
        else RenderUnlit(reg, shader, view, proj, viewPos);
    }

    // Декали поверх геометрии сцены
    {
        CJOKA_PROFILE_GPU("Decals Pass");
        Renderer::DecalSystem::Render(reg, view, proj, 0);
    }

    // Update prevMatrix for all transforms (motion vectors for next frame)
    for (Entity e : reg.view<Transform>()) {
        reg.get<Transform>(e).updatePrevMatrix();
    }
}

// ------------------------------------------------------------------
// High-level Render — находит камеру в ECS и делегирует RenderWithCamera
// ------------------------------------------------------------------
inline void Render(Registry& reg, const Shader& shader, const Window& window, const glm::mat4& prevViewProj = glm::mat4(1.0f)) {
    int w,h; window.getFramebufferSize(w,h);
    float aspect = static_cast<float>(w) / static_cast<float>(h ? h : 1);
    Entity cam = NullEntity;
    for (Entity e : reg.view<Camera, Transform>()) if (reg.get<Camera>(e).primary) { cam=e; break; }
    if (cam==NullEntity) { auto v=reg.view<Camera, Transform>(); if(v.begin() != v.end()) cam=*v.begin(); }
    glm::mat4 view(1), proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    glm::vec3 viewPos{0,0,5};
    if (cam!=NullEntity) { auto& c=reg.get<Camera>(cam); auto& t=reg.get<Transform>(cam); view=Camera::viewFromTransform(t); proj=c.projection(aspect); viewPos=t.position; }
    RenderWithCamera(reg, shader, window, view, proj, viewPos, prevViewProj);
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

// ------------------------------------------------------------------
// SpringArmSystem — UE4 USpringArmComponent smooth camera boom
// ------------------------------------------------------------------
inline void UpdateSpringArms(Registry& reg, float dt) {
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

    for (Entity armEnt : reg.view<SpringArmComponent, Transform>()) {
        auto& arm = reg.get<SpringArmComponent>(armEnt);
        auto& armTr = reg.get<Transform>(armEnt);

        // Find target
        Entity targetEnt = arm.target;
        if (targetEnt == NullEntity || !reg.valid(targetEnt) || !reg.has<Transform>(targetEnt)) {
            for (Entity e : reg.view<Transform>()) {
                if (e != armEnt) { targetEnt = e; break; }
            }
        }
        if (targetEnt == NullEntity || !reg.valid(targetEnt) || !reg.has<Transform>(targetEnt)) continue;

        const auto& targetTr = reg.get<Transform>(targetEnt);

        // Compute desired boom rotation & position
        float targetYawRad = glm::radians(targetTr.rotation.y);
        glm::vec3 fwd(std::sin(targetYawRad), 0.0f, std::cos(targetYawRad));
        glm::vec3 right(fwd.z, 0.0f, -fwd.x);
        glm::vec3 up(0.0f, 1.0f, 0.0f);

        glm::vec3 targetAnchor = targetTr.position + up * arm.targetOffset.y + right * arm.targetOffset.x + fwd * arm.targetOffset.z;
        glm::vec3 desiredPos = targetAnchor - fwd * arm.targetArmLength + up * arm.socketOffset.y + right * arm.socketOffset.x;

        if (!arm.initialized) {
            arm.currentPosition = desiredPos;
            arm.currentRotation = targetTr.rotation;
            arm.initialized = true;
        }

        // Camera Lag (smooth spring-damper lerp)
        if (arm.enableCameraLag) {
            float lagFactor = std::clamp(arm.cameraLagSpeed * dt, 0.0f, 1.0f);
            arm.currentPosition = glm::mix(arm.currentPosition, desiredPos, lagFactor);
        } else {
            arm.currentPosition = desiredPos;
        }

        if (arm.enableCameraRotationLag) {
            float rotFactor = std::clamp(arm.cameraRotationLagSpeed * dt, 0.0f, 1.0f);
            arm.currentRotation = glm::mix(arm.currentRotation, targetTr.rotation, rotFactor);
        } else {
            arm.currentRotation = targetTr.rotation;
        }

        armTr.position = arm.currentPosition;
        armTr.rotation = arm.currentRotation;

        // If this entity also has a Camera, orient it toward targetAnchor
        if (reg.has<Camera>(armEnt)) {
            glm::vec3 dir = glm::normalize(targetAnchor - armTr.position);
            float pitch = glm::degrees(std::asin(dir.y));
            float yaw = glm::degrees(std::atan2(dir.x, dir.z));
            armTr.rotation.x = pitch;
            armTr.rotation.y = yaw;
        }
    }
}

} // namespace Systems
