#include "engine/Renderer/MeshClusters.h"
#include "engine/Renderer/MeshLoader.h"
#include "engine/Renderer/DefaultShaders.h"
#include "engine/Renderer/Shader.h"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {
int s_drawnClusters = 0;

inline uint64_t posHash(const glm::vec3& p) {
    uint32_t x, y, z;
    std::memcpy(&x, &p.x, 4);
    std::memcpy(&y, &p.y, 4);
    std::memcpy(&z, &p.z, 4);
    return (uint64_t(x) * 73856093u) ^ (uint64_t(y) * 19349663u) ^ (uint64_t(z) * 83492791u);
}

inline uint64_t cellKey(const glm::vec3& p, const glm::vec3& n, float cell) {
    uint64_t x = static_cast<uint64_t>(static_cast<int64_t>(std::floor(p.x / cell)));
    uint64_t y = static_cast<uint64_t>(static_cast<int64_t>(std::floor(p.y / cell)));
    uint64_t z = static_cast<uint64_t>(static_cast<int64_t>(std::floor(p.z / cell)));
    uint64_t o = (n.x > 0 ? 1u : 0u) | (n.y > 0 ? 2u : 0u) | (n.z > 0 ? 4u : 0u);
    return ((x & 0x1FFFFF) | ((y & 0x1FFFFF) << 21) | ((z & 0x1FFFFF) << 42)) ^ (o << 60);
}

inline void NormalizePlanes(glm::vec4 planes[6]) {
    for (int i = 0; i < 6; ++i) {
        float len = glm::length(glm::vec3(planes[i]));
        if (len > 1e-6f) planes[i] /= len;
    }
}

inline bool FrustumSphere(const glm::vec4 planes[6], const glm::vec3& c, float r) {
    for (int p = 0; p < 6; ++p) {
        if (glm::dot(glm::vec3(planes[p]), c) + planes[p].w < -r) return false;
    }
    return true;
}

inline uint32_t ExpandBits(uint32_t v) {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

inline uint32_t Morton3D(float x, float y, float z) {
    uint32_t ix = static_cast<uint32_t>(glm::clamp(x * 1023.0f, 0.0f, 1023.0f));
    uint32_t iy = static_cast<uint32_t>(glm::clamp(y * 1023.0f, 0.0f, 1023.0f));
    uint32_t iz = static_cast<uint32_t>(glm::clamp(z * 1023.0f, 0.0f, 1023.0f));
    return (ExpandBits(ix) << 2) | (ExpandBits(iy) << 1) | ExpandBits(iz);
}

} // namespace

ClusteredMesh::~ClusteredMesh() {
    Shutdown();
}

ClusteredMesh::ClusteredMesh(ClusteredMesh&& other) noexcept
    : m_lods(std::move(other.m_lods)),
      m_clusters(std::move(other.m_clusters)),
      m_baseCell(other.m_baseCell),
      m_boundCenter(other.m_boundCenter),
      m_boundRadius(other.m_boundRadius),
      m_lodRanges(std::move(other.m_lodRanges)),
      m_instanceVBO(other.m_instanceVBO),
      m_instanceCap(other.m_instanceCap) {
    other.m_instanceVBO = 0;
    other.m_instanceCap = 0;
}

ClusteredMesh& ClusteredMesh::operator=(ClusteredMesh&& other) noexcept {
    if (this != &other) {
        Shutdown();
        m_lods = std::move(other.m_lods);
        m_clusters = std::move(other.m_clusters);
        m_baseCell = other.m_baseCell;
        m_boundCenter = other.m_boundCenter;
        m_boundRadius = other.m_boundRadius;
        m_lodRanges = std::move(other.m_lodRanges);
        m_instanceVBO = other.m_instanceVBO;
        m_instanceCap = other.m_instanceCap;
        other.m_instanceVBO = 0;
        other.m_instanceCap = 0;
    }
    return *this;
}

