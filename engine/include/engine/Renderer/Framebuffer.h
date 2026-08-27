#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>

// Framebuffer — HDR ready (RGBA16F + optional RG16F velocity + depth)
// MRT support: color0=HDR RGBA16F, color1=Velocity RG16F (optional)
class Framebuffer {
public:
    Framebuffer() = default;
    Framebuffer(int w, int h, bool hdr = true, bool withDepth = true, bool withVelocity = false);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& o) noexcept;
    Framebuffer& operator=(Framebuffer&& o) noexcept;

    void create(int w, int h, bool hdr = true, bool withDepth = true, bool withVelocity = false);
    void destroy();
    void bind() const;
    void bindRead() const;   // bind for reading only (GL_READ_FRAMEBUFFER)
    void unbind() const;     // bind default
    void resize(int w, int h);
    void blitToDefault() const; // GL_NEAREST
    void blitColorTo(Framebuffer& dst) const; // blit color only

    GLuint fbo() const { return m_fbo; }
    GLuint colorTexture() const { return m_color; }
    GLuint velocityTexture() const { return m_velocity; }
    GLuint normalRoughnessTexture() const { return m_velocity; }
    GLuint depthTexture() const { return m_depth; }
    GLuint depthRBO() const { return m_depth; }
    int width() const { return m_w; }
    int height() const { return m_h; }
    bool valid() const { return m_fbo != 0; }
    bool hasVelocity() const { return m_velocity != 0; }

    bool isHDR() const { return m_hdr; }

private:
    GLuint m_fbo = 0;
    GLuint m_color = 0;
    GLuint m_velocity = 0;  // RG16F velocity buffer
    GLuint m_depth = 0;
    int m_w = 0, m_h = 0;
    bool m_hdr = true;
};
