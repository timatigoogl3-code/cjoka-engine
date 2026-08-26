#include "engine/Renderer/CascadedShadowMap.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>

void CascadedShadowMap::init(int size) {
    m_size = size;
    if (m_fbo) return;

    glGenFramebuffers(1, &m_fbo);
    glGenTextures(1, &m_depthArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthArray);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, m_size, m_size, CASCADE_COUNT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthArray, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_cascadeSplits = { 0.1f, 15.0f, 45.0f, 140.0f };
    m_lightMatrices.resize(CASCADE_COUNT, glm::mat4(1.0f));
}

void CascadedShadowMap::shutdown() {
    if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
    if (m_depthArray) { glDeleteTextures(1, &m_depthArray); m_depthArray = 0; }
}

void CascadedShadowMap::calculateCascadeSplits(float nearPlane, float farPlane, float lambda) {
    m_cascadeSplits.resize(CASCADE_COUNT + 1);
    m_cascadeSplits[0] = nearPlane;
    m_cascadeSplits[CASCADE_COUNT] = farPlane;

    for (int i = 1; i < CASCADE_COUNT; ++i) {
        float si = float(i) / float(CASCADE_COUNT);
        float logSplit = nearPlane * std::pow(farPlane / nearPlane, si);
        float uniformSplit = nearPlane + (farPlane - nearPlane) * si;
        m_cascadeSplits[i] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
    }
}

std::vector<glm::mat4> CascadedShadowMap::calculateLightMatrices(const glm::mat4& camView, const glm::mat4& camProj, const glm::vec3& lightDir) {
    if (!m_fbo) init(m_size);

    m_lightMatrices.resize(CASCADE_COUNT);
    glm::mat4 invCam = glm::inverse(camProj * camView);
    glm::vec3 normLightDir = glm::normalize(lightDir);

    for (int c = 0; c < CASCADE_COUNT; ++c) {
        float nearZ = m_cascadeSplits[c];
        float farZ = m_cascadeSplits[c + 1];

        // 8 frustum corners in NDC
        // We calculate near/far slice in projection space
        // Using camera near/far
        float camNear = m_cascadeSplits[0];
        float camFar = m_cascadeSplits[CASCADE_COUNT];

        float nNDC = (nearZ - camNear) / (camFar - camNear) * 2.0f - 1.0f;
        float fNDC = (farZ - camNear) / (camFar - camNear) * 2.0f - 1.0f;

        glm::vec4 cornersNDC[8] = {
            {-1.0f, -1.0f, -1.0f, 1.0f},
            { 1.0f, -1.0f, -1.0f, 1.0f},
            {-1.0f,  1.0f, -1.0f, 1.0f},
            { 1.0f,  1.0f, -1.0f, 1.0f},
            {-1.0f, -1.0f,  1.0f, 1.0f},
            { 1.0f, -1.0f,  1.0f, 1.0f},
            {-1.0f,  1.0f,  1.0f, 1.0f},
            { 1.0f,  1.0f,  1.0f, 1.0f},
        };

        // Simpler way: build custom sub-frustum
        float aspect = camProj[1][1] / camProj[0][0];
        float fovRad = 2.0f * std::atan(1.0f / camProj[1][1]);
        float tanHalfFOV = std::tan(fovRad / 2.0f);

        float xn = nearZ * tanHalfFOV * aspect;
        float yn = nearZ * tanHalfFOV;
        float xf = farZ * tanHalfFOV * aspect;
        float yf = farZ * tanHalfFOV;

        glm::vec4 frustumCornersView[8] = {
            {-xn, -yn, -nearZ, 1.0f},
            { xn, -yn, -nearZ, 1.0f},
            {-xn,  yn, -nearZ, 1.0f},
            { xn,  yn, -nearZ, 1.0f},
            {-xf, -yf, -farZ,  1.0f},
            { xf, -yf, -farZ,  1.0f},
            {-xf,  yf, -farZ,  1.0f},
            { xf,  yf, -farZ,  1.0f},
        };

        glm::mat4 invView = glm::inverse(camView);
        glm::vec3 center(0.0f);

        for (int i = 0; i < 8; ++i) {
            glm::vec4 worldCorner = invView * frustumCornersView[i];
            center += glm::vec3(worldCorner);
        }
        center /= 8.0f;

        float radius = 0.0f;
        for (int i = 0; i < 8; ++i) {
            glm::vec4 worldCorner = invView * frustumCornersView[i];
            radius = std::max(radius, glm::distance(center, glm::vec3(worldCorner)));
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        glm::vec3 lightPos = center - normLightDir * (radius * 2.0f);
        glm::mat4 lightView = glm::lookAt(lightPos, center, glm::vec3(0.0f, 1.0f, 0.0f));

        // Texel stabilization to eliminate shimmering
        float shadowResolution = float(m_size);
        float worldUnitsPerTexel = (2.0f * radius) / shadowResolution;
        glm::vec4 shadowOrigin = lightView * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        float snappedX = std::floor(shadowOrigin.x / worldUnitsPerTexel) * worldUnitsPerTexel;
        float snappedY = std::floor(shadowOrigin.y / worldUnitsPerTexel) * worldUnitsPerTexel;
        float diffX = snappedX - shadowOrigin.x;
        float diffY = snappedY - shadowOrigin.y;

        glm::mat4 lightProj = glm::ortho(-radius + diffX, radius + diffX, -radius + diffY, radius + diffY, 0.0f, radius * 4.0f);
        m_lightMatrices[c] = lightProj * lightView;
    }

    return m_lightMatrices;
}

void CascadedShadowMap::beginCascade(int cascadeIndex, const glm::mat4& lightMatrix) {
    if (!m_fbo) init(m_size);

    if (cascadeIndex == 0) {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_prevFbo);
        glGetIntegerv(GL_VIEWPORT, m_prevViewport);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthArray, 0, cascadeIndex);
    glViewport(0, 0, m_size, m_size);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClear(GL_DEPTH_BUFFER_BIT);

    glCullFace(GL_FRONT);
}

void CascadedShadowMap::end() {
    glCullFace(GL_BACK);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, m_prevFbo);
    glViewport(m_prevViewport[0], m_prevViewport[1], m_prevViewport[2], m_prevViewport[3]);
}

void CascadedShadowMap::bind(int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthArray);
}
