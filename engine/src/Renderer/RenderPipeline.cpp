#include "engine/Renderer/RenderPipeline.h"
#include "engine/Renderer/PostProcess.h"
#include "engine/Renderer/DefaultShaders.h"
#include "engine/Renderer/VXGI.h"
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>

void RenderPipeline::setSettings(const Settings& s) {
    m_settings = s;
    cjoka::VXGI::Get().setEnabled(s.vxgi);
}

RenderPipeline::RenderPipeline(int w,int h): m_w(w), m_h(h) {
    m_hdr.create(w,h,true,true,true);   // RGBA16F + depth + velocity RG16F
    m_ssr.create(w,h,true,false);
    m_hdrPost[0].create(w,h,true,false);
    m_hdrPost[1].create(w,h,true,false);
    int hw=w/2, hh=h/2;
    m_bloomExtract.create(hw,hh,true,false);
    m_bloomPing.create(hw,hh,true,false);
    m_bloomPong.create(hw,hh,true,false);
    m_gtao.create(hw,hh,false,false,false);    // half-res RGBA8 AO
    m_gtaoBlur.create(hw,hh,false,false,false); // half-res RGBA8 AO blurred
    int qw = w/4, qh = h/4;
    m_fog.create(qw,qh,true,false,false);      // quarter-res RGBA16F fog
    m_sunMask.create(hw,hh,true,false,false);  // half-res RGBA16F sun mask
    m_lightShafts.create(hw,hh,true,false,false); // half-res RGBA16F
    m_composite.create(w,h,true,false);
    m_taaHistory[0].create(w,h,true,false);
    m_taaHistory[1].create(w,h,true,false);
    PostProcess::Init(w,h);
    std::cout << "[RenderPipeline] HDR " << w << "x" << h << " + velocity + SSR + GTAO + bloom + TAA (Ping-Pong Safe)\n";
}
RenderPipeline::~RenderPipeline(){
    PostProcess::Shutdown();
}

void RenderPipeline::resize(int w,int h){
    if(w==m_w && h==m_h) return;
    m_w=w; m_h=h;
    m_hdr.resize(w,h);
    m_ssr.resize(w,h);
    m_hdrPost[0].resize(w,h);
    m_hdrPost[1].resize(w,h);
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
    m_sunMask.resize(hw,hh);
    m_lightShafts.resize(hw,hh);
    PostProcess::Resize(w,h);
}

void RenderPipeline::setCameraMatrices(const glm::mat4& view, const glm::mat4& proj) {
    m_prevViewProj = (m_frameIndex == 0) ? (proj * view) : m_currentViewProj;
    m_currentViewProj = proj * view;
    m_currentProj = proj;
    m_currentView = view;
}

#include "engine/Core/Profiler.h"

static float halton(int index, int base) {
    float f = 1.0f;
    float r = 0.0f;
    while (index > 0) {
        f = f / float(base);
        r = r + f * float(index % base);
        index = index / base;
    }
    return r;
}

glm::vec2 RenderPipeline::getJitterOffset() const {
    if (!m_settings.taa || m_w <= 0 || m_h <= 0) return glm::vec2(0.0f);
    int idx = (m_frameIndex % 16) + 1;
    float jx = (halton(idx, 2) - 0.5f) / float(m_w);
    float jy = (halton(idx, 3) - 0.5f) / float(m_h);
    return glm::vec2(jx, jy);
}

