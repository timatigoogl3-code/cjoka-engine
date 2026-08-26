#include "engine/Renderer/ForwardPlus.h"
#include <iostream>

const char* kCullComputeShader = R"(
#version 460 core

layout(local_size_x = 16, local_size_y = 9, local_size_z = 1) in;

struct PointLight {
    vec4 positionAndRadius;
    vec4 colorAndIntensity;
};

struct Cluster {
    uint lightCount;
    uint lightIndices[127];
};

layout(std430, binding = 0) readonly buffer LightBuffer {
    uint uNumLights;
    PointLight lights[];
};

layout(std430, binding = 1) writeonly buffer ClusterBuffer {
    Cluster clusters[];
};

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uInverseProj;
uniform vec2 uScreenSize;
uniform float uZNear;
uniform float uZFar;

// Grid size is hardcoded to 16x9x24
const uvec3 GRID_SIZE = uvec3(16, 9, 24);

// Convert screen/depth coords to view space
vec4 clipToView(vec4 clip) {
    vec4 view = uInverseProj * clip;
    view /= view.w;
    return view;
}

// Linearly/logarithmically convert Z to slice
uint zToSlice(float zView) {
    // Logarithmic depth slicing
    float slice = log(-zView / uZNear) * float(GRID_SIZE.z) / log(uZFar / uZNear);
    return uint(clamp(int(slice), 0, int(GRID_SIZE.z) - 1));
}

void main() {
    uint clusterIndex = gl_GlobalInvocationID.x +
                        gl_GlobalInvocationID.y * GRID_SIZE.x +
                        gl_GlobalInvocationID.z * (GRID_SIZE.x * GRID_SIZE.y);
                        
    if (gl_GlobalInvocationID.x >= GRID_SIZE.x || 
        gl_GlobalInvocationID.y >= GRID_SIZE.y || 
        gl_GlobalInvocationID.z >= GRID_SIZE.z) return;

    // 1. Calculate AABB for this cluster in View Space
    vec2 tileSize = uScreenSize / vec2(GRID_SIZE.xy);
    
    vec4 minScreen = vec4(vec2(gl_GlobalInvocationID.xy) * tileSize, 0.0, 1.0);
    vec4 maxScreen = vec4(vec2(gl_GlobalInvocationID.xy + 1) * tileSize, 0.0, 1.0);
    
    // Convert to NDC [-1, 1]
    vec2 minNDC = (minScreen.xy / uScreenSize) * 2.0 - 1.0;
    vec2 maxNDC = (maxScreen.xy / uScreenSize) * 2.0 - 1.0;
    
    // Depth slice bounds in view space (logarithmic)
    float tileNearZ = -uZNear * pow(uZFar / uZNear, float(gl_GlobalInvocationID.z) / float(GRID_SIZE.z));
    float tileFarZ  = -uZNear * pow(uZFar / uZNear, float(gl_GlobalInvocationID.z + 1) / float(GRID_SIZE.z));
    
    // 4 corners of the tile at NearZ
    vec3 v0 = clipToView(vec4(minNDC.x, minNDC.y, -1.0, 1.0)).xyz;
    vec3 v1 = clipToView(vec4(maxNDC.x, minNDC.y, -1.0, 1.0)).xyz;
    vec3 v2 = clipToView(vec4(minNDC.x, maxNDC.y, -1.0, 1.0)).xyz;
    vec3 v3 = clipToView(vec4(maxNDC.x, maxNDC.y, -1.0, 1.0)).xyz;
    
    // Scale by tileNearZ and tileFarZ
    // Because clipToView gives us a vector on the z=-1 plane in view space (if proj is standard)
    // Wait, the standard way is to find the view ray for each corner.
    vec3 ray0 = v0 * (tileNearZ / v0.z);
    vec3 ray1 = v1 * (tileNearZ / v1.z);
    vec3 ray2 = v2 * (tileNearZ / v2.z);
    vec3 ray3 = v3 * (tileNearZ / v3.z);
    
    vec3 ray0Far = v0 * (tileFarZ / v0.z);
    vec3 ray1Far = v1 * (tileFarZ / v1.z);
    vec3 ray2Far = v2 * (tileFarZ / v2.z);
    vec3 ray3Far = v3 * (tileFarZ / v3.z);
    
    vec3 minAABB = min(min(min(ray0, ray1), min(ray2, ray3)), 
                       min(min(ray0Far, ray1Far), min(ray2Far, ray3Far)));
    vec3 maxAABB = max(max(max(ray0, ray1), max(ray2, ray3)), 
                       max(max(ray0Far, ray1Far), max(ray2Far, ray3Far)));
                       
    // 2. Cull lights against AABB
    uint lightCount = 0;
    uint visibleLights[127];
    
    uint numLights = min(uNumLights, 1024u);
    for (uint i = 0; i < numLights && lightCount < 127; ++i) {
        vec3 lightPosWorld = lights[i].positionAndRadius.xyz;
        float radius = lights[i].positionAndRadius.w;
        
        vec3 lightPosView = (uView * vec4(lightPosWorld, 1.0)).xyz;
        
        // Closest point on AABB to light
        vec3 closest = clamp(lightPosView, minAABB, maxAABB);
        vec3 diff = closest - lightPosView;
        float distSq = dot(diff, diff);
        
        if (distSq <= radius * radius) {
            visibleLights[lightCount++] = i;
        }
    }
    
    // 3. Write to SSBO
    clusters[clusterIndex].lightCount = lightCount;
    for (uint i = 0; i < lightCount; ++i) {
        clusters[clusterIndex].lightIndices[i] = visibleLights[i];
    }
}
)";

