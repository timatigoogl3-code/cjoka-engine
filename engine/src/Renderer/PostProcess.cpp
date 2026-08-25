#include "engine/Renderer/PostProcess.h"
#include "engine/Renderer/Shader.h"
#include "engine/Renderer/DefaultShaders.h"
#include <glad/gl.h>
#include <iostream>

namespace PostProcess {

static GLuint s_quadVAO=0, s_quadVBO=0;
static Shader* s_tonemap=nullptr;
static Shader* s_bloomExtract=nullptr;
static Shader* s_blur=nullptr;
static Shader* s_composite=nullptr;
static Shader* s_fxaa=nullptr;
static int s_w=0,s_h=0;

static void ensureQuad(){
    if(s_quadVAO) return;
    float verts[] = {
        // pos   uv
        -1,-1, 0,0,
         1,-1, 1,0,
         1, 1, 1,1,
        -1,-1, 0,0,
         1, 1, 1,1,
        -1, 1, 0,1
    };
    glGenVertexArrays(1,&s_quadVAO);
    glGenBuffers(1,&s_quadVBO);
    glBindVertexArray(s_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER,s_quadVBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
    glBindVertexArray(0);
}

void Init(int w,int h){
    s_w=w; s_h=h;
    ensureQuad();
    if(!s_tonemap) s_tonemap=new Shader(DefaultShaders::kPostVS, DefaultShaders::kTonemapFS);
    if(!s_bloomExtract) s_bloomExtract=new Shader(DefaultShaders::kPostVS, DefaultShaders::kBloomExtractFS);
    if(!s_blur) s_blur=new Shader(DefaultShaders::kPostVS, DefaultShaders::kBlurFS);
    if(!s_composite) s_composite=new Shader(DefaultShaders::kPostVS, DefaultShaders::kCompositeFS);
    if(!s_fxaa) s_fxaa=new Shader(DefaultShaders::kPostVS, DefaultShaders::kFXAAFS);
    std::cout << "[PostProcess] init " << w << "x" << h << "\n";
}
void Shutdown(){
    if(s_tonemap){ delete s_tonemap; s_tonemap=nullptr; }
    if(s_bloomExtract){ delete s_bloomExtract; s_bloomExtract=nullptr; }
    if(s_blur){ delete s_blur; s_blur=nullptr; }
    if(s_composite){ delete s_composite; s_composite=nullptr; }
    if(s_fxaa){ delete s_fxaa; s_fxaa=nullptr; }
    if(s_quadVAO){ glDeleteVertexArrays(1,&s_quadVAO); s_quadVAO=0; }
    if(s_quadVBO){ glDeleteBuffers(1,&s_quadVBO); s_quadVBO=0; }
}
void Resize(int w,int h){ s_w=w; s_h=h; }

void DrawFullscreen(){
    glBindVertexArray(s_quadVAO);
    glDrawArrays(GL_TRIANGLES,0,6);
    glBindVertexArray(0);
}

void Tonemap(GLuint hdrTex,float exposure,float gamma){
    s_tonemap->use();
    s_tonemap->setInt("uHDR",0);
    s_tonemap->setFloat("uExposure",exposure);
    s_tonemap->setFloat("uGamma",gamma);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,hdrTex);
    DrawFullscreen();
}
void BloomExtract(GLuint hdrTex,GLuint outFBO,float threshold){
    glBindFramebuffer(GL_FRAMEBUFFER,outFBO);
    glViewport(0,0,s_w/2,s_h/2); // bloom at half res (opционально, но рисуем в текущий FBO размер)
    // на самом деле outFBO уже half size — viewport уже установлен вызывающим, но ставим на всякий
    s_bloomExtract->use();
    s_bloomExtract->setInt("uHDR",0);
    s_bloomExtract->setFloat("uThreshold",threshold);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,hdrTex);
    DrawFullscreen();
}
void Blur(GLuint tex,GLuint outFBO,glm::vec2 dir){
    glBindFramebuffer(GL_FRAMEBUFFER,outFBO);
    s_blur->use();
    s_blur->setInt("uTex",0);
    s_blur->setVec2("uDir",dir);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,tex);
    DrawFullscreen();
}
void Composite(GLuint sceneTex,GLuint bloomTex,float bloomIntensity,float vignette,
               float exposure,float gamma,float saturation){
    s_composite->use();
    s_composite->setInt("uScene",0);
    s_composite->setInt("uBloom",1);
    s_composite->setFloat("uBloomIntensity",bloomIntensity);
    s_composite->setFloat("uVignette",vignette);
    s_composite->setFloat("uExposure",exposure);
    s_composite->setFloat("uGamma",gamma);
    s_composite->setFloat("uSaturation",saturation);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,sceneTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,bloomTex);
    DrawFullscreen();
    glActiveTexture(GL_TEXTURE0);
}
void FXAA(GLuint tex){
    s_fxaa->use();
    s_fxaa->setInt("uTex",0);
    s_fxaa->setVec2("uTexel",glm::vec2(1.0f/s_w,1.0f/s_h));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,tex);
    DrawFullscreen();
}

} // namespace PostProcess
