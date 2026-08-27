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

// Compute vertex indices that are shared by triangles from different clusters.
// These vertices must survive simplification to prevent T-junction cracks.
// Returns posHash(position) values (matching SimplifyVertexCluster's locked key).
std::unordered_set<uint64_t> ComputeLockedVertices(const std::vector<Vertex>& verts,
                                                   const std::vector<uint32_t>& idx,
                                                   int clusterTris) {
    std::unordered_set<uint64_t> locked;
    size_t totalTris = idx.size() / 3;
    std::unordered_map<uint32_t, uint32_t> vertCluster;
    for (size_t t = 0; t < totalTris; ++t) {
        uint32_t clusterId = static_cast<uint32_t>(t / clusterTris);
        for (int j = 0; j < 3; ++j) {
            uint32_t vi = idx[t * 3 + j];
            auto it = vertCluster.find(vi);
            if (it == vertCluster.end()) {
                vertCluster[vi] = clusterId;
            } else if (it->second != clusterId) {
                locked.insert(posHash(verts[vi].position));
            }
        }
    }
    return locked;
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

// ── Quadric Error Metrics (QEM) simplification ──────────────────────────────
// Garland & Heckbert 1997. Preserves thin features (leaves) much better
// than vertex clustering because it measures geometric error per-edge.

struct Quadric4x4 {
    float m[10]; // upper triangle of symmetric 4x4: 0..3=row0, 4..6=row1, 7..8=row2, 9=row3

    Quadric4x4() { std::fill(m, m + 10, 0.0f); }

    static Quadric4x4 FromPlane(float nx, float ny, float nz, float d) {
        Quadric4x4 q;
        q.m[0]=nx*nx; q.m[1]=nx*ny; q.m[2]=nx*nz; q.m[3]=nx*d;
        q.m[4]=ny*ny; q.m[5]=ny*nz; q.m[6]=ny*d;
        q.m[7]=nz*nz; q.m[8]=nz*d;
        q.m[9]=d*d;
        return q;
    }

    Quadric4x4 operator+(const Quadric4x4& b) const {
        Quadric4x4 r;
        for (int i = 0; i < 10; ++i) r.m[i] = m[i] + b.m[i];
        return r;
    }

    float Evaluate(const glm::vec3& v) const {
        float x=v.x, y=v.y, z=v.z;
        return m[0]*x*x + 2*m[1]*x*y + 2*m[2]*x*z + 2*m[3]*x
             + m[4]*y*y + 2*m[5]*y*z + 2*m[6]*y
             + m[7]*z*z + 2*m[8]*z + m[9];
    }
};

static bool SolveOptimal(const Quadric4x4& q, glm::vec3& out) {
    float a00=q.m[0], a01=q.m[1], a02=q.m[2], b0=q.m[3];
    float a11=q.m[4], a12=q.m[5], b1=q.m[6];
    float a22=q.m[7], b2=q.m[8];
    float det = a00*(a11*a22-a12*a12) - a01*(a01*a22-a12*b2) + a02*(a01*a12-a11*b2);
    if (std::abs(det) < 1e-10f) return false;
    float inv = 1.0f / det;
    out.x = -((a11*a22-a12*a12)*b0 - (a01*a22-a12*b2)*b1 + (a01*a12-a11*b2)*b2) * inv;
    out.y = -((a02*a12-a01*a22)*b0 + (a00*a22-a02*a02)*b1 - (a00*a12-a01*a02)*b2) * inv;
    out.z = -((a01*a12-a02*a11)*b0 - (a00*a12-a02*a01)*b1 + (a00*a11-a01*a01)*b2) * inv;
    return true;
}

static void SimplifyQuadric(const std::vector<Vertex>& inV, const std::vector<uint32_t>& inI,
                            float targetRatio, std::vector<Vertex>& outV, std::vector<uint32_t>& outI,
                            std::vector<uint32_t>& surviveCount,
                            const std::unordered_set<uint64_t>* locked) {
    outV.clear();
    outI.clear();
    surviveCount.clear();

    const size_t nVert = inV.size();
    const size_t nTri = inI.size() / 3;
    const size_t targetTri = std::max<size_t>(static_cast<size_t>(nTri * targetRatio), 1);
    if (nTri <= targetTri) {
        outV = inV; outI = inI;
        surviveCount.resize(nTri);
        for (size_t i = 0; i < nTri; ++i) surviveCount[i] = static_cast<uint32_t>(i + 1);
        return;
    }

    // Mark locked vertices
    std::vector<bool> vertLocked(nVert, false);
    if (locked) {
        for (size_t i = 0; i < nVert; ++i)
            if (locked->count(posHash(inV[i].position))) vertLocked[i] = true;
    }

    // Per-vertex quadrics and live positions (updated on collapse)
    std::vector<Quadric4x4> Q(nVert);
    std::vector<glm::vec3> pos(nVert);
    for (size_t i = 0; i < nVert; ++i) pos[i] = inV[i].position;

    // Build face quadrics + edge adjacency
    std::unordered_map<uint64_t, std::vector<uint32_t>> edgeTris;
    edgeTris.reserve(nTri * 3);
    for (uint32_t t = 0; t < nTri; ++t) {
        uint32_t a=inI[t*3], b=inI[t*3+1], c=inI[t*3+2];
        glm::vec3 n = glm::cross(pos[b]-pos[a], pos[c]-pos[a]);
        float area = glm::length(n);
        if (area < 1e-10f) continue;
        n /= area;
        float d = -glm::dot(n, pos[a]);
        auto fq = Quadric4x4::FromPlane(n.x,n.y,n.z,d);
        // Weight by area
        for (int i=0;i<10;++i) fq.m[i] *= area;
        Q[a]=Q[a]+fq; Q[b]=Q[b]+fq; Q[c]=Q[c]+fq;

        auto addE=[&](uint32_t u,uint32_t v){
            uint64_t k=(uint64_t(std::min(u,v))<<32)|std::max(u,v);
            edgeTris[k].push_back(t);
        };
        addE(a,b); addE(b,c); addE(c,a);
    }

    // Edge cost computation
    struct Edge { uint32_t v0,v1; float cost; };
    auto computeCost=[&](uint32_t v0,uint32_t v1)->float{
        Quadric4x4 eq = Q[v0]+Q[v1];
        glm::vec3 opt;
        float cost;
        if (SolveOptimal(eq,opt) && std::isfinite(opt.x)) {
            cost = eq.Evaluate(opt);
        } else {
            float e0=Q[v0].Evaluate(pos[v0]), e1=Q[v1].Evaluate(pos[v1]);
            cost = std::min(e0,e1);
        }
        return cost < 0 ? 0 : cost;
    };

    auto edgeLess=[](const Edge& a,const Edge& b){return a.cost>b.cost;};
    std::vector<Edge> heap;
    heap.reserve(edgeTris.size());
    for (auto& [key,tris] : edgeTris) {
        if (tris.size()<2) continue;
        uint32_t v0=uint32_t(key>>32), v1=uint32_t(key&0xFFFFFFFF);
        if (vertLocked[v0]||vertLocked[v1]) continue;
        heap.push_back({v0,v1,computeCost(v0,v1)});
    }
    std::make_heap(heap.begin(),heap.end(),edgeLess);

    // Union-Find
    std::vector<uint32_t> uf(nVert);
    for (uint32_t i=0;i<nVert;++i) uf[i]=i;
    auto find=[&](uint32_t x)->uint32_t{
        while(uf[x]!=x){uf[x]=uf[uf[x]];x=uf[x];} return x;
    };

    std::vector<bool> alive(nVert,true);
    std::vector<bool> triAlive(nTri,true);
    size_t curTriCount=nTri;

    // Collapse loop
    while (curTriCount>targetTri && !heap.empty()) {
        Edge e=heap.front();
        std::pop_heap(heap.begin(),heap.end(),edgeLess);
        heap.pop_back();

        uint32_t u=find(e.v0), v=find(e.v1);
        if (u==v || !alive[u] || !alive[v]) continue;

        // Merge v → u
        alive[v]=false;
        uf[v]=u;
        Q[u]=Q[u]+Q[v];

        // Optimal position for merged vertex
        Quadric4x4 eq=Q[u];
        glm::vec3 opt;
        if (SolveOptimal(eq,opt) && std::isfinite(opt.x)) {
            pos[u]=opt;
        }
        // else keep pos[u] as is

        // Fix triangles: redirect v→u, remove degenerates
        std::vector<uint64_t> affectedEdges;
        for (uint32_t t=0;t<nTri && curTriCount>targetTri;++t) {
            if (!triAlive[t]) continue;
            uint32_t ra=find(inI[t*3]), rb=find(inI[t*3+1]), rc=find(inI[t*3+2]);
            if (ra==rb || rb==rc || ra==rc) {
                triAlive[t]=false; --curTriCount; continue;
            }
            // Record affected edges
            auto rec=[&](uint32_t x,uint32_t y){
                if(x!=y) affectedEdges.push_back((uint64_t(std::min(x,y))<<32)|std::max(x,y));
            };
            rec(ra,rb); rec(rb,rc); rec(ra,rc);
        }

        // Re-evaluate affected edges
        std::sort(affectedEdges.begin(),affectedEdges.end());
        affectedEdges.erase(std::unique(affectedEdges.begin(),affectedEdges.end()),affectedEdges.end());
        for (uint64_t ek : affectedEdges) {
            uint32_t eu=find(uint32_t(ek>>32)), ev=find(uint32_t(ek&0xFFFFFFFF));
            if(eu==ev||!alive[eu]||!alive[ev]) continue;
            if(vertLocked[eu]||vertLocked[ev]) continue;
            heap.push_back({eu,ev,computeCost(eu,ev)});
            std::push_heap(heap.begin(),heap.end(),edgeLess);
        }
    }

    // Collect surviving vertices
    std::vector<uint32_t> newIdx(nVert, UINT32_MAX);
    outV.clear();
    for (uint32_t i=0;i<nVert;++i) {
        uint32_t root=find(i);
        if (root==i && alive[i]) {
            newIdx[i]=static_cast<uint32_t>(outV.size());
            Vertex v=inV[i];
            v.position=pos[i];
            outV.push_back(v);
        }
    }

    // Remap index buffer
    for (uint32_t t=0;t<nTri;++t) {
        if (!triAlive[t]) continue;
        uint32_t a=find(inI[t*3]), b=find(inI[t*3+1]), c=find(inI[t*3+2]);
        if (a==b||b==c||a==c) continue;
        if (newIdx[a]==UINT32_MAX||newIdx[b]==UINT32_MAX||newIdx[c]==UINT32_MAX) continue;
        outI.push_back(newIdx[a]); outI.push_back(newIdx[b]); outI.push_back(newIdx[c]);
    }

    // Build surviveCount
    surviveCount.resize(nTri);
    uint32_t acc=0;
    for (size_t t=0;t<nTri;++t) {
        if(triAlive[t]) ++acc;
        surviveCount[t]=acc;
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

        // Recompute locked vertices for THIS level's cluster topology
        auto lockedSet = ComputeLockedVertices(curV, curI, clusterTris);
        if (cluster_lod::useQuadric) {
            // QEM: target 50% reduction per level (preserves thin features)
            SimplifyQuadric(curV, curI, 0.5f, nxtV, nxtI, survive, &lockedSet);
        } else {
            SimplifyVertexCluster(curV, curI, cell, nxtV, nxtI, survive, &lockedSet);
        }

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

ClusteredMesh::LODSelection ClusteredMesh::SelectLODs(const glm::mat4& mvp, const glm::mat4& model,
                                                       const glm::vec3& camPos, float fovYrad,
                                                       float screenH, float thresholdPx) const {
    LODSelection sel;
    if (m_clusters.empty()) return sel;

    float objScale = glm::length(glm::vec3(model[0]));
    glm::mat4 mT = glm::transpose(mvp);
    glm::vec4 planes[6] = { mT[3]+mT[0], mT[3]-mT[0], mT[3]+mT[1], mT[3]-mT[1], mT[3]+mT[2], mT[3]-mT[2] };
    NormalizePlanes(planes);

    glm::vec3 worldBoundCenter = glm::vec3(model * glm::vec4(m_boundCenter, 1.0f));
    if (!FrustumSphere(planes, worldBoundCenter, m_boundRadius * objScale)) return sel;

    float tanHalf = std::tan(fovYrad * 0.5f);

    auto visible = [&](const MeshCluster& cl) -> bool {
        glm::vec3 wc = glm::vec3(model * glm::vec4(cl.center, 1.0f));
        if (!FrustumSphere(planes, wc, cl.radius * objScale)) return false;
        float dist = std::max(glm::length(camPos - wc) - cl.radius * objScale, 0.05f);
        float pxScale = screenH / (2.0f * dist * tanHalf + 1e-6f) * objScale;
        if (cl.coarserError * pxScale <= thresholdPx) return false;
        if (cl.coarserError < 1e8f && cl.ownError * pxScale > thresholdPx) return false;
        return true;
    };

    for (const auto& cl : m_clusters) {
        if (!visible(cl)) continue;
        int lod = std::min(static_cast<int>(cl.lod), 7);
        sel.counts[lod].push_back(static_cast<GLsizei>(cl.indexCount));
        sel.offs[lod].push_back(reinterpret_cast<const void*>(static_cast<uintptr_t>(cl.indexOffset * sizeof(uint32_t))));
        ++sel.drawn;
    }
    return sel;
}

void ClusteredMesh::DrawSelection(const LODSelection& sel) const {
    for (int k = 7; k >= 0; --k) {
        if (sel.counts[k].empty() || k >= static_cast<int>(m_lods.size())) continue;
        glBindVertexArray(m_lods[k].vao);
        glMultiDrawElements(GL_TRIANGLES, sel.counts[k].data(), GL_UNSIGNED_INT, sel.offs[k].data(),
                            static_cast<GLsizei>(sel.counts[k].size()));
    }
    glBindVertexArray(0);
}

int ClusteredMesh::Draw(const glm::mat4& mvp, const glm::mat4& model, const glm::vec3& camPos,
                        float fovYrad, float screenH, float errorThresholdPx) const {
    auto sel = SelectLODs(mvp, model, camPos, fovYrad, screenH, errorThresholdPx);
    DrawSelection(sel);
    s_drawnClusters += sel.drawn;
    return sel.drawn;
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

    auto sel = SelectLODs(mvp, model, camPos, fovYrad, screenH, errorThresholdPx);

    for (int k = 7; k >= 0; --k) {
        if (sel.counts[k].empty() || k >= static_cast<int>(m_lods.size())) continue;
        flatShader->setVec3("uColor", rainbow[std::min(k, 7)]);
        glBindVertexArray(m_lods[k].vao);
        glMultiDrawElements(GL_TRIANGLES, sel.counts[k].data(), GL_UNSIGNED_INT, sel.offs[k].data(),
                            static_cast<GLsizei>(sel.counts[k].size()));
    }
    glBindVertexArray(0);

    s_drawnClusters += sel.drawn;
    return sel.drawn;
}

void ClusteredMesh::EnsureInstanceVBO(int needed) const {
    if (m_instanceVBO == 0) {
        glGenBuffers(1, &m_instanceVBO);
    }
    if (needed > m_instanceCap) {
        m_instanceCap = std::max(needed, m_instanceCap * 2 + 16);
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, m_instanceCap * sizeof(InstanceData), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void ClusteredMesh::BindInstanceAttribs() const {
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    // layout 4-7: mat4 model
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(4 + i);
        glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), reinterpret_cast<void*>(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(4 + i, 1);
    }
    // layout 8-11: mat4 prevModel
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(8 + i);
        glVertexAttribPointer(8 + i, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), reinterpret_cast<void*>(sizeof(glm::mat4) + i * sizeof(glm::vec4)));
        glVertexAttribDivisor(8 + i, 1);
    }
    // layout 12: albedo
    glEnableVertexAttribArray(12);
    glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), reinterpret_cast<void*>(2 * sizeof(glm::mat4)));
    glVertexAttribDivisor(12, 1);
    // layout 13: emissive
    glEnableVertexAttribArray(13);
    glVertexAttribPointer(13, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), reinterpret_cast<void*>(2 * sizeof(glm::mat4) + sizeof(glm::vec4)));
    glVertexAttribDivisor(13, 1);
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