void ForwardPlus::init() {
    m_cullShader = new Shader(kCullComputeShader);
    
    glGenBuffers(1, &m_lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightSSBO);
    // uint count + padding + 1024 lights
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * 4 + sizeof(PointLightData) * MAX_LIGHTS, nullptr, GL_DYNAMIC_DRAW);
    
    glGenBuffers(1, &m_clusterSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_clusterSSBO);
    // cluster size = uint + 127 uints = 128 uints = 512 bytes.
    // 16 * 9 * 24 = 3456 clusters. 3456 * 512 = 1.76 MB.
    size_t clusterBufferSize = GRID_SIZE_X * GRID_SIZE_Y * GRID_SIZE_Z * (sizeof(uint) * 128);
    glBufferData(GL_SHADER_STORAGE_BUFFER, clusterBufferSize, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ForwardPlus::cullLights(const glm::mat4& view, const glm::mat4& proj, const std::vector<PointLightData>& lights, float screenW, float screenH) {
    // 1. Upload lights to SSBO
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightSSBO);
    uint32_t numLights = std::min((uint32_t)lights.size(), (uint32_t)MAX_LIGHTS);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &numLights);
    if (numLights > 0) {
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t) * 4, numLights * sizeof(PointLightData), lights.data());
    }
    
    // 2. Dispatch compute shader
    m_cullShader->use();
    m_cullShader->setMat4("uView", view);
    m_cullShader->setMat4("uProj", proj);
    m_cullShader->setMat4("uInverseProj", glm::inverse(proj));
    m_cullShader->setVec2("uScreenSize", glm::vec2(screenW, screenH));
    
    // Extrapolate Near/Far from projection matrix
    // proj[3][2] = -2nf / (f-n)
    // proj[2][2] = -(f+n) / (f-n)
    float nearPlane = proj[3][2] / (proj[2][2] - 1.0f);
    float farPlane = proj[3][2] / (proj[2][2] + 1.0f);
    m_cullShader->setFloat("uZNear", nearPlane);
    m_cullShader->setFloat("uZFar", farPlane);
    
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_lightSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_clusterSSBO);
    
    glDispatchCompute(1, 1, GRID_SIZE_Z);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void ForwardPlus::bindBuffers(int lightBinding, int clusterBinding) const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, lightBinding, m_lightSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, clusterBinding, m_clusterSSBO);
}
