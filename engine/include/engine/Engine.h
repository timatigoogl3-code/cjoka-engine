#pragma once
// Engine.h — единая точка входа в движок cjoka
// Подключи один файл и получи всё: окно, рендер, ECS, GUI.
//   #include "engine/Engine.h"
#include "engine/Core/Window.h"
#include "engine/Core/Application.h"
#include "engine/Scene/Scene.h"
#include "engine/Assets/AssetManager.h"
#include "engine/Assets/AssetBrowser.h"
#include "engine/Renderer/Shader.h"
#include "engine/Renderer/DefaultShaders.h"
#include "engine/Renderer/Mesh3D.h"
#include "engine/Renderer/MeshLoader.h"
#include "engine/Renderer/Texture.h"
#include "engine/Renderer/Renderer.h"
#include "engine/Renderer/Framebuffer.h"
#include "engine/Renderer/PostProcess.h"
#include "engine/Renderer/RenderPipeline.h"
#include "engine/Renderer/Batcher.h"
#include "engine/Scene/SceneLoader.h"
#include "engine/ECS/CameraControllers.h"
#include "engine/World/WorldGen.h"
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/ECS/Systems.h"
#include "engine/GUI/Font.h"
#include "engine/GUI/kGUI.h"

// Удобный хелпер для материала с текстурой
inline Material texturedMaterial(const std::shared_ptr<Texture>& tex, glm::vec3 albedo = glm::vec3(1.0f), float shininess = 48.0f) {
    Material m; m.albedo = albedo; m.diffuseMap = tex; m.useDiffuseMap = tex && tex->valid(); m.shininess = shininess; return m;
}
