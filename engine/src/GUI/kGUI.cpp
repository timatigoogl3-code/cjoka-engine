#include "engine/GUI/kGUI.h"
#include "engine/GUI/Font.h"
#include "engine/Renderer/Shader.h"
#include "engine/Renderer/DefaultShaders.h"
#include "engine/Core/Window.h"
#include "engine/ECS/Components.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iostream>

namespace kGUI {

struct Vertex { glm::vec2 pos; glm::vec2 uv; glm::vec4 col; };

static Shader* s_shader = nullptr;
static Font* s_font = nullptr;
static GLuint s_vao=0, s_vbo=0;
static glm::mat4 s_proj(1);
static std::vector<Vertex> s_batch;
static int s_fbW=1280, s_fbH=720;

static void ensureVAO(){
    if(s_vao) return;
    glGenVertexArrays(1,&s_vao);
    glGenBuffers(1,&s_vbo);
    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, 1024*1024*sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,col));
    glBindVertexArray(0);
}

bool Init(int fbW,int fbH,const char* fontTTF,float fontSize){
    if(s_shader) return true;
    s_fbW=fbW; s_fbH=fbH;
    s_proj = glm::ortho(0.0f,(float)fbW,(float)fbH,0.0f,-1.0f,1.0f);
    s_shader = new Shader(DefaultShaders::kGUIVS, DefaultShaders::kGUIFS);
    s_font = new Font(fontTTF, fontSize);
    if(!s_font->valid()) std::cerr << "[kGUI] font invalid\n";
    ensureVAO();
    std::cout << "[kGUI] init " << fbW << "x" << fbH << " font=" << (fontTTF?fontTTF:"default") << "\n";
    return true;
}
void Shutdown(){
    if(s_shader){ delete s_shader; s_shader=nullptr; }
    if(s_font){ delete s_font; s_font=nullptr; }
    if(s_vao){ glDeleteVertexArrays(1,&s_vao); s_vao=0; }
    if(s_vbo){ glDeleteBuffers(1,&s_vbo); s_vbo=0; }
    s_batch.clear();
}
void SetWindowSize(int fbW,int fbH){
    s_fbW=fbW; s_fbH=fbH;
    s_proj = glm::ortho(0.0f,(float)fbW,(float)fbH,0.0f,-1.0f,1.0f);
}
void BeginFrame(){
    s_batch.clear();
    // update proj in case resize
}
static void pushQuad(float x,float y,float w,float h, glm::vec4 col, float u0=0,float v0=0,float u1=0,float v1=0){
    // two triangles (6 verts)
    Vertex v[6] = {
        {{x, y}, {u0,v0}, col}, {{x+w, y}, {u1,v0}, col}, {{x+w, y+h}, {u1,v1}, col},
        {{x, y}, {u0,v0}, col}, {{x+w, y+h}, {u1,v1}, col}, {{x, y+h}, {u0,v1}, col}
    };
    s_batch.insert(s_batch.end(), std::begin(v), std::end(v));
}
void Panel(float x,float y,float w,float h, glm::vec4 col,float radius){
    (void)radius; // пока без скругления
    // тень
    pushQuad(x+3,y+3,w,h, glm::vec4(0,0,0, col.a*0.25f));
    pushQuad(x,y,w,h,col);
}
void Text(float x,float y,float scale,const std::string& text,glm::vec4 col){
    if(!s_font || !s_font->valid()) return;
    float ox = x;
    float baseline = y + s_font->ascent() * scale;
    size_t i = 0;
    while (i < text.size()) {
        // обработка \n до декодирования
        if (text[i] == '\n') { x = ox; baseline += s_font->lineHeight() * scale; ++i; continue; }
        uint32_t cp = Font::decodeUTF8(text, i);
        if (cp == '\n') { x = ox; baseline += s_font->lineHeight() * scale; continue; }
        if (cp == ' ') { x += s_font->glyph((uint32_t)' ').xadvance * scale; continue; }
        if (cp == '\t') { x += s_font->glyph((uint32_t)' ').xadvance * 4 * scale; continue; }
        Glyph g = s_font->glyph(cp);
        // если глиф пустой (нет в атласе) — пропустим, но advance всё равно
        if (g.x1 == g.x0 && g.y1 == g.y0) { x += g.xadvance * scale; continue; }
        float gx = x + g.xoff * scale;
        float gy = baseline + g.yoff * scale;
        float gw = (g.x1 - g.x0) * scale;
        float gh = (g.y1 - g.y0) * scale;
        pushQuad(gx, gy, gw, gh, col, g.u0, g.v0, g.u1, g.v1);
        x += g.xadvance * scale;
    }
}
static void flush(){
    if(s_batch.empty() || !s_shader) return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    s_shader->use();
    s_shader->setMat4("uProj", s_proj);
    s_shader->setVec4("uTint", glm::vec4(1));
    // font atlas bound to slot 0
    if(s_font && s_font->valid()){
        s_shader->setBool("uUseFont", true);
        s_shader->setInt("uFont", 0);
        s_font->atlasTexture()->bind(0);
    } else {
        s_shader->setBool("uUseFont", false);
    }
    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(s_batch.size() * sizeof(Vertex)), s_batch.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(s_batch.size()));
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    // cull остаётся выкл глобально
    s_batch.clear();
}
void EndFrame(){ flush(); }

void Draw(Registry& reg, const Window& win){
    int w,h; win.getFramebufferSize(w,h);
    SetWindowSize(w,h);
    BeginFrame();
    // Panels
    for(Entity e: reg.view<Panel2D>()){
        auto& p = reg.get<Panel2D>(e);
        Panel(p.pos.x, p.pos.y, p.size.x, p.size.y, p.color, p.radius);
    }
    // Texts
    for(Entity e: reg.view<Text2D>()){
        auto& t = reg.get<Text2D>(e);
        Text(t.position.x, t.position.y, t.scale, t.text, t.color);
    }
    EndFrame();
}

Font* GetFont(){ return s_font; }
glm::vec2 Measure(const std::string& text,float scale){
    if(!s_font) return {0,0};
    return s_font->measure(text, scale);
}

} // namespace kGUI