void RenderPipeline::syncFromRegistry(const Registry& reg){
    auto dlView = reg.view<DirectionalLight>();
    if (!dlView.empty()) {
        const auto& dl = reg.get<DirectionalLight>(*dlView.begin());
        if (glm::length(dl.direction) > 0.001f) {
            m_sunDir = -glm::normalize(dl.direction);
        }
        m_sunColor = dl.color;
    }

    auto v = reg.view<PostProcessSettings>();
    if (v.empty()) return;
    const auto& p = reg.get<PostProcessSettings>(*v.begin());
    m_settings.hdr = p.hdr;
    m_settings.bloom = p.bloom;
    m_settings.bloomThreshold = p.bloomThreshold;
    m_settings.bloomIntensity = p.bloomIntensity;
    m_settings.bloomBlurPasses = p.bloomBlurPasses;
    m_settings.fxaa = p.fxaa;
    m_settings.taa = p.taa;
    m_settings.vignette = p.vignette;
    m_settings.exposure = p.exposure;
    m_settings.gamma = p.gamma;
    m_settings.gtao = p.gtao;
    m_settings.gtaoRadius = p.gtaoRadius;
    m_settings.gtaoIntensity = p.gtaoIntensity;
    m_settings.volumetricFog = p.volumetricFog;
    m_settings.fogDensity = p.fogDensity;
    m_settings.fogHeightFalloff = p.fogHeightFalloff;
    m_settings.fogHeight = p.fogHeight;
    m_settings.fogStart = p.fogStart;
    m_settings.fogEnd = p.fogEnd;
    m_settings.lightShafts = p.lightShafts;
    m_settings.shaftDensity = p.shaftDensity;
    m_settings.shaftWeight = p.shaftWeight;
    m_settings.ssr = p.ssr;
    m_settings.vxgi = p.vxgi;
    m_settings.radialBlur = p.radialBlur;
    m_settings.radialBlurStrength = p.radialBlurStrength;
    m_settings.chromaticAberration = p.chromaticAberration;
    cjoka::VXGI::Get().setEnabled(p.vxgi);
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
    if(!m_sunMaskShader) m_sunMaskShader = new Shader(DefaultShaders::kPostVS, DefaultShaders::kSunMaskFS);
    if(!m_lightShaftsShader) m_lightShaftsShader = new Shader(DefaultShaders::kPostVS, DefaultShaders::kLightShaftsFS);
    if(!m_lightShaftsCompositeShader) m_lightShaftsCompositeShader = new Shader(DefaultShaders::kPostVS, DefaultShaders::kLightShaftsCompositeFS);
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
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdr.normalRoughnessTexture());
    m_gtaoShader->setInt("uNormalTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_hdr.depthTexture());
    m_gtaoShader->setInt("uDepthTex", 1);
    m_gtaoShader->setMat4("uInvProj", glm::inverse(m_currentProj));
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

void RenderPipeline::applyAO(GLuint inputTex){
    if(!m_settings.gtao) return;

    auto& dst = currentWritePostFBO();
    dst.bind();
    glViewport(0, 0, m_w, m_h);
    glClear(GL_COLOR_BUFFER_BIT);

    m_aoMultiplyShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTex);
    m_aoMultiplyShader->setInt("uHDR", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_gtaoBlur.colorTexture());
    m_aoMultiplyShader->setInt("uAO", 1);

    PostProcess::DrawFullscreen();
    swapPostBuffers();

    glActiveTexture(GL_TEXTURE0);
}

bool RenderPipeline::volumetricFog(GLuint inputTex){
    if(!m_settings.volumetricFog) return false;
    ensureFogShaders();

    int hw = m_w / 2, hh = m_h / 2;

    m_fog.bind();
    glViewport(0, 0, hw, hh);
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
    m_fogShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdr.depthTexture());
    m_fogShader->setInt("uDepthTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, inputTex);
    m_fogShader->setInt("uSceneTex", 1);
    m_fogShader->setMat4("uInvProj", glm::inverse(m_currentProj));
    m_fogShader->setMat4("uInvView", glm::inverse(m_currentView));
    m_fogShader->setMat4("uView", m_currentView);
    glm::vec3 camPos = glm::vec3(glm::inverse(m_currentView)[3]);
    m_fogShader->setVec3("uCameraPos", camPos);
    m_fogShader->setVec3("uFogColor", glm::vec3(0.65f, 0.72f, 0.85f));
    m_fogShader->setFloat("uFogDensity", m_settings.fogDensity);
    m_fogShader->setFloat("uFogHeightFalloff", m_settings.fogHeightFalloff);
    m_fogShader->setFloat("uFogHeight", m_settings.fogHeight);
    m_fogShader->setFloat("uFogStart", m_settings.fogStart);
    m_fogShader->setFloat("uFogEnd", m_settings.fogEnd);
    m_fogShader->setInt("uStepCount", 32);
    m_fogShader->setFloat("uTime", static_cast<float>(glfwGetTime()));
    PostProcess::DrawFullscreen();

    // Composite fog onto currentWritePostFBO()
    auto& dst = currentWritePostFBO();
    dst.bind();
    glViewport(0, 0, m_w, m_h);
    m_fogCompositeShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTex);
    m_fogCompositeShader->setInt("uSceneTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_fog.colorTexture());
    m_fogCompositeShader->setInt("uFogTex", 1);
    PostProcess::DrawFullscreen();
    swapPostBuffers();

    glViewport(0, 0, m_w, m_h);
    return true;
}

