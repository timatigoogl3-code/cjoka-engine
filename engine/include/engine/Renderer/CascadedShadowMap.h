#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>

class CascadedShadowMap {
public:
    static const int CASCADE_COUNT = 3;

    CascadedShadowMap() = default;
    ~CascadedShadowMap() { shutdown(); }

    void init(int size = 2048);
    void shutdown();

    void calculateCascadeSplits(float nearPlane, float farPlane, float lambda = 0.85f);
    std::vector<glm::mat4> calculateLightMatrices(const glm::mat4& camView, const glm::mat4& camProj, const glm::vec3& lightDir);

    void beginCascade(int cascadeIndex, const glm::mat4& lightMatrix);
    void end();

    void bind(int slot) const;

    const std::vector<float>& cascadeSplits() const { return m_cascadeSplits; }
    const std::vector<glm::mat4>& lightMatrices() const { return m_lightMatrices; }
    bool ready() const { return m_depthArray != 0; }
    int size() const { return m_size; }

private:
    GLuint m_fbo = 0;
    GLuint m_depthArray = 0;
    int m_size = 2048;
    std::vector<float> m_cascadeSplits;
    std::vector<glm::mat4> m_lightMatrices;
    GLint m_prevFbo = 0;
    GLint m_prevViewport[4] = {0, 0, 0, 0};
    GLboolean m_prevCullFace = GL_FALSE;
    GLint m_prevCullMode = GL_BACK;
};
