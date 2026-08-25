#pragma once
#include <glm/glm.hpp>

namespace Renderer {
    void Clear(float r, float g, float b, float a = 1.0f);
    void Clear(); // с текущим цветом
    void SetClearColor(float r, float g, float b, float a = 1.0f);
    void SetViewport(int x, int y, int w, int h);
    void EnableDepthTest(bool enable);
}