bool RenderPipeline::lightShafts(GLuint inputTex){
    if(!m_settings.lightShafts) return false;
    ensureFogShaders();

    glm::vec3 sunDir = m_sunDir;
    if (glm::length(sunDir) < 0.001f) return false;
    glm::vec4 sunView = m_currentView * glm::vec4(sunDir, 0.0f);
    if (sunView.z >= 0.0f) return false; // Sun is behind camera

    glm::vec4 sunClip = m_currentProj * glm::vec4(sunView.x, sunView.y, sunView.z, 1.0f);
    if (sunClip.w <= 0.0001f) return false;

    glm::vec2 sunNDC = glm::vec2(sunClip.x / sunClip.w, sunClip.y / sunClip.w);
    glm::vec2 sunScreen = sunNDC * 0.5f + 0.5f;

    if (sunScreen.x < -0.4f || sunScreen.x > 1.4f || sunScreen.y < -0.4f || sunScreen.y > 1.4f) return false;

    float sunFacing = glm::clamp(-sunView.z / glm::length(glm::vec3(sunView)), 0.0f, 1.0f);
    float maxDist = std::max(std::abs(sunScreen.x - 0.5f), std::abs(sunScreen.y - 0.5f));
    float edgeFade = glm::clamp(1.0f - (maxDist - 0.5f) * 1.25f, 0.0f, 1.0f);
    edgeFade = edgeFade * edgeFade * (3.0f - 2.0f * edgeFade);
    if (edgeFade * sunFacing <= 0.001f) return false;

    int hw = m_w / 2, hh = m_h / 2;

    // 1. Pass 1: Sun mask (depth occludes sun) into m_sunMask (half-res)
    m_sunMask.bind();
    glViewport(0, 0, hw, hh);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    m_sunMaskShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdr.depthTexture());
    m_sunMaskShader->setInt("uDepthTex", 0);
    m_sunMaskShader->setVec2("uSunScreenPos", sunScreen);
    PostProcess::DrawFullscreen();

    // 2. Pass 2: Radial blur on the sun mask into m_lightShafts
    m_lightShafts.bind();
    glViewport(0, 0, hw, hh);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    m_lightShaftsShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_sunMask.colorTexture());
    m_lightShaftsShader->setInt("uMaskTex", 0);
    m_lightShaftsShader->setVec2("uSunScreenPos", sunScreen);
    m_lightShaftsShader->setFloat("uDensity", m_settings.shaftDensity);
    m_lightShaftsShader->setFloat("uWeight", m_settings.shaftWeight * sunFacing * edgeFade * 0.45f);
    m_lightShaftsShader->setFloat("uDecay", 0.97f);
    m_lightShaftsShader->setFloat("uExposure", 0.6f);
    m_lightShaftsShader->setInt("uSamples", 64);
    PostProcess::DrawFullscreen();

    // 3. Pass 3: Additive composite onto currentWritePostFBO()
    auto& dst = currentWritePostFBO();
    dst.bind();
    glViewport(0, 0, m_w, m_h);
    m_lightShaftsCompositeShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTex);
    m_lightShaftsCompositeShader->setInt("uSceneTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_lightShafts.colorTexture());
    m_lightShaftsCompositeShader->setInt("uShaftsTex", 1);
    m_lightShaftsCompositeShader->setVec3("uSunColor", m_sunColor);
    PostProcess::DrawFullscreen();
    swapPostBuffers();

    glViewport(0, 0, m_w, m_h);
    return true;
}

void RenderPipeline::zPrepass(){
    if(!m_settings.zPrepass) return;
    ensureZPrepassShader();

    m_hdr.bind();
    glDepthMask(GL_TRUE);
    glDepthFunc(DepthState::kFunc);
    glClearDepthf(DepthState::kClear);
    glClear(GL_DEPTH_BUFFER_BIT);
    // Draw calls for depth only
}

