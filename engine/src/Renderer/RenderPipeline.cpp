#include "engine/Renderer/RenderPipeline.h"
#include "engine/Renderer/PostProcess.h"
#include "engine/Renderer/DefaultShaders.h"
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>

RenderPipeline::RenderPipeline(int w,int h): m_w(w), m_h(h) {
    m_hdr.create(w,h,true,true,true);   // RGBA16F + depth + velocity RG16F
    m_ssr.create(w,h,true,false);
    m_hdrPostSSR.create(w,h,true,false);
    int hw=w/2, hh=h/2;
    m_bloomExtract.create(hw,hh,true,false);
    m_bloomPing.create(hw,hh,true,false);
    m_bloomPong.create(hw,hh,true,false);
    m_gtao.create(hw,hh,false,false,false);    // half-res RGBA8 AO
    m_gtaoBlur.create(hw,hh,false,false,false); // half-res RGBA8 AO blurred
    int qw = w/4, qh = h/4;
    m_fog.create(qw,qh,true,false,false);      // quarter-res RGBA16F fog
    m_lightShafts.create(hw,hh,true,false,false); // half-res RGBA16F
    m_composite.create(w,h,true,false);
    m_taaHistory[0].create(w,h,true,false);
    m_taaHistory[1].create(w,h,true,false);
    PostProcess::Init(w,h);
    std::cout << "[RenderPipeline] HDR " << w << "x" << h << " + velocity + SSR + GTAO + bloom + TAA\n";
}
RenderPipeline::~RenderPipeline(){
    PostProcess::Shutdown();
}

void RenderPipeline::resize(int w,int h){
    if(w==m_w && h==m_h) return;
    m_w=w; m_h=h;
    m_hdr.resize(w,h);
    m_ssr.resize(w,h);
    m_hdrPostSSR.resize(w,h);
    m_composite.resize(w,h);
    m_taaHistory[0].resize(w,h);
    m_taaHistory[1].resize(w,h);
    int hw=w/2, hh=h/2;
    m_bloomExtract.resize(hw,hh);
    m_bloomPing.resize(hw,hh);
    m_bloomPong.resize(hw,hh);
    m_gtao.resize(hw,hh);
    m_gtaoBlur.resize(hw,hh);
    int qw = w/4, qh = h/4;
    m_fog.resize(qw,qh);
    m_lightShafts.resize(hw,hh);
    PostProcess::Resize(w,h);
}

void RenderPipeline::setCameraMatrices(const glm::mat4& view, const glm::mat4& proj) {
    m_prevViewProj = (m_frameIndex == 0) ? (proj * view) : m_currentViewProj;
    m_currentViewProj = proj * view;
    m_currentProj = proj;
    m_currentView = view;
}

void RenderPipeline::syncFromRegistry(const Registry& reg){
    auto v = reg.view<PostProcessSettings>();
    if (v.empty()) return;
    const auto& p = reg.get<PostProcessSettings>(*v.begin());
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

void RenderPipeline::ensureZPrepassShader(){
    if(!m_zPrepassShader) m_zPrepassShader = new Shader(DefaultShaders::kZPrepassVS, DefaultShaders::kZPrepassFS);
}

void RenderPipeline::ensureGTAOShaders(){
    if(!m_gtaoShader) m_gtaoShader = new Shader(DefaultShaders::kPostVS, DefaultShaders::kGTAOFS);
    if(!m_gtaoBlurShader) m_gtaoBlurShader = new Shader(DefaultShaders::kPostVS, DefaultShaders::kGTAOBlurFS);
    if(!m_aoMultiplyShader) m_aoMultiplyShader = new Shader(DefaultShaders::kPostVS, DefaultShaders::kAOMultiplyFS);
}

void RenderPipeline::ensureFogShaders(){
    if(!m_fogShader) m_fogShader = new Shader(DefaultShaders::kPostVS, DefaultShaders::kVolumetricFogFS);
    if(!m_fogCompositeShader) m_fogCompositeShader = new Shader(DefaultShaders::kPostVS, DefaultShaders::kFogCompositeFS);
    if(!m_lightShaftsShader) m_lightShaftsShader = new Shader(DefaultShaders::kPostVS, DefaultShaders::kLightShaftsFS);
}

void RenderPipeline::gtao(){
    if(!m_settings.gtao) return;
    ensureGTAOShaders();

    int hw = m_w / 2, hh = m_h / 2;

    // 1. GTAO — half-res
    m_gtao.bind();
    glViewport(0, 0, hw, hh);
    glClear(GL_COLOR_BUFFER_BIT);
    m_gtaoShader->use();
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_hdr.depthTexture());
    m_gtaoShader->setInt("uDepthTex", 3);
    m_gtaoShader->setMat4("uProj", m_currentProj);
    m_gtaoShader->setMat4("uView", m_currentView);
    m_gtaoShader->setVec2("uScreenSize", glm::vec2(hw, hh));
    m_gtaoShader->setFloat("uRadius", m_settings.gtaoRadius);
    m_gtaoShader->setFloat("uIntensity", m_settings.gtaoIntensity);
    m_gtaoShader->setInt("uDirections", 4);
    m_gtaoShader->setInt("uSteps", 4);
    PostProcess::DrawFullscreen();

    // 2. Edge-aware blur — half-res
    m_gtaoBlur.bind();
    glViewport(0, 0, hw, hh);
    glClear(GL_COLOR_BUFFER_BIT);
    m_gtaoBlurShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_gtao.colorTexture());
    m_gtaoBlurShader->setInt("uAOTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_hdr.depthTexture());
    m_gtaoBlurShader->setInt("uDepthTex", 1);
    m_gtaoBlurShader->setVec2("uScreenSize", glm::vec2(hw, hh));
    PostProcess::DrawFullscreen();

    // Restore viewport
    glViewport(0, 0, m_w, m_h);
}

