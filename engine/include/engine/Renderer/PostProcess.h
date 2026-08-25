#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>

// PostProcess — полноэкранный quad + шейдеры для RAGE-подобных эффектов
// Использование: PostProcess::Init(w,h); PostProcess::Blit(tex); etc.
namespace PostProcess {

void Init(int w, int h);
void Shutdown();
void Resize(int w, int h);

// Базовый fullscreen треугольник (без VBO — vertexID)
void DrawFullscreen();

// Эффекты (вход — текстура, выход — на текущий FBO или default)
void Tonemap(GLuint hdrTex, float exposure = 1.0f, float gamma = 2.2f);
void BloomExtract(GLuint hdrTex, GLuint outFBO, float threshold = 1.0f);
void Blur(GLuint tex, GLuint outFBO, glm::vec2 dir); // dir = (1,0) или (0,1)
void Composite(GLuint sceneTex, GLuint bloomTex, float bloomIntensity = 0.6f, float vignette = 0.35f,
               float exposure = 1.0f, float gamma = 2.2f, float saturation = 1.06f);
void FXAA(GLuint tex);

} // namespace PostProcess