int ClusteredMesh::DrawInstanced(const InstanceData* instances, int count,
                                 const glm::vec3& camPos, const glm::mat4& viewProj,
                                 float fovYrad, float screenH, float thresholdPx) const {
    if (count <= 0 || m_lods.empty()) return 0;

    EnsureInstanceVBO(count);

    std::vector<InstanceData> lodInstances[8];
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = glm::vec3(instances[i].model[3]);
        float scale = glm::length(glm::vec3(instances[i].model[0]));
        float dist = glm::length(camPos - pos);
        int lod = PickLOD(dist, scale, fovYrad, screenH, thresholdPx);
        lod = std::clamp(lod, 0, static_cast<int>(m_lods.size()) - 1);
        lodInstances[lod].push_back(instances[i]);
    }

    int totalDrawn = 0;
    for (int l = 0; l < static_cast<int>(m_lods.size()); ++l) {
        if (lodInstances[l].empty()) continue;
        int instCount = static_cast<int>(lodInstances[l].size());
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, instCount * sizeof(InstanceData), lodInstances[l].data());

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
    // Convert model matrices to InstanceData for shadow pass (prevModel = model, no motion needed)
    std::vector<InstanceData> shadowInstances(count);
    for (int i = 0; i < count; ++i) {
        shadowInstances[i].model = models[i];
        shadowInstances[i].prevModel = models[i];
    }
    return DrawInstanced(shadowInstances.data(), count, lightEye, lightVP, glm::radians(60.0f), 1024.0f, thresholdPx);
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
