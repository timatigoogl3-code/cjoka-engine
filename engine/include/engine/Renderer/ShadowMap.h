#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>

// ShadowMap — карта глубины для направленного света (солнце).
class ShadowMap {
public:
    void Init(int size = 2048);
    void Shutdown();

    // Начать рендер теней: биндит FBO, вьюпорт, чистит глубину
    void Begin(const glm::mat4& lightMatrix);
    // Закончить: вернуть вьюпорт по умолчанию
    void End(int fbW, int fbH);
    // Забиндить глубину в слот и вернуть матрицу света
    void Bind(int slot) const;
    const glm::mat4& matrix() const { return m_lightMatrix; }
    bool ready() const { return m_depth != 0; }
    int size() const { return m_size; }

private:
    GLuint m_fbo = 0, m_depth = 0;
    GLint m_prevFbo = 0;               // FBO до теней (HDR) — вернуть в End
    GLint m_prevViewport[4] = {0,0,0,0};
    int m_size = 2048;
    glm::mat4 m_lightMatrix{1};
};