void RenderPipeline::applyAO(){
    if(!m_settings.gtao) return;
    ensureGTAOShaders();

    // Multiply HDR by AO: m_hdr * m_gtaoBlur → m_hdrPostSSR
    m_hdrPostSSR.bind();
    glViewport(0, 0, m_w, m_h);
    glDisable(GL_DEPTH_TEST);
    m_aoMultiplyShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdr.colorTexture());
    m_aoMultiplyShader->setInt("uHDR", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_gtaoBlur.colorTexture());
    m_aoMultiplyShader->setInt("uAO", 1);
    PostProcess::DrawFullscreen();
}

void RenderPipeline::volumetricFog(){
    if(!m_settings.volumetricFog) return;
    ensureFogShaders();

    int qw = m_w / 4, qh = m_h / 4;

    // 1. Compute fog at quarter-res
    m_fog.bind();
    glViewport(0, 0, qw, qh);
    glClear(GL_COLOR_BUFFER_BIT);
    m_fogShader->use();

    // Depth + scene
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdr.depthTexture());
    m_fogShader->setInt("uDepthTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_hdr.colorTexture());
    m_fogShader->setInt("uSceneTex", 1);

    // Matrices
    glm::mat4 invProj = glm::inverse(m_currentProj);
    glm::mat4 invView = glm::inverse(m_currentView);
    m_fogShader->setMat4("uInvProj", invProj);
    m_fogShader->setMat4("uInvView", invView);
    m_fogShader->setMat4("uView", m_currentView);
    m_fogShader->setVec3("uCameraPos", glm::vec3(invView[3]));

    // Fog parameters
    m_fogShader->setVec3("uFogColor", glm::vec3(0.7f, 0.75f, 0.85f));
    m_fogShader->setFloat("uFogDensity", m_settings.fogDensity);
    m_fogShader->setFloat("uFogHeightFalloff", m_settings.fogHeightFalloff);
    m_fogShader->setFloat("uFogHeight", m_settings.fogHeight);
    m_fogShader->setFloat("uFogStart", m_settings.fogStart);
    m_fogShader->setFloat("uFogEnd", m_settings.fogEnd);
    m_fogShader->setInt("uStepCount", 16);
    m_fogShader->setFloat("uTime", static_cast<float>(glfwGetTime()));

    PostProcess::DrawFullscreen();

    // 2. Composite: upsample fog to full-res and replace scene
    m_hdrPostSSR.bind();
    glViewport(0, 0, m_w, m_h);
    m_fogCompositeShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdr.colorTexture());
    m_fogCompositeShader->setInt("uSceneTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_fog.colorTexture());
    m_fogCompositeShader->setInt("uFogTex", 1);
    PostProcess::DrawFullscreen();

    glViewport(0, 0, m_w, m_h);
}

void RenderPipeline::lightShafts(){
    if(!m_settings.lightShafts) return;
    ensureFogShaders();

    int hw = m_w / 2, hh = m_h / 2;

    // Sun screen-space position
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.4f, 0.6f, 0.2f));
    glm::vec4 sunClip = m_currentProj * m_currentView * glm::vec4(sunDir * 100.0f, 1.0f);
    glm::vec2 sunNDC = glm::vec2(sunClip.x / sunClip.w, sunClip.y / sunClip.w);
    glm::vec2 sunScreen = sunNDC * 0.5f + 0.5f;

    m_lightShafts.bind();
    glViewport(0, 0, hw, hh);
    glClear(GL_COLOR_BUFFER_BIT);
    m_lightShaftsShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdr.colorTexture());
    m_lightShaftsShader->setInt("uSceneTex", 0);
    m_lightShaftsShader->setVec2("uSunScreenPos", sunScreen);
    m_lightShaftsShader->setFloat("uDensity", m_settings.shaftDensity);
    m_lightShaftsShader->setFloat("uWeight", m_settings.shaftWeight);
    m_lightShaftsShader->setFloat("uDecay", 0.96f);
    m_lightShaftsShader->setFloat("uExposure", 0.15f);
    m_lightShaftsShader->setInt("uSamples", 64);
    PostProcess::DrawFullscreen();

    // Additive blend shafts onto m_hdrPostSSR
    m_hdrPostSSR.bind();
    glViewport(0, 0, m_w, m_h);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    m_fogCompositeShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_lightShafts.colorTexture());
    m_fogCompositeShader->setInt("uSceneTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_lightShafts.colorTexture());
    m_fogCompositeShader->setInt("uFogTex", 1);
    PostProcess::DrawFullscreen();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glViewport(0, 0, m_w, m_h);
}

