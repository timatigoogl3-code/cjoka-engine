#pragma once
#include "engine/Renderer/Framebuffer.h"
#include "engine/Renderer/Shader.h"
#include "engine/Renderer/DepthState.h"
#include <glm/glm.hpp>

class Window;
class Registry;

// RenderPipeline — как в RAGE: HDR FBO → Bloom → Composite → FXAA → Default
// Использование:
//   RenderPipeline pipe(w,h);
//   pipe.beginFrame(); // bind HDR
//   Systems::Render(registry, litShader, window); // рисует в HDR
//   pipe.endFrame();   // post + blit
class RenderPipeline {
public:
    struct Settings {
        bool hdr = true;
        bool bloom = true;
        float bloomThreshold = 1.0f;
        float bloomIntensity = 0.55f;
        int bloomBlurPasses = 2; // ping-pong
        bool fxaa = true;
        bool taa = false;
        float vignette = 0.32f;
        float exposure = 1.0f;
        float gamma = 2.2f;
        bool zPrepass = false;
        bool gtao = false;
        float gtaoRadius = 1.5f;
        float gtaoIntensity = 1.0f;
        bool volumetricFog = false;
        float fogDensity = 0.02f;
        float fogHeightFalloff = 0.5f;
        float fogHeight = 0.0f;
        float fogStart = 1.0f;
        float fogEnd = 50.0f;
        bool lightShafts = false;
        float shaftDensity = 0.8f;
        float shaftWeight = 0.6f;
        bool ssr = true;
    };

    RenderPipeline(int w, int h);
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;

    void resize(int w, int h);
    void setSettings(const Settings& s) { m_settings = s; }
    const Settings& settings() const { return m_settings; }
    void syncFromRegistry(const Registry& reg); // тянет PostProcessSettings из ECS если есть

    // Начать кадр: биндит HDR FBO, чистит, включает depth
    void beginFrame();
    // Закончить кадр: bloom + composite + FXAA + blit в default
    // Вызывать после Systems::Render(...)
    void endFrame();

    void setCameraMatrices(const glm::mat4& view, const glm::mat4& proj);

    Framebuffer& hdrFBO() { return m_hdr; }
    GLuint hdrTexture() const { return m_hdr.colorTexture(); }
    GLuint velocityTexture() const { return m_hdr.velocityTexture(); }
    GLuint aoTexture() const { return m_gtao.colorTexture(); }
    GLuint fogTexture() const { return m_fog.colorTexture(); }
    GLuint lightShaftsTexture() const { return m_lightShafts.colorTexture(); }
    const glm::mat4& prevViewProj() const { return m_prevViewProj; }

private:
    void ensurePostShaders();
    void ensureZPrepassShader();
    void ensureGTAOShaders();
    void ensureFogShaders();
    void zPrepass();
    void gtao();
    void applyAO();
    void volumetricFog();
    void lightShafts(); // screen-space god rays

    int m_w=0, m_h=0;
    Settings m_settings;
    Framebuffer m_hdr;          // full res HDR
    Framebuffer m_ssr;          // full res SSR reflections
    Framebuffer m_hdrPostSSR;   // full res HDR + SSR
    Framebuffer m_bloomExtract; // half res
    Framebuffer m_bloomPing;    // half res
    Framebuffer m_bloomPong;    // half res
    Framebuffer m_composite;    // full res (для FXAA/TAA)
    Framebuffer m_taaHistory[2];
    int m_taaIndex = 0;
    Framebuffer m_gtao;       // half-res AO output (R8)
    Framebuffer m_gtaoBlur;   // half-res AO blurred (R8)
    Framebuffer m_fog;        // quarter-res volumetric fog
    Framebuffer m_lightShafts; // half-res light shafts
    glm::mat4 m_currentViewProj{1.0f};
    glm::mat4 m_prevViewProj{1.0f};
    glm::mat4 m_currentProj{1.0f};
    glm::mat4 m_currentView{1.0f};
    int m_frameIndex = 0;
    Shader* m_zPrepassShader = nullptr;
    Shader* m_gtaoShader = nullptr;
    Shader* m_gtaoBlurShader = nullptr;
    Shader* m_aoMultiplyShader = nullptr;
    Shader* m_fogShader = nullptr;
    Shader* m_fogCompositeShader = nullptr;
    Shader* m_lightShaftsShader = nullptr;
};
