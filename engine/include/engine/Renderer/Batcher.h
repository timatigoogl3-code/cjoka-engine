#pragma once
#include "engine/Renderer/Mesh3D.h"
#include "engine/ECS/Components.h"
#include "engine/Renderer/ShadowMap.h"
#include <unordered_map>
#include <vector>
#include <memory>

class Registry; // forward

// Batcher — динамический батчинг + инстансинг
// Группирует по mesh+diffuse, рисует одним drawInstanced.
// Пример: batcher.begin(); for(e : view) batcher.submit(tr,mr); batcher.flush(reg, view, proj, viewPos);
class Batcher {
public:
    void begin();
    void submit(const Transform& tr, const MeshRenderer& mr);
    // flush сам находит свет/туман/скай из Registry и рисует instanced
        void flush(Registry& reg, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos, ShadowMap* shadow = nullptr);

    size_t batchCount() const { return m_batches.size(); }
    size_t totalInstances() const;

private:
    struct Key {
        const Mesh3D* mesh = nullptr;
        GLuint diffuseTex = 0;
        bool operator==(const Key& o) const { return mesh == o.mesh && diffuseTex == o.diffuseTex; }
    };
    struct KeyHash {
        size_t operator()(const Key& k) const noexcept {
            return std::hash<const void*>{}(k.mesh) ^ (std::hash<GLuint>{}(k.diffuseTex) << 1);
        }
    };
    struct Batch {
        const Mesh3D* mesh = nullptr;
        Material material;
        std::vector<InstanceData> instances;
    };
    std::unordered_map<Key, Batch, KeyHash> m_batches;
};
