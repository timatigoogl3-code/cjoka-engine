#pragma once
// Skybox — HDR cubemap / процедурный градиент. Один вызов: Skybox::Draw(view,proj)
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>

class Skybox {
public:
    Skybox() = default;
    // hdrPath — equirectangular .hdr (если nullptr — процедурный градиент)
    explicit Skybox(const std::string& hdrPath);
    ~Skybox();

    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;
    Skybox(Skybox&&) noexcept;
    Skybox& operator=(Skybox&&) noexcept;

    void draw(const glm::mat4& view, const glm::mat4& proj) const;
    bool valid() const { return m_vao != 0; }
    bool isHDR() const { return m_isHDR; }

    // Процедурный HDR градиент (без текстуры) — как в RAGE небо
    static void DrawProcedural(const glm::mat4& view, const glm::mat4& proj, float time = 0.0f);

private:
    void createCube();
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    GLuint m_cubemap = 0; // 0 = процедурный
    bool m_isHDR = false;
};
