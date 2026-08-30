#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <iostream>
#include "engine/Renderer/Shader.h"
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"

namespace cjoka {

class VXGI {
public:
    static VXGI& Get() {
        static VXGI s_instance;
        return s_instance;
    }

    VXGI(int resolution = 64) : m_res(resolution) {
        initVoxelGrid(m_res);
    }

    ~VXGI() {
        destroy();
    }

    void initVoxelGrid(int res) {
        m_res = res;
        if (m_voxelTex != 0) destroy();

        glGenTextures(1, &m_voxelTex);
        glBindTexture(GL_TEXTURE_3D, m_voxelTex);
        glTexStorage3D(GL_TEXTURE_3D, 6, GL_RGBA16F, m_res, m_res, m_res);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_3D, 0);

        std::cout << "[VXGI] Initialized 3D Voxel Texture " << m_res << "x" << m_res << "x" << m_res << " (6 mip levels)\n";
    }

    void clearVoxels() {
        if (!m_voxelTex) return;
        glBindTexture(GL_TEXTURE_3D, m_voxelTex);
        GLfloat clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        for (int level = 0; level < 6; ++level) {
            glClearTexImage(m_voxelTex, level, GL_RGBA, GL_FLOAT, clearColor);
        }
        glBindTexture(GL_TEXTURE_3D, 0);
    }

    void generateMipmaps() {
        if (!m_voxelTex) return;
        glBindTexture(GL_TEXTURE_3D, m_voxelTex);
        glGenerateMipmap(GL_TEXTURE_3D);
        glBindTexture(GL_TEXTURE_3D, 0);
    }

    void voxelizeScene(Registry& reg, const glm::vec3& camPos, const DirectionalLight* sun, int screenW, int screenH) {
        if (!m_enabled || m_voxelTex == 0) return;
        ensureVoxelizeShader();

        float voxelSize = m_gridExtent / float(m_res);
        glm::vec3 snappedCamPos = glm::floor((camPos + glm::vec3(voxelSize * 0.5f)) / voxelSize) * voxelSize;

        // Voxel grid hysteresis: only re-voxelize when camera moves more than half a voxel cell
        if (m_hasVoxelized && glm::distance(m_lastVoxelizedPos, snappedCamPos) < voxelSize * 0.5f) {
            return;
        }
        m_hasVoxelized = true;
        m_lastVoxelizedPos = snappedCamPos;
        m_gridCenter = snappedCamPos;
        clearVoxels();

        GLint prevFBO = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
        if (m_voxelFbo == 0) {
            glGenFramebuffers(1, &m_voxelFbo);
            glBindFramebuffer(GL_FRAMEBUFFER, m_voxelFbo);
            glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, m_res);
            glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, m_res);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, m_voxelFbo);
        }

        glViewport(0, 0, m_res, m_res);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glBindImageTexture(0, m_voxelTex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        m_voxelizeShader->use();
        m_voxelizeShader->setInt("uVoxelImage", 0);
        m_voxelizeShader->setVec3("uVoxelCenter", m_gridCenter);
        m_voxelizeShader->setFloat("uVoxelExtent", m_gridExtent);
        m_voxelizeShader->setInt("uVoxelResolution", m_res);

        glm::vec3 sunDir = sun ? normalize(-sun->direction) : glm::vec3(0.4f, 0.8f, 0.2f);
        glm::vec3 sunCol = sun ? (sun->color * sun->intensity) : glm::vec3(2.5f);
        m_voxelizeShader->setVec3("uSunDir", sunDir);
        m_voxelizeShader->setVec3("uSunColor", sunCol);

        for (Entity e : reg.view<Transform, MeshRenderer>()) {
            auto& mr = reg.get<MeshRenderer>(e);
            if (!mr.visible || !mr.mesh || mr.mesh->empty()) continue;
            auto& tr = reg.get<Transform>(e);

            glm::mat4 model = tr.matrix();
            m_voxelizeShader->setMat4("uModel", model);
            m_voxelizeShader->setVec3("uAlbedoColor", mr.material.albedo);
            m_voxelizeShader->setVec3("uEmissiveColor", mr.material.emissive);
            bool hasDiff = mr.material.useDiffuseMap && mr.material.diffuseMap && mr.material.diffuseMap->valid();
            m_voxelizeShader->setBool("uUseDiffuseMap", hasDiff);
            if (hasDiff) {
                mr.material.diffuseMap->bind(0);
                m_voxelizeShader->setInt("uDiffuseMap", 0);
            }

            mr.mesh->draw();
        }

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        generateMipmaps();

        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
        glViewport(0, 0, screenW, screenH);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, bufs);
    }

    void setWorldBounds(const glm::vec3& center, float extent) {
        m_gridCenter = center;
        m_gridExtent = extent;
    }

    void bindVoxelTexture(int slot = 7) const {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot));
        glBindTexture(GL_TEXTURE_3D, m_voxelTex);
    }

    GLuint voxelTexture() const { return m_voxelTex; }
    int resolution() const { return m_res; }
    const glm::vec3& gridCenter() const { return m_gridCenter; }
    float gridExtent() const { return m_gridExtent; }

    bool enabled() const { return m_enabled; }
    void setEnabled(bool e) { m_enabled = e; }
    float giIntensity() const { return m_giIntensity; }
    void setGIIntensity(float i) { m_giIntensity = i; }

