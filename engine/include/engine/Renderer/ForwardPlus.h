#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>
#include "engine/Renderer/Shader.h"

// Structs matching the SSBOs
struct PointLightData {
    glm::vec4 positionAndRadius; // xyz = position, w = radius
    glm::vec4 colorAndIntensity; // xyz = color, w = intensity
};

class ForwardPlus {
public:
    static const int GRID_SIZE_X = 16;
    static const int GRID_SIZE_Y = 9;
    static const int GRID_SIZE_Z = 24;
    static const int MAX_LIGHTS_PER_CLUSTER = 127;
    static const int MAX_LIGHTS = 1024;

    ForwardPlus() = default;
    void init();
    void cullLights(const glm::mat4& view, const glm::mat4& proj, const std::vector<PointLightData>& lights, float screenW, float screenH);
    
    void bindBuffers(int lightBinding, int clusterBinding) const;

private:
    GLuint m_lightSSBO = 0;
    GLuint m_clusterSSBO = 0;
    Shader* m_cullShader = nullptr;
};
