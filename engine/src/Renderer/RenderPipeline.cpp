#include "engine/Renderer/RenderPipeline.h"
#include "engine/Renderer/PostProcess.h"
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include <glad/gl.h>
#include <iostream>

RenderPipeline::RenderPipeline(int w,int h): m_w(w), m_h(h) {
    m_hdr.create(w,h,true,true);
    int hw=w/2, hh=h/2;
    m_bloomExtract.create(hw,hh,true,false);
    m_bloomPing.create(hw,hh,true,false);
    m_bloomPong.create(hw,hh,true,false);
    m_composite.create(w,h,true,false);
    PostProcess::Init(w,h);
    std::cout << "[RenderPipeline] HDR " << w << "x" << h << " + bloom half\n";
}
RenderPipeline::~RenderPipeline(){
    PostProcess::Shutdown();
}

void RenderPipeline::resize(int w,int h){
    if(w==m_w && h==m_h) return;
    m_w=w; m_h=h;
    m_hdr.resize(w,h);
    m_composite.resize(w,h);
    int hw=w/2, hh=h/2;
    m_bloomExtract.resize(hw,hh);
    m_bloomPing.resize(hw,hh);
    m_bloomPong.resize(hw,hh);
    PostProcess::Resize(w,h);
}

void RenderPipeline::syncFromRegistry(const Registry& reg){
    auto v = reg.view<PostProcessSettings>();
    if (v.empty()) return;
    const auto& p = reg.get<PostProcessSettings>(v[0]);
    m_settings.hdr = p.hdr;
    m_settings.bloom = p.bloom;
    m_settings.bloomThreshold = p.bloomThreshold;
    m_settings.bloomIntensity = p.bloomIntensity;
    m_settings.bloomBlurPasses = p.bloomBlurPasses;
    m_settings.fxaa = p.fxaa;
    m_settings.vignette = p.vignette;
    m_settings.exposure = p.exposure;
    m_settings.gamma = p.gamma;
}

void RenderPipeline::beginFrame(){
    // проверяем ресайз (окно могло измениться)
    // вызывающий Game уже делает SetViewport, но мы биндим HDR
    m_hdr.bind();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RenderPipeline::endFrame(){
    // HDR уже содержит sky + scene (lit shader уже с tonemapping, но bloom всё равно вытянет яркие)
    if(!m_settings.bloom && !m_settings.fxaa){
        m_hdr.blitToDefault();
        return;
    }

    GLuint bloomTex = 0;
    if(m_settings.bloom){
        // 1. extract bright -> half res
        m_bloomExtract.bind();
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        PostProcess::BloomExtract(m_hdr.colorTexture(), m_bloomExtract.fbo(), m_settings.bloomThreshold);
        // 2. blur ping-pong
        // ping -> pong
        m_bloomPing.bind();
        PostProcess::Blur(m_bloomExtract.colorTexture(), m_bloomPing.fbo(), glm::vec2(1.0f/(m_w/2), 0));
        m_bloomPong.bind();
        PostProcess::Blur(m_bloomPing.colorTexture(), m_bloomPong.fbo(), glm::vec2(0, 1.0f/(m_h/2)));
        // extra passes
        for(int i=1;i<m_settings.bloomBlurPasses;++i){
            m_bloomPing.bind();
            PostProcess::Blur(m_bloomPong.colorTexture(), m_bloomPing.fbo(), glm::vec2(1.0f/(m_w/2),0));
            m_bloomPong.bind();
            PostProcess::Blur(m_bloomPing.colorTexture(), m_bloomPong.fbo(), glm::vec2(0,1.0f/(m_h/2)));
        }
        bloomTex = m_bloomPong.colorTexture();
    }

    // 3. composite (scene + bloom) -> composite FBO (full res) для FXAA, или сразу в default если FXAA off
    if(m_settings.fxaa){
        m_composite.bind();
        glDisable(GL_DEPTH_TEST);
        if(m_settings.bloom){
            PostProcess::Composite(m_hdr.colorTexture(), bloomTex, m_settings.bloomIntensity, m_settings.vignette);
        } else {
            // без bloom — просто blit HDR с vignette
            PostProcess::Composite(m_hdr.colorTexture(), m_hdr.colorTexture(), 0.0f, m_settings.vignette);
        }
        // 4. FXAA -> default
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,m_w,m_h);
        glDisable(GL_DEPTH_TEST);
        PostProcess::FXAA(m_composite.colorTexture());
    } else {
        // без FXAA — composite сразу в default
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glViewport(0,0,m_w,m_h);
        if(m_settings.bloom){
            PostProcess::Composite(m_hdr.colorTexture(), bloomTex, m_settings.bloomIntensity, m_settings.vignette);
        } else {
            // просто blit HDR
            m_hdr.blitToDefault();
        }
    }
    glEnable(GL_DEPTH_TEST);
}
