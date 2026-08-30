#pragma once
#include <glad/gl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <iostream>

namespace cjoka {

struct GPUTimerResult {
    std::string name;
    double timeMs = 0.0;
};

struct CPUTimerResult {
    std::string name;
    double timeMs = 0.0;
};

struct RenderStats {
    uint32_t totalDrawCalls = 0;
    uint32_t instancedDrawCalls = 0;
    uint32_t clusterDrawCalls = 0;
    uint32_t drawnClusters = 0;
    uint32_t totalTriangles = 0;
    uint32_t clusterTrianglesLod0 = 0;
    uint32_t savedClusterTriangles = 0;

    void reset() {
        totalDrawCalls = 0;
        instancedDrawCalls = 0;
        clusterDrawCalls = 0;
        drawnClusters = 0;
        totalTriangles = 0;
        clusterTrianglesLod0 = 0;
        savedClusterTriangles = 0;
    }
};

struct VRAMUsage {
    size_t meshVRAM = 0;
    size_t textureVRAM = 0;
    size_t fboVRAM = 0;
    size_t vxgiVRAM = 0;
    size_t totalBudget = 6ULL * 1024ULL * 1024ULL * 1024ULL; // 6 GB default (RTX 3050)

    size_t total() const {
        return meshVRAM + textureVRAM + fboVRAM + vxgiVRAM;
    }

    double totalMB() const {
        return static_cast<double>(total()) / (1024.0 * 1024.0);
    }

    double totalGB() const {
        return static_cast<double>(total()) / (1024.0 * 1024.0 * 1024.0);
    }

    double usagePercentage() const {
        return (totalBudget > 0) ? (static_cast<double>(total()) / static_cast<double>(totalBudget)) * 100.0 : 0.0;
    }
};

class Profiler {
public:
    static Profiler& Get() {
        static Profiler s_instance;
        return s_instance;
    }

    Profiler();
    ~Profiler();

    void beginFrame();
    void endFrame();

    // GPU Query Zone management
    void beginGPUZone(const std::string& name);
    void endGPUZone(const std::string& name);

    // CPU Zone management
    void beginCPUZone(const std::string& name);
    void endCPUZone(const std::string& name);

    // Stats & VRAM
    RenderStats& stats() { return m_currentStats; }
    const RenderStats& stats() const { return m_lastFrameStats; }
    VRAMUsage& vram() { return m_vram; }
    const VRAMUsage& vram() const { return m_vram; }

    const std::vector<GPUTimerResult>& gpuTimers() const { return m_gpuResults; }
    const std::vector<CPUTimerResult>& cpuTimers() const { return m_cpuResults; }
    double totalGPUTimeMs() const { return m_totalGPUTimeMs; }
    double totalCPUTimeMs() const { return m_totalCPUTimeMs; }

    void registerMeshVRAM(size_t bytes) { m_vram.meshVRAM += bytes; checkVRAM(); }
    void unregisterMeshVRAM(size_t bytes) { if (m_vram.meshVRAM >= bytes) m_vram.meshVRAM -= bytes; }
    void registerTextureVRAM(size_t bytes) { m_vram.textureVRAM += bytes; checkVRAM(); }
    void unregisterTextureVRAM(size_t bytes) { if (m_vram.textureVRAM >= bytes) m_vram.textureVRAM -= bytes; }
    void registerFBOVRAM(size_t bytes) { m_vram.fboVRAM += bytes; checkVRAM(); }
    void unregisterFBOVRAM(size_t bytes) { if (m_vram.fboVRAM >= bytes) m_vram.fboVRAM -= bytes; }
    void setVXGIVRAM(size_t bytes) { m_vram.vxgiVRAM = bytes; checkVRAM(); }

private:
    void checkVRAM();

    struct GPUQueryPair {
        GLuint queryIds[2] = {0, 0}; // double buffered for async fetch
        bool issued[2] = {false, false};
        double lastResultMs = 0.0;
    };

    struct CPUScope {
        std::chrono::high_resolution_clock::time_point start;
    };

    int m_bufferIndex = 0;
    std::unordered_map<std::string, GPUQueryPair> m_gpuQueries;
    std::vector<std::string> m_gpuOrder;
    std::unordered_map<std::string, CPUScope> m_cpuScopes;
    std::unordered_map<std::string, double> m_cpuAccumulated;
    std::vector<std::string> m_cpuOrder;

    std::vector<GPUTimerResult> m_gpuResults;
    std::vector<CPUTimerResult> m_cpuResults;
    double m_totalGPUTimeMs = 0.0;
    double m_totalCPUTimeMs = 0.0;

    RenderStats m_currentStats;
    RenderStats m_lastFrameStats;
    VRAMUsage m_vram;
    bool m_warnedVRAM = false;
};

// RAII Scopes for profiling
class GPUZoneScope {
public:
    explicit GPUZoneScope(const std::string& name) : m_name(name) {
        Profiler::Get().beginGPUZone(m_name);
    }
    ~GPUZoneScope() {
        Profiler::Get().endGPUZone(m_name);
    }
private:
    std::string m_name;
};

class CPUZoneScope {
public:
    explicit CPUZoneScope(const std::string& name) : m_name(name) {
        Profiler::Get().beginCPUZone(m_name);
    }
    ~CPUZoneScope() {
        Profiler::Get().endCPUZone(m_name);
    }
private:
    std::string m_name;
};

#define CJOKA_PROFILE_GPU(name) ::cjoka::GPUZoneScope _gpuZone##__LINE__(name)
#define CJOKA_PROFILE_CPU(name) ::cjoka::CPUZoneScope _cpuZone##__LINE__(name)

} // namespace cjoka
