#pragma once
// ClusteredMesh — система кластеризованной геометрии и автоматических уровней детализации (LOD).
// Возможности:
//   1. Разбиение меша на кластеры по 128 треугольников (пространственный Morton-порядок).
//   2. Vertex-clustering LOD-цепочка с сохранением граничных вершин без артефактов швов.
//   3. Кадровый Frustum Culling и выбор уровня детализации по screen-space error в пикселях.
//   4. GPU Instancing с пакетной отрисовкой инстансов через glMultiDrawElements / instanced buffers.
#include "engine/Renderer/Mesh3D.h"
#include <vector>
#include <cstdint>
#include <memory>
#include <string>

struct MeshCluster {
    uint32_t lod = 0;
    uint32_t indexOffset = 0;      // Смещение в EBO своего уровня LOD
    uint32_t indexCount = 0;
    glm::vec3 center{0.0f};
    float radius = 0.0f;
    float ownError = 0.0f;         // Ошибка текущего уровня (L0 = 0)
    float coarserError = 0.0f;     // Ошибка более грубого уровня
};

class ClusteredMesh {
public:
    ClusteredMesh() = default;
    ~ClusteredMesh();

    ClusteredMesh(const ClusteredMesh&) = delete;
    ClusteredMesh& operator=(const ClusteredMesh&) = delete;

    ClusteredMesh(ClusteredMesh&& other) noexcept;
    ClusteredMesh& operator=(ClusteredMesh&& other) noexcept;

    void Build(const std::vector<Vertex>& verts, const std::vector<uint32_t>& idx,
               int maxLODs = 4, int clusterTris = 128);
    void Shutdown();

    // Отрисовка отдельного объекта
    int Draw(const glm::mat4& mvp, const glm::mat4& model, const glm::vec3& camPos,
             float fovYrad, float screenH, float errorThresholdPx) const;

    // Режим отладки: визуализация кластеров цветами LOD
    int DrawLODColors(const glm::mat4& mvp, const glm::mat4& model, const glm::vec3& camPos,
                      float fovYrad, float screenH, float errorThresholdPx) const;

    // GPU Instancing (основной высокопроизводительный пайплайн) — now uses InstanceData for motion vectors
    int DrawInstanced(const struct InstanceData* instances, int count,
                      const glm::vec3& camPos, const glm::mat4& viewProj,
                      float fovYrad, float screenH, float thresholdPx) const;

    // Shadow map pass (still just model matrices)
    int DrawInstancedShadow(const glm::mat4* models, int count,
                            const glm::vec3& lightEye, const glm::mat4& lightVP,
                            float thresholdPx) const;

    // Выбор LOD для всего объекта целиком
    int PickLOD(float dist, float objScale, float fovYrad, float screenH, float thresholdPx) const;

    size_t clusterCount() const { return m_clusters.size(); }
    bool empty() const { return m_clusters.empty(); }
    size_t totalTrisLod0() const;
    int lodCount() const { return static_cast<int>(m_lods.size()); }

    static int lastDrawn();
    static void ResetStats();

private:
    struct GLLOD {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
    };

    std::vector<GLLOD> m_lods;
    std::vector<MeshCluster> m_clusters;
    float m_baseCell = 0.0f;
    glm::vec3 m_boundCenter{0.0f};
    float m_boundRadius = 0.0f;

    struct LODRange {
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        float ownError = 0.0f;
        float coarserError = 0.0f;
    };
    std::vector<LODRange> m_lodRanges;

    mutable GLuint m_instanceVBO = 0;
    mutable int m_instanceCap = 0;

    void EnsureInstanceVBO(int needed) const;
    void BindInstanceAttribs() const;

    struct LODSelection {
        std::vector<GLsizei> counts[8];
        std::vector<const void*> offs[8];
        int drawn = 0;
    };

    LODSelection SelectLODs(const glm::mat4& mvp, const glm::mat4& model, const glm::vec3& camPos,
                            float fovYrad, float screenH, float thresholdPx) const;
    void DrawSelection(const LODSelection& sel) const;
};

// Конфигурация кластерного LOD
namespace cluster_lod {
inline bool  enabled = true;
inline float thresholdPx = 1.0f;
inline int   shadowEveryNFrames = 1;
inline bool  useQuadric = true;    // true = QEM simplification, false = vertex clustering
}
namespace clusterLOD = cluster_lod;
namespace nanite = cluster_lod;

namespace Assets {
std::shared_ptr<ClusteredMesh> Clustered(const std::string& objPath, int maxLODs = 4);
std::shared_ptr<ClusteredMesh> ClusteredFrom(const std::shared_ptr<class Mesh3D>& mesh);
}
