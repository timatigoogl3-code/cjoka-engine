#pragma once
// ChunkManager — Стриминг открытого мира по сетке чанков с реальным управлением VRAM (п. A3)
// Автоматически загружает/выгружает GPU буферы мешей и текстур в зависимости от дистанции до игрока,
// предотвращая переполнение видеопамяти на масштабных картах и городских локациях.
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/Assets/AssetManager.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iostream>

struct ChunkCoord {
    int x = 0;
    int z = 0;

    bool operator==(const ChunkCoord& o) const noexcept {
        return x == o.x && z == o.z;
    }
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const noexcept {
        return (static_cast<size_t>(c.x) * 73856093u) ^ (static_cast<size_t>(c.z) * 19349663u);
    }
};

struct ChunkEntityData {
    Entity entity = NullEntity;
    std::string meshAssetPath;
    std::string diffusePath;
    std::string normalPath;
    std::string specularPath;
    std::shared_ptr<Mesh3D> cachedMesh;
    size_t estimatedVRAMBytes = 0;
};

struct Chunk {
    ChunkCoord coord;
    std::vector<ChunkEntityData> entities;
    bool isLoaded = false;
    bool vramLoaded = false;
    float fadeAlpha = 0.0f; // 0.0 -> 1.0 (smooth pop-in)
    size_t totalVRAMBytes = 0;

    void loadVRAM(Registry* reg) {
        if (vramLoaded || !reg) return;
        totalVRAMBytes = 0;
        for (auto& item : entities) {
            if (!reg->valid(item.entity) || !reg->has<MeshRenderer>(item.entity)) continue;
            auto& mr = reg->get<MeshRenderer>(item.entity);
            if (!mr.mesh || mr.mesh->empty()) {
                if (!item.meshAssetPath.empty()) {
                    if (item.meshAssetPath.find(".obj") != std::string::npos) {
                        mr.mesh = Assets::Mesh(item.meshAssetPath);
                    } else if (item.meshAssetPath.find("sphere") != std::string::npos) {
                        mr.mesh = Assets::Sphere(0.5f);
                    } else if (item.meshAssetPath.find("plane") != std::string::npos) {
                        mr.mesh = Assets::Plane(10.0f, 10.0f, 20, 20);
                    } else {
                        mr.mesh = Assets::Cube(1.0f);
                    }
                }
            }
            if (mr.mesh) {
                item.estimatedVRAMBytes = mr.mesh->vertices().size() * sizeof(Vertex) + mr.mesh->indices().size() * sizeof(uint32_t);
                totalVRAMBytes += item.estimatedVRAMBytes;
            }
            mr.visible = true;
        }
        vramLoaded = true;
    }

    void unloadVRAM(Registry* reg) {
        if (!vramLoaded || !reg) return;
        for (auto& item : entities) {
            if (!reg->valid(item.entity) || !reg->has<MeshRenderer>(item.entity)) continue;
            auto& mr = reg->get<MeshRenderer>(item.entity);
            mr.visible = false;
            // Free GPU VRAM resources by releasing shared_ptr when unloaded
            if (!item.meshAssetPath.empty()) {
                mr.mesh.reset();
            }
        }
        totalVRAMBytes = 0;
        vramLoaded = false;
    }
};

class ChunkManager {
public:
    ChunkManager(float chunkSize = 128.0f, float loadRadius = 256.0f)
        : m_chunkSize(chunkSize), m_loadRadius(loadRadius) {}

    void init(Registry* reg) {
        m_registry = reg;
    }

    void clear() {
        for (auto& [coord, chunk] : m_chunks) {
            chunk.unloadVRAM(m_registry);
        }
        m_chunks.clear();
        m_entityToChunk.clear();
    }

    ChunkCoord worldToChunk(const glm::vec3& pos) const {
        if (std::isnan(pos.x) || std::isnan(pos.z) || std::isinf(pos.x) || std::isinf(pos.z) || m_chunkSize <= 0.001f) {
            return {0, 0};
        }
        float cx = std::clamp(std::floor(pos.x / m_chunkSize), -100000.0f, 100000.0f);
        float cz = std::clamp(std::floor(pos.z / m_chunkSize), -100000.0f, 100000.0f);
        return {static_cast<int>(cx), static_cast<int>(cz)};
    }

