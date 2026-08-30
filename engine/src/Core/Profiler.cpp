#include "engine/Core/Profiler.h"
#include <algorithm>

namespace cjoka {

Profiler::Profiler() {
    // Initial estimation of system components
    m_vram.vxgiVRAM = 64 * 64 * 64 * 8; // ~2MB for 3D clipmap
}

Profiler::~Profiler() {
    for (auto& [name, q] : m_gpuQueries) {
        if (q.queryIds[0]) glDeleteQueries(2, q.queryIds);
    }
    m_gpuQueries.clear();
}

void Profiler::checkVRAM() {
    if (!m_warnedVRAM && m_vram.usagePercentage() > 85.0) {
        std::cerr << "[VRAM Guard WARNING] GPU memory allocation is high: "
                  << m_vram.totalMB() << " MB / "
                  << (m_vram.totalBudget / (1024 * 1024)) << " MB ("
                  << m_vram.usagePercentage() << "%). Approaching RTX 3050 limit!\n";
        m_warnedVRAM = true;
    }
}

void Profiler::beginFrame() {
    m_lastFrameStats = m_currentStats;
    m_currentStats.reset();

    // Read back previous GPU query results (double-buffered, no sync stall)
    int prevBuffer = 1 - m_bufferIndex;
    m_gpuResults.clear();
    m_totalGPUTimeMs = 0.0;

    for (const auto& name : m_gpuOrder) {
        auto& q = m_gpuQueries[name];
        if (q.issued[prevBuffer]) {
            GLuint64 elapsedNs = 0;
            GLint available = 0;
            glGetQueryObjectiv(q.queryIds[prevBuffer], GL_QUERY_RESULT_AVAILABLE, &available);
            if (available) {
                glGetQueryObjectui64v(q.queryIds[prevBuffer], GL_QUERY_RESULT, &elapsedNs);
                q.lastResultMs = static_cast<double>(elapsedNs) / 1000000.0;
            }
            q.issued[prevBuffer] = false;
        }
        m_gpuResults.push_back({name, q.lastResultMs});
        m_totalGPUTimeMs += q.lastResultMs;
    }

    // CPU results
    m_cpuResults.clear();
    m_totalCPUTimeMs = 0.0;
    for (const auto& name : m_cpuOrder) {
        double ms = m_cpuAccumulated[name];
        m_cpuResults.push_back({name, ms});
        m_totalCPUTimeMs += ms;
    }
    m_cpuAccumulated.clear();
    m_cpuScopes.clear();

    m_bufferIndex = 1 - m_bufferIndex;
}

void Profiler::endFrame() {
}

void Profiler::beginGPUZone(const std::string& name) {
    auto& q = m_gpuQueries[name];
    if (!q.queryIds[0]) {
        glGenQueries(2, q.queryIds);
        m_gpuOrder.push_back(name);
    }
    glBeginQuery(GL_TIME_ELAPSED, q.queryIds[m_bufferIndex]);
    q.issued[m_bufferIndex] = true;
}

void Profiler::endGPUZone(const std::string& name) {
    glEndQuery(GL_TIME_ELAPSED);
}

void Profiler::beginCPUZone(const std::string& name) {
    if (std::find(m_cpuOrder.begin(), m_cpuOrder.end(), name) == m_cpuOrder.end()) {
        m_cpuOrder.push_back(name);
    }
    m_cpuScopes[name].start = std::chrono::high_resolution_clock::now();
}

void Profiler::endCPUZone(const std::string& name) {
    auto it = m_cpuScopes.find(name);
    if (it != m_cpuScopes.end()) {
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - it->second.start).count();
        m_cpuAccumulated[name] += ms;
    }
}

} // namespace cjoka