private:
    void ensureVoxelizeShader() {
        if (m_voxelizeShader) return;

        const char* vs = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;

uniform mat4 uModel;

out VS_OUT {
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
} vs_out;

void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vs_out.worldPos = wp.xyz;
    vs_out.normal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vs_out.uv = aUV;
    gl_Position = wp;
}
)";

        const char* gs = R"(#version 460 core
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in VS_OUT {
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
} gs_in[];

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

uniform vec3 uVoxelCenter;
uniform float uVoxelExtent;

void main() {
    vec3 p0 = gs_in[0].worldPos;
    vec3 p1 = gs_in[1].worldPos;
    vec3 p2 = gs_in[2].worldPos;
    vec3 triNormal = normalize(cross(p1 - p0, p2 - p0));

    vec3 absN = abs(triNormal);
    int axis = 2; // Z
    if (absN.x > absN.y && absN.x > absN.z) axis = 0; // X
    else if (absN.y > absN.x && absN.y > absN.z) axis = 1; // Y

    vec3 minBounds = uVoxelCenter - vec3(uVoxelExtent * 0.5);

    for (int i = 0; i < 3; ++i) {
        vWorldPos = gs_in[i].worldPos;
        vNormal = gs_in[i].normal;
        vUV = gs_in[i].uv;

        vec3 normPos = (gs_in[i].worldPos - minBounds) / uVoxelExtent;
        vec3 clipPos = normPos * 2.0 - 1.0;

        if (axis == 0) {
            gl_Position = vec4(clipPos.y, clipPos.z, 0.0, 1.0);
        } else if (axis == 1) {
            gl_Position = vec4(clipPos.x, clipPos.z, 0.0, 1.0);
        } else {
            gl_Position = vec4(clipPos.x, clipPos.y, 0.0, 1.0);
        }
        EmitVertex();
    }
    EndPrimitive();
}
)";

        const char* fs = R"(#version 460 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

layout(rgba16f, binding = 0) uniform writeonly image3D uVoxelImage;

uniform vec3 uVoxelCenter;
uniform float uVoxelExtent;
uniform int uVoxelResolution;

uniform vec3 uAlbedoColor;
uniform vec3 uEmissiveColor;
uniform bool uUseDiffuseMap;
uniform sampler2D uDiffuseMap;

uniform vec3 uSunDir;
uniform vec3 uSunColor;

void main() {
    vec3 minBounds = uVoxelCenter - vec3(uVoxelExtent * 0.5);
    vec3 voxelNorm = (vWorldPos - minBounds) / uVoxelExtent;
    if (any(lessThan(voxelNorm, vec3(0.0))) || any(greaterThan(voxelNorm, vec3(1.0)))) return;

    ivec3 voxelCoord = ivec3(voxelNorm * float(uVoxelResolution));

    vec3 albedo = uAlbedoColor;
    if (uUseDiffuseMap) {
        albedo *= texture(uDiffuseMap, vUV).rgb;
    }

    float NdotL = max(dot(normalize(vNormal), normalize(uSunDir)), 0.0);
    vec3 directLight = albedo * (uSunColor * NdotL + vec3(0.2));
    vec3 totalRadiance = directLight + uEmissiveColor;

    imageStore(uVoxelImage, voxelCoord, vec4(totalRadiance, 1.0));
}
)";
        m_voxelizeShader = new Shader(vs, fs, gs);
    }

    void destroy() {
        if (m_voxelFbo != 0) {
            glDeleteFramebuffers(1, &m_voxelFbo);
            m_voxelFbo = 0;
        }
        if (m_voxelTex != 0) {
            glDeleteTextures(1, &m_voxelTex);
            m_voxelTex = 0;
        }
        if (m_voxelizeShader) {
            delete m_voxelizeShader;
            m_voxelizeShader = nullptr;
        }
    }

    int m_res = 64;
    GLuint m_voxelTex = 0;
    GLuint m_voxelFbo = 0;
    glm::vec3 m_gridCenter{0.0f};
    float m_gridExtent = 60.0f; // 60m cube around camera
    bool m_enabled = false;
    float m_giIntensity = 1.0f;
    bool m_hasVoxelized = false;
    glm::vec3 m_lastVoxelizedPos{1e9f};
    Shader* m_voxelizeShader = nullptr;
};

} // namespace cjoka
