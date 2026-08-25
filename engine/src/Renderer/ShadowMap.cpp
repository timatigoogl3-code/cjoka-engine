#include "engine/Renderer/ShadowMap.h"

void ShadowMap::Init(int size) {
    m_size = size;
    if (m_fbo) return;
    glGenFramebuffers(1, &m_fbo);
    glGenTextures(1, &m_depth);
    glBindTexture(GL_TEXTURE_2D, m_depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_size, m_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // мягкие края за пределами карты
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[]{1,1,1,1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    // сравнение — для sampler2DShadow не юзаем, обычный sample + ручной PCF
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depth, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowMap::Shutdown() {
    if (m_fbo) { glDeleteFramebuffers(1,&m_fbo); m_fbo=0; }
    if (m_depth) { glDeleteTextures(1,&m_depth); m_depth=0; }
}

void ShadowMap::Begin(const glm::mat4& lightMatrix) {
    if (!m_fbo) Init(m_size);   // ЛЕНИВАЯ ИНИЦИАЛИЗАЦИЯ — иначе рендер уходит в default FBO
    m_lightMatrix = lightMatrix;
    // сохранить текущий FBO (HDR пайплайна!) чтобы вернуть его в End
    GLint prevFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    m_prevFbo = prevFbo;
    glGetIntegerv(GL_VIEWPORT, m_prevViewport);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0,0,m_size,m_size);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
    glClear(GL_DEPTH_BUFFER_BIT);
    // peter-panning чуть меньше
    glCullFace(GL_FRONT);
}

void ShadowMap::End(int, int) {
    glCullFace(GL_BACK);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    // вернуть ТОТ ЖЕ FBO и вьюпорт что были до теней (HDR), а не default
    glBindFramebuffer(GL_FRAMEBUFFER, m_prevFbo);
    glViewport(m_prevViewport[0], m_prevViewport[1], m_prevViewport[2], m_prevViewport[3]);
}

void ShadowMap::Bind(int slot) const {
    glActiveTexture(GL_TEXTURE0+slot);
    glBindTexture(GL_TEXTURE_2D, m_depth);
}