void ClusteredMesh::Shutdown() {
    for (auto& lod : m_lods) {
        if (lod.vao) { glDeleteVertexArrays(1, &lod.vao); lod.vao = 0; }
        if (lod.vbo) { glDeleteBuffers(1, &lod.vbo); lod.vbo = 0; }
        if (lod.ebo) { glDeleteBuffers(1, &lod.ebo); lod.ebo = 0; }
    }
    m_lods.clear();
    m_clusters.clear();
    m_lodRanges.clear();

    if (m_instanceVBO) {
        glDeleteBuffers(1, &m_instanceVBO);
        m_instanceVBO = 0;
    }
    m_instanceCap = 0;
}

static void SimplifyVertexCluster(const std::vector<Vertex>& inV, const std::vector<uint32_t>& inI,
                                  float cell, std::vector<Vertex>& outV, std::vector<uint32_t>& outI,
                                  std::vector<uint32_t>& surviveCount,
                                  const std::unordered_set<uint64_t>* locked) {
    outV.clear();
    outI.clear();
    surviveCount.clear();
    std::unordered_map<uint64_t, uint32_t> weld;
    weld.reserve(inV.size());

    const uint64_t LOCKBIT = 1ull << 63;
    auto keyOf = [&](const Vertex& v) -> uint64_t {
        return (locked && locked->count(posHash(v.position)))
             ? (posHash(v.position) | LOCKBIT)
             : cellKey(v.position, v.normal, cell);
    };

    for (const auto& v : inV) {
        uint64_t key = keyOf(v);
        auto it = weld.find(key);
        if (it != weld.end()) continue;
        uint32_t id = static_cast<uint32_t>(outV.size());
        weld.emplace(key, id);
        outV.push_back(v);
    }

    std::vector<uint32_t> raw;
    raw.resize(inI.size());
    for (size_t i = 0; i < inI.size(); ++i) raw[i] = weld[keyOf(inV[inI[i]])];

    const float minArea2 = cell * cell * 1e-3f;
    surviveCount.reserve(inI.size() / 3);
    uint32_t acc = 0;
    for (size_t t = 0; t < inI.size() / 3; ++t) {
        uint32_t a = raw[t * 3], b = raw[t * 3 + 1], c = raw[t * 3 + 2];
        bool bad = (a == b || b == c || a == c);
        if (!bad) {
            glm::vec3 p0 = inV[inI[t * 3]].position, p1 = inV[inI[t * 3 + 1]].position, p2 = inV[inI[t * 3 + 2]].position;
            glm::vec3 cr = glm::cross(p1 - p0, p2 - p0);
            if (glm::dot(cr, cr) < minArea2) bad = true;
        }
        if (!bad) {
            outI.insert(outI.end(), {a, b, c});
            ++acc;
        }
        surviveCount.push_back(acc);
    }
}

static void MortonReorder(std::vector<uint32_t>& idx, std::vector<Vertex>& verts,
                          const glm::vec3& bmin, const glm::vec3& bmax) {
    size_t nTri = idx.size() / 3;
    if (nTri == 0) return;
    glm::vec3 bsz = bmax - bmin;
    bsz.x = std::max(bsz.x, 1e-4f);
    bsz.y = std::max(bsz.y, 1e-4f);
    bsz.z = std::max(bsz.z, 1e-4f);

    struct TriKey { uint32_t code; uint32_t t; };
    std::vector<TriKey> tk(nTri);
    for (size_t t = 0; t < nTri; ++t) {
        glm::vec3 c = (verts[idx[t * 3]].position + verts[idx[t * 3 + 1]].position + verts[idx[t * 3 + 2]].position) / 3.0f;
        glm::vec3 u = (c - bmin) / bsz;
        tk[t] = { Morton3D(u.x, u.y, u.z), static_cast<uint32_t>(t) };
    }
    std::sort(tk.begin(), tk.end(), [](const TriKey& a, const TriKey& b){ return a.code < b.code; });

    std::vector<uint32_t> nidx(idx.size());
    for (size_t i = 0; i < nTri; ++i) {
        size_t st = tk[i].t;
        nidx[i * 3 + 0] = idx[st * 3 + 0];
        nidx[i * 3 + 1] = idx[st * 3 + 1];
        nidx[i * 3 + 2] = idx[st * 3 + 2];
    }
    idx = std::move(nidx);
}