void RenderPipeline::zPrepass(){
    if(!m_settings.zPrepass) return;
    ensureZPrepassShader();

    m_hdr.bind();
    glDepthMask(GL_TRUE);
    glDepthFunc(DepthState::kFunc);
    glClearDepthf(DepthState::kClear);
    glClear(GL_DEPTH_BUFFER_BIT);

    m_zPrepassShader->use();
    m_zPrepassShader->setMat4("uMVP", m_currentViewProj);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glDepthFunc(DepthState::kFunc);
}

void RenderPipeline::beginFrame(){
    m_hdr.bind();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearDepthf(1.0f);
    glDepthMask(GL_TRUE);

    GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, bufs);
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RenderPipeline::endFrame(){
    glDisable(GL_DEPTH_TEST);

    GLuint baseHdrTex = m_hdr.colorTexture();

    // 1. SSR
    if(m_settings.ssr){
        glm::mat4 invProj = glm::inverse(m_currentProj);
        PostProcess::SSR(m_hdr.colorTexture(), m_hdr.depthTexture(), m_hdr.normalRoughnessTexture(), m_ssr.fbo(), m_currentProj, invProj, m_w, m_h);
        PostProcess::SSRComposite(m_hdr.colorTexture(), m_ssr.colorTexture(), m_hdrPostSSR.fbo(), 0.85f);
        baseHdrTex = m_hdrPostSSR.colorTexture();
    }

    if(!m_settings.bloom && !m_settings.fxaa){
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_settings.ssr ? m_hdrPostSSR.fbo() : m_hdr.fbo());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0,0,m_w,m_h, 0,0,m_w,m_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        return;
    }

    GLuint bloomTex = 0;
    if(m_settings.bloom){
        // extract bright -> half res
        m_bloomExtract.bind();
        glClear(GL_COLOR_BUFFER_BIT);
        PostProcess::BloomExtract(baseHdrTex, m_bloomExtract.fbo(), m_settings.bloomThreshold);
        // blur ping-pong
        float halfW = static_cast<float>(m_w) * 0.5f;
        float halfH = static_cast<float>(m_h) * 0.5f;
        glm::vec2 blurH(1.0f / halfW, 0.0f);
        glm::vec2 blurV(0.0f, 1.0f / halfH);
        m_bloomPing.bind();
        PostProcess::Blur(m_bloomExtract.colorTexture(), m_bloomPing.fbo(), blurH);
        m_bloomPong.bind();
        PostProcess::Blur(m_bloomPing.colorTexture(), m_bloomPong.fbo(), blurV);
        for(int i=1;i<m_settings.bloomBlurPasses;++i){
            m_bloomPing.bind();
            PostProcess::Blur(m_bloomPong.colorTexture(), m_bloomPing.fbo(), blurH);
            m_bloomPong.bind();
            PostProcess::Blur(m_bloomPing.colorTexture(), m_bloomPong.fbo(), blurV);
        }
        bloomTex = m_bloomPong.colorTexture();
    }

    // 2. composite (scene + SSR + bloom) -> composite FBO
    m_composite.bind();
    glDisable(GL_DEPTH_TEST);
    if (m_settings.bloom) {
        PostProcess::Composite(baseHdrTex, bloomTex, m_settings.bloomIntensity, m_settings.vignette, m_settings.exposure, m_settings.gamma, 1.06f);
    } else {
        PostProcess::Composite(baseHdrTex, baseHdrTex, 0.0f, m_settings.vignette, m_settings.exposure, m_settings.gamma, 1.06f);
    }

    // 3. TAA / Final output
    GLuint finalColorTex = m_composite.colorTexture();

    if (m_settings.taa) {
        int currentHistory = m_taaIndex;
        int prevHistory = 1 - m_taaIndex;
        glm::mat4 invVP = glm::inverse(m_currentViewProj);
        PostProcess::TAA(m_composite.colorTexture(), m_hdr.depthTexture(), m_taaHistory[prevHistory].colorTexture(),
                         m_taaHistory[currentHistory].fbo(), invVP, m_prevViewProj, 0.88f);
        finalColorTex = m_taaHistory[currentHistory].colorTexture();
        m_taaIndex = 1 - m_taaIndex;
    }

    // 4. Output to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_w, m_h);
    glDisable(GL_DEPTH_TEST);

    if (m_settings.fxaa) {
        PostProcess::FXAA(finalColorTex);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_composite.fbo());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, m_w, m_h, 0, 0, m_w, m_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    m_frameIndex++;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}
