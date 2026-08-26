#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>

// Framebuffer — HDR ready (RGBA16F + depth)
// Использование: Framebuffer fb(w,h,true); fb.bind(); ... fb.unbind(); fb.blitToDefault();
class Framebuffer {
public:
    Framebuffer() = default;
    Framebuffer(int w, int h, bool hdr = true, bool withDepth = true);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& o) noexcept;
    Framebuffer& operator=(Framebuffer&& o) noexcept;

    void create(int w, int h, bool hdr = true, bool withDepth = true);
    void destroy();
    void bind() const;
    void unbind() const; // bind default
    void resize(int w, int h);
    void blitToDefault() const; // GL_NEAREST

    GLuint fbo() const { return m_fbo; }
    GLuint colorTexture() const { return m_color; }
    GLuint depthTexture() const { return m_depth; }
    GLuint depthRBO() const { return m_depth; }
    int width() const { return m_w; }
    int height() const { return m_h; }
    bool valid() const { return m_fbo != 0; }

    // проверка HDR текстуры: GL_RGBA16F vs GL_RGBA8
    bool isHDR() const { return m_hdr; }

private:
    GLuint m_fbo = 0;
    GLuint m_color = 0;
    GLuint m_depth = 0;
    int m_w = 0, m_h = 0;
    bool m_hdr = true;
};
