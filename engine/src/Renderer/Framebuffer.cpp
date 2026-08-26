#include "engine/Renderer/Framebuffer.h"
#include <iostream>

Framebuffer::Framebuffer(int w, int h, bool hdr, bool withDepth) { create(w,h,hdr,withDepth); }
Framebuffer::~Framebuffer(){ destroy(); }

Framebuffer::Framebuffer(Framebuffer&& o) noexcept
    : m_fbo(o.m_fbo), m_color(o.m_color), m_depth(o.m_depth), m_w(o.m_w), m_h(o.m_h), m_hdr(o.m_hdr) {
    o.m_fbo=o.m_color=o.m_depth=0;
}
Framebuffer& Framebuffer::operator=(Framebuffer&& o) noexcept {
    if(this!=&o){ destroy(); m_fbo=o.m_fbo; m_color=o.m_color; m_depth=o.m_depth; m_w=o.m_w; m_h=o.m_h; m_hdr=o.m_hdr; o.m_fbo=o.m_color=o.m_depth=0; }
    return *this;
}

void Framebuffer::create(int w,int h,bool hdr,bool withDepth){
    destroy();
    m_w=w; m_h=h; m_hdr=hdr;
    glGenFramebuffers(1,&m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1,&m_color);
    glBindTexture(GL_TEXTURE_2D, m_color);
    GLenum internal = hdr ? GL_RGBA16F : GL_RGBA8;
    GLenum format = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D,0,internal,w,h,0,format,GL_UNSIGNED_BYTE,nullptr);
    // HDR needs linear filtering for blur
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,m_color,0);

    if(withDepth){
        glGenTextures(1, &m_depth);
        glBindTexture(GL_TEXTURE_2D, m_depth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depth, 0);
    }

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){
        std::cerr << "[Framebuffer] incomplete " << w << "x" << h << "\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}
void Framebuffer::destroy(){
    if(m_depth) { glDeleteTextures(1,&m_depth); m_depth=0; }
    if(m_color) { glDeleteTextures(1,&m_color); m_color=0; }
    if(m_fbo) { glDeleteFramebuffers(1,&m_fbo); m_fbo=0; }
}
void Framebuffer::bind() const { glBindFramebuffer(GL_FRAMEBUFFER,m_fbo); glViewport(0,0,m_w,m_h); }
void Framebuffer::unbind() const { glBindFramebuffer(GL_FRAMEBUFFER,0); }
void Framebuffer::resize(int w,int h){
    if(w==m_w && h==m_h) return;
    bool hdr=m_hdr;
    bool hasDepth=m_depth!=0;
    create(w,h,hdr,hasDepth);
}
void Framebuffer::blitToDefault() const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER,m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
    glBlitFramebuffer(0,0,m_w,m_h,0,0,m_w,m_h,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}