void RenderPipeline::beginFrame(){
    cjoka::Profiler::Get().beginFrame();
    m_hdr.bind();
    glViewport(0, 0, m_w, m_h);
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

    m_postPingPongIdx = 0;
    GLuint currentHdrTex = m_hdr.colorTexture();
    GLuint lastWrittenFBO = m_hdr.fbo();

    // 0. Ambient Occlusion (GTAO)
    if(m_settings.gtao){
        CJOKA_PROFILE_GPU("GTAO (Ambient Occlusion)");
        gtao();
        applyAO(currentHdrTex);
        currentHdrTex = currentReadPostFBO().colorTexture();
        lastWrittenFBO = currentReadPostFBO().fbo();
    }

    // 0.5. Volumetric Fog
    if(m_settings.volumetricFog){
        CJOKA_PROFILE_GPU("Volumetric Fog");
        if (volumetricFog(currentHdrTex)) {
            currentHdrTex = currentReadPostFBO().colorTexture();
            lastWrittenFBO = currentReadPostFBO().fbo();
        }
    }

    // 0.6. Light Shafts (God Rays)
    if(m_settings.lightShafts){
        CJOKA_PROFILE_GPU("Light Shafts (God Rays)");
        if (lightShafts(currentHdrTex)) {
            currentHdrTex = currentReadPostFBO().colorTexture();
            lastWrittenFBO = currentReadPostFBO().fbo();
        }
    }

    // 1. SSR (Screen Space Reflections) - Ping-pong safe (reads currentHdrTex, writes to distinct write FBO)
    if(m_settings.ssr){
        CJOKA_PROFILE_GPU("SSR Reflections");
        glm::mat4 invProj = glm::inverse(m_currentProj);
        PostProcess::SSR(currentHdrTex, m_hdr.depthTexture(), m_hdr.normalRoughnessTexture(), m_ssr.fbo(), m_currentProj, invProj, m_w, m_h);
        auto& dst = currentWritePostFBO();
        PostProcess::SSRComposite(currentHdrTex, m_ssr.colorTexture(), dst.fbo(), 0.85f);
        swapPostBuffers();
        currentHdrTex = currentReadPostFBO().colorTexture();
        lastWrittenFBO = currentReadPostFBO().fbo();
    }

    GLuint bloomTex = 0;
    if(m_settings.bloom){
        CJOKA_PROFILE_GPU("Bloom Extract & Blur");
        // extract bright -> half res
        m_bloomExtract.bind();
        glClear(GL_COLOR_BUFFER_BIT);
        PostProcess::BloomExtract(currentHdrTex, m_bloomExtract.fbo(), m_settings.bloomThreshold);
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
    {
        CJOKA_PROFILE_GPU("Composite & Tonemapping");
        m_composite.bind();
        glDisable(GL_DEPTH_TEST);
        if (m_settings.bloom) {
            PostProcess::Composite(currentHdrTex, bloomTex, m_settings.bloomIntensity, m_settings.vignette, m_settings.exposure, m_settings.gamma, 1.06f);
        } else {
            PostProcess::Composite(currentHdrTex, currentHdrTex, 0.0f, m_settings.vignette, m_settings.exposure, m_settings.gamma, 1.06f);
        }
    }

    // 2.5. Radial / Motion Blur
    GLuint sourceForFinal = m_composite.colorTexture();
    if (m_settings.radialBlur || m_settings.radialBlurStrength > 0.001f) {
        CJOKA_PROFILE_GPU("Radial Blur");
        auto& dst = currentWritePostFBO();
        PostProcess::RadialBlur(m_composite.colorTexture(), dst.fbo(), m_settings.radialBlurStrength, m_settings.chromaticAberration);
        sourceForFinal = dst.colorTexture();
        swapPostBuffers();
    }

    // 3. TAA / Final output
    GLuint finalColorTex = sourceForFinal;

    if (m_settings.taa) {
        CJOKA_PROFILE_GPU("TAA Reprojection");
        int currentHistory = m_taaIndex;
        int prevHistory = 1 - m_taaIndex;
        glm::mat4 invVP = glm::inverse(m_currentViewProj);
        PostProcess::TAA(sourceForFinal, m_hdr.depthTexture(), m_taaHistory[prevHistory].colorTexture(),
                         m_taaHistory[currentHistory].fbo(), invVP, m_prevViewProj, 0.88f);
        finalColorTex = m_taaHistory[currentHistory].colorTexture();
        m_taaIndex = 1 - m_taaIndex;
    }

    // 4. Output to default framebuffer
    {
        CJOKA_PROFILE_GPU("FXAA & Blit");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_w, m_h);
        glDisable(GL_DEPTH_TEST);

        if (m_settings.fxaa) {
            PostProcess::FXAA(finalColorTex);
        } else {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_settings.taa ? m_taaHistory[1 - m_taaIndex].fbo() : m_composite.fbo());
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, m_w, m_h, 0, 0, m_w, m_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    m_frameIndex++;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}
