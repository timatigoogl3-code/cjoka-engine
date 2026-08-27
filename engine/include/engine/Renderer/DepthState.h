#pragma once
#include <glad/gl.h>

// Standard OpenGL depth state: far=1.0, near=0.0
namespace DepthState {
    constexpr GLenum kFunc = GL_LESS;           // depth comparison: pass if incoming < stored
    constexpr float  kClear = 1.0f;             // clear value (far plane = 1)
    constexpr float  kNear = 0.0f;              // near plane = 0
    constexpr float  kFar  = 1.0f;              // far plane = 1
    inline void apply() { glDepthFunc(kFunc); } // set current depth func
    inline void clear() { glClearDepthf(kClear); }
}