void ClusteredMesh::Build(const std::vector<Vertex>& verts, const std::vector<uint32_t>& idx,
                          int maxLODs, int clusterTris) {
    Shutdown();
    if (verts.empty() || idx.empty()) return;

    glm::vec3 bmin(1e9f), bmax(-1e9f);
    for (const auto& v : verts) {
        bmin = glm::min(bmin, v.position);
        bmax = glm::max(bmax, v.position);
    }
    m_boundCenter = (bmin + bmax) * 0.5f;
    m_boundRadius = glm::length(bmax - bmin) * 0.5f;
    float maxDim = std::max({ bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z });
    m_baseCell = maxDim * 0.0015f;

    std::vector<Vertex> curV = verts;
    std::vector<uint32_t> curI = idx;
    MortonReorder(curI, curV, bmin, bmax);

    std::vector<std::vector<Vertex>> lodVerts;
    std::vector<std::vector<uint32_t>> lodIdx;
    std::vector<float> lodErrors;

    lodVerts.push_back(curV);
    lodIdx.push_back(curI);
    lodErrors.push_back(0.0f);

    float cell = m_baseCell;
    for (int l = 1; l < maxLODs; ++l) {
        std::vector<Vertex> nxtV;
        std::vector<uint32_t> nxtI;
        std::vector<uint32_t> survive;
        SimplifyVertexCluster(curV, curI, cell, nxtV, nxtI, survive, nullptr);
        if (nxtI.empty()) break;
        MortonReorder(nxtI, nxtV, bmin, bmax);
        lodVerts.push_back(nxtV);
        lodIdx.push_back(nxtI);
        lodErrors.push_back(cell);
        curV = std::move(nxtV);
        curI = std::move(nxtI);
        cell *= 2.0f;
    }

    int nLods = static_cast<int>(lodVerts.size());
    m_lods.resize(nLods);
    m_lodRanges.resize(nLods);

    for (int l = 0; l < nLods; ++l) {
        glGenVertexArrays(1, &m_lods[l].vao);
        glGenBuffers(1, &m_lods[l].vbo);
        glGenBuffers(1, &m_lods[l].ebo);

        glBindVertexArray(m_lods[l].vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_lods[l].vbo);
        glBufferData(GL_ARRAY_BUFFER, lodVerts[l].size() * sizeof(Vertex), lodVerts[l].data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lods[l].ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, lodIdx[l].size() * sizeof(uint32_t), lodIdx[l].data(), GL_STATIC_DRAW);

        // Standard Vertex attributes (0:pos, 1:color, 2:normal, 3:uv)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

        glBindVertexArray(0);

        m_lodRanges[l].firstIndex = 0;
        m_lodRanges[l].indexCount = static_cast<uint32_t>(lodIdx[l].size());
        m_lodRanges[l].ownError = lodErrors[l];
        m_lodRanges[l].coarserError = (l + 1 < nLods) ? lodErrors[l + 1] : 1e9f;

        // Cut LOD into clusters
        size_t totalTris = lodIdx[l].size() / 3;
        for (size_t t = 0; t < totalTris; t += clusterTris) {
            size_t count = std::min(static_cast<size_t>(clusterTris), totalTris - t);
            MeshCluster cl;
            cl.lod = static_cast<uint32_t>(l);
            cl.indexOffset = static_cast<uint32_t>(t * 3);
            cl.indexCount = static_cast<uint32_t>(count * 3);
            cl.ownError = m_lodRanges[l].ownError;
            cl.coarserError = m_lodRanges[l].coarserError;

            glm::vec3 cmin(1e9f), cmax(-1e9f);
            for (size_t i = 0; i < cl.indexCount; ++i) {
                const auto& p = lodVerts[l][lodIdx[l][cl.indexOffset + i]].position;
                cmin = glm::min(cmin, p);
                cmax = glm::max(cmax, p);
            }
            cl.center = (cmin + cmax) * 0.5f;
            cl.radius = glm::length(cmax - cmin) * 0.5f;
            m_clusters.push_back(cl);
        }
    }
}

