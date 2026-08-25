#include "engine/Renderer/Renderer.h"
#include <glad/gl.h>

namespace Renderer {
void SetClearColor(float r,float g,float b,float a){ glClearColor(r,g,b,a); }
void Clear(float r,float g,float b,float a){ glClearColor(r,g,b,a); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }
void Clear(){ glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }
void SetViewport(int x,int y,int w,int h){ glViewport(x,y,w,h); }
void EnableDepthTest(bool e){ if(e) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST); }
}