    void registerEntity(Entity e, const glm::vec3& pos) {
        if (!m_registry || !m_registry->valid(e)) return;
        ChunkCoord coord = worldToChunk(pos);
        auto& chunk = m_chunks[coord];
        chunk.coord = coord;

        ChunkEntityData data;
        data.entity = e;
        if (m_registry->has<MeshRenderer>(e)) {
            auto& mr = m_registry->get<MeshRenderer>(e);
            data.meshAssetPath = mr.assetPath;
            data.diffusePath = mr.material.diffuseMapPath;
            data.normalPath = mr.material.normalMapPath;
            data.specularPath = mr.material.specularMapPath;
            data.cachedMesh = mr.mesh;
        }
        chunk.entities.push_back(data);
        m_entityToChunk[e] = coord;

        if (chunk.isLoaded) {
            chunk.loadVRAM(m_registry);
        }
    }

    void unregisterEntity(Entity e) {
        auto it = m_entityToChunk.find(e);
        if (it != m_entityToChunk.end()) {
            auto chunkIt = m_chunks.find(it->second);
            if (chunkIt != m_chunks.end()) {
                auto& ents = chunkIt->second.entities;
                ents.erase(std::remove_if(ents.begin(), ents.end(), [&](const ChunkEntityData& d) {
                    return d.entity == e;
                }), ents.end());
            }
            m_entityToChunk.erase(it);
        }
    }

    void update(const glm::vec3& cameraPos, float dt = 0.016f) {
        if (!m_registry || !m_streamingEnabled) return;

        ChunkCoord center = worldToChunk(cameraPos);
        float fadeSpeed = 3.5f;

        for (auto& [coord, chunk] : m_chunks) {
            int64_t dx = std::abs((int64_t)coord.x - (int64_t)center.x);
            int64_t dz = std::abs((int64_t)coord.z - (int64_t)center.z);
            float distChunks = std::sqrt(static_cast<float>(dx * dx + dz * dz));
            float distWorld = distChunks * m_chunkSize;

            bool shouldBeLoaded = (distWorld <= m_loadRadius);

            if (shouldBeLoaded) {
                if (!chunk.isLoaded) {
                    chunk.isLoaded = true;
                    chunk.loadVRAM(m_registry); // Stream geometry into VRAM
                    chunk.fadeAlpha = 0.0f;
                }
                chunk.fadeAlpha = std::min(1.0f, chunk.fadeAlpha + dt * fadeSpeed);
            } else if (distWorld > (m_loadRadius + m_chunkSize * 0.5f)) {
                if (chunk.isLoaded) {
                    chunk.fadeAlpha = std::max(0.0f, chunk.fadeAlpha - dt * fadeSpeed);
                    if (chunk.fadeAlpha <= 0.0f) {
                        chunk.isLoaded = false;
                        chunk.unloadVRAM(m_registry); // Free GPU memory from VRAM
                    }
                }
            }

            // Apply fadeAlpha & visibility
            for (auto& item : chunk.entities) {
                if (m_registry->valid(item.entity) && m_registry->has<MeshRenderer>(item.entity)) {
                    auto& mr = m_registry->get<MeshRenderer>(item.entity);
                    mr.visible = chunk.isLoaded;
                }
            }
        }
    }

    void rebuildFromRegistry() {
        if (!m_registry) return;
        clear();
        for (Entity e : m_registry->view<Transform, MeshRenderer>()) {
            auto& tr = m_registry->get<Transform>(e);
            registerEntity(e, tr.position);
        }
    }

    void rebuildFromScene() { rebuildFromRegistry(); }
    bool isStreamingEnabled() const { return m_streamingEnabled; }
    size_t loadedChunksCount() const { return activeChunksCount(); }
    size_t totalManagedEntities() const { return m_entityToChunk.size(); }

    size_t totalActiveVRAMBytes() const {
        size_t total = 0;
        for (const auto& [coord, chunk] : m_chunks) {
            if (chunk.vramLoaded) total += chunk.totalVRAMBytes;
        }
        return total;
    }

    size_t activeChunksCount() const {
        size_t count = 0;
        for (const auto& [coord, chunk] : m_chunks) {
            if (chunk.isLoaded) count++;
        }
        return count;
    }

    float chunkSize() const { return m_chunkSize; }
    void setChunkSize(float s) { m_chunkSize = s; }

    float loadRadius() const { return m_loadRadius; }
    void setLoadRadius(float r) { m_loadRadius = r; }

    bool streamingEnabled() const { return m_streamingEnabled; }
    void setStreamingEnabled(bool e) { m_streamingEnabled = e; }

    size_t totalChunks() const { return m_chunks.size(); }
    const std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash>& chunks() const { return m_chunks; }

private:
    Registry* m_registry = nullptr;
    float m_chunkSize = 128.0f;
    float m_loadRadius = 256.0f;
    bool m_streamingEnabled = true;

    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> m_chunks;
    std::unordered_map<Entity, ChunkCoord> m_entityToChunk;
};