int ClusteredMesh::SelectAndFill(const glm::mat4& mvp, const glm::mat4& model, const glm::vec3& camPos,
                                 float fovYrad, float screenH, float thresholdPx) const {
    if (m_clusters.empty()) return 0;

    float objScale = glm::length(glm::vec3(model[0]));
    glm::mat4 mT = glm::transpose(mvp);
    glm::vec4 planes[6] = { mT[3]+mT[0], mT[3]-mT[0], mT[3]+mT[1], mT[3]-mT[1], mT[3]+mT[2], mT[3]-mT[2] };
    NormalizePlanes(planes);

    glm::vec3 worldBoundCenter = glm::vec3(model * glm::vec4(m_boundCenter, 1.0f));
    if (!FrustumSphere(planes, worldBoundCenter, m_boundRadius * objScale)) return 0;

    float tanHalf = std::tan(fovYrad * 0.5f);
    std::vector<GLsizei> counts[8];
    std::vector<const void*> offs[8];

    auto visible = [&](const MeshCluster& cl) -> bool {
        glm::vec3 wc = glm::vec3(model * glm::vec4(cl.center, 1.0f));
        if (!FrustumSphere(planes, wc, cl.radius * objScale)) return false;
        float dist = std::max(glm::length(camPos - wc) - cl.radius * objScale, 0.05f);
        float pxScale = screenH / (2.0f * dist * tanHalf + 1e-6f) * objScale;
        if (cl.coarserError * pxScale <= thresholdPx) return false;
        if (cl.coarserError < 1e8f && cl.ownError * pxScale > thresholdPx) return false;
        return true;
    };

    int drawn = 0;
    for (const auto& cl : m_clusters) {
        if (!visible(cl)) continue;
        int lod = std::min(static_cast<int>(cl.lod), 7);
        counts[lod].push_back(static_cast<GLsizei>(cl.indexCount));
        offs[lod].push_back(reinterpret_cast<const void*>(static_cast<uintptr_t>(cl.indexOffset * sizeof(uint32_t))));
        ++drawn;
    }

    for (int k = 7; k >= 0; --k) {
        if (counts[k].empty() || k >= static_cast<int>(m_lods.size())) continue;
        glBindVertexArray(m_lods[k].vao);
        glMultiDrawElements(GL_TRIANGLES, counts[k].data(), GL_UNSIGNED_INT, offs[k].data(), static_cast<GLsizei>(counts[k].size()));
    }
    glBindVertexArray(0);
    return drawn;
}

int ClusteredMesh::Draw(const glm::mat4& mvp, const glm::mat4& model, const glm::vec3& camPos,
                        float fovYrad, float screenH, float errorThresholdPx) const {
    int drawn = SelectAndFill(mvp, model, camPos, fovYrad, screenH, errorThresholdPx);
    s_drawnClusters += drawn;
    return drawn;
}

int ClusteredMesh::DrawLODColors(const glm::mat4& mvp, const glm::mat4& model, const glm::vec3& camPos,
                                 float fovYrad, float screenH, float errorThresholdPx) const {
    static std::unique_ptr<Shader> flatShader = nullptr;
    if (!flatShader) {
        flatShader = std::make_unique<Shader>(kFlatVS, kFlatFS);
    }
    flatShader->use();
    flatShader->setMat4("uMVP", mvp);

    static const glm::vec3 rainbow[8] = {
        {1.0f, 0.15f, 0.10f}, {1.0f, 0.60f, 0.05f}, {1.0f, 0.95f, 0.10f}, {0.20f, 1.0f, 0.25f},
        {0.10f, 0.95f, 0.95f}, {0.25f, 0.40f, 1.0f}, {0.65f, 0.20f, 1.0f}, {0.90f, 0.30f, 0.95f}
    };

    int drawn = SelectAndFill(mvp, model, camPos, fovYrad, screenH, errorThresholdPx);
    s_drawnClusters += drawn;
    return drawn;
}

