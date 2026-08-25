#pragma once
#include "engine/Renderer/Framebuffer.h"
#include "engine/Renderer/Shader.h"
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
        float vignette = 0.32f;
        float exposure = 1.0f;
        float gamma = 2.2f;
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

    Framebuffer& hdrFBO() { return m_hdr; }
    GLuint hdrTexture() const { return m_hdr.colorTexture(); }

private:
    void ensurePostShaders();

    int m_w=0, m_h=0;
    Settings m_settings;
    Framebuffer m_hdr;          // full res HDR
    Framebuffer m_bloomExtract; // half res
    Framebuffer m_bloomPing;    // half res
    Framebuffer m_bloomPong;    // half res
    Framebuffer m_composite;    // full res (для FXAA)
};
