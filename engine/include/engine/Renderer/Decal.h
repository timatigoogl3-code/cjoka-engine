#pragma once
#include <memory>
#include <glm/glm.hpp>
#include <glad/gl.h>
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"

class Shader;

namespace Renderer {

class DecalSystem {
public:
    static void Init();
    static void Shutdown();

    // Отрисовка всех декалей в ECS (Projected OBB + Planar)
    static void Render(Registry& reg, const glm::mat4& view, const glm::mat4& proj, GLuint depthTexture = 0);

private:
    static void InitUnitBox();
    static GLuint s_cubeVAO;
    static GLuint s_cubeVBO;
    static GLuint s_cubeEBO;
    static std::unique_ptr<Shader> s_decalShader;
    static bool s_initialized;
};

} // namespace Renderer