void ClusteredMesh::EnsureInstanceVBO(int needed) const {
    if (m_instanceVBO == 0) {
        glGenBuffers(1, &m_instanceVBO);
    }
    if (needed > m_instanceCap) {
        m_instanceCap = std::max(needed, m_instanceCap * 2 + 16);
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, m_instanceCap * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void ClusteredMesh::BindInstanceAttribs() const {
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(4 + i);
        glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), reinterpret_cast<void*>(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(4 + i, 1);
    }
}

int ClusteredMesh::PickLOD(float dist, float objScale, float fovYrad, float screenH, float thresholdPx) const {
    float tanHalf = std::tan(fovYrad * 0.5f);
    float pxScale = screenH / (2.0f * std::max(dist, 0.05f) * tanHalf + 1e-6f) * objScale;
    for (int l = 0; l < static_cast<int>(m_lodRanges.size()); ++l) {
        if (m_lodRanges[l].coarserError * pxScale > thresholdPx || l == static_cast<int>(m_lodRanges.size()) - 1) {
            return l;
        }
    }
    return static_cast<int>(m_lodRanges.size()) - 1;
}

int ClusteredMesh::DrawInstanced(const glm::mat4* models, int count,
                                 const glm::vec3& camPos, const glm::mat4& viewProj,
                                 float fovYrad, float screenH, float thresholdPx) const {
    if (count <= 0 || m_lods.empty()) return 0;

    EnsureInstanceVBO(count);

    std::vector<glm::mat4> lodModels[8];
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = glm::vec3(models[i][3]);
        float scale = glm::length(glm::vec3(models[i][0]));
        float dist = glm::length(camPos - pos);
        int lod = PickLOD(dist, scale, fovYrad, screenH, thresholdPx);
        lod = std::clamp(lod, 0, static_cast<int>(m_lods.size()) - 1);
        lodModels[lod].push_back(models[i]);
    }

    int totalDrawn = 0;
    for (int l = 0; l < static_cast<int>(m_lods.size()); ++l) {
        if (lodModels[l].empty()) continue;
        int instCount = static_cast<int>(lodModels[l].size());
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, instCount * sizeof(glm::mat4), lodModels[l].data());

        glBindVertexArray(m_lods[l].vao);
        BindInstanceAttribs();
        glDrawElementsInstanced(GL_TRIANGLES, m_lodRanges[l].indexCount, GL_UNSIGNED_INT, 0, instCount);
        totalDrawn += instCount;
    }
    glBindVertexArray(0);
    return totalDrawn;
}

int ClusteredMesh::DrawInstancedShadow(const glm::mat4* models, int count,
                                       const glm::vec3& lightEye, const glm::mat4& lightVP,
                                       float thresholdPx) const {
    (void)lightVP;
    return DrawInstanced(models, count, lightEye, lightVP, glm::radians(60.0f), 1024.0f, thresholdPx);
}

size_t ClusteredMesh::totalTrisLod0() const {
    size_t n = 0;
    for (const auto& c : m_clusters) if (c.lod == 0) n += c.indexCount / 3;
    return n;
}

int ClusteredMesh::lastDrawn() { return s_drawnClusters; }
void ClusteredMesh::ResetStats() { s_drawnClusters = 0; }

namespace Assets {
namespace { std::unordered_map<std::string, std::weak_ptr<ClusteredMesh>> s_clusterCache; }

std::shared_ptr<ClusteredMesh> Clustered(const std::string& objPath, int maxLODs) {
    auto it = s_clusterCache.find(objPath);
    if (it != s_clusterCache.end()) {
        if (auto sp = it->second.lock()) return sp;
    }
    auto src = MeshLoader::LoadOBJ(objPath);
    auto cm = std::make_shared<ClusteredMesh>();
    cm->Build(src->vertices(), src->indices(), maxLODs);
    s_clusterCache[objPath] = cm;
    return cm;
}

std::shared_ptr<ClusteredMesh> ClusteredFrom(const std::shared_ptr<Mesh3D>& mesh) {
    static std::unordered_map<Mesh3D*, std::weak_ptr<ClusteredMesh>> autoCache;
    auto it = autoCache.find(mesh.get());
    if (it != autoCache.end()) {
        if (auto sp = it->second.lock()) return sp;
    }
    auto cm = std::make_shared<ClusteredMesh>();
    cm->Build(mesh->vertices(), mesh->indices(), 3);
    autoCache[mesh.get()] = cm;
    return cm;
}

} // namespace Assets
