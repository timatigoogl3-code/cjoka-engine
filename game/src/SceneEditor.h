#pragma once
#include "engine/Engine.h"
#include "engine/Renderer/RenderPipeline.h"
#include "engine/Renderer/Shader.h"
#include <memory>
#include <vector>
#include <string>

class SceneEditor : public Application {
public:
    SceneEditor();
    ~SceneEditor() override;

protected:
    void onInit() override;
    void onUpdate(float dt) override;
    void onImGuiRender() override;
    void onShutdown() override;

private:
    void updateCamera(float dt);
    void pickObjectUnderMouse();
    void renderUI();
    void renderGizmo();
    void renderMenuBar();
    void renderHierarchy();
    void renderInspector();
    void renderSpawner();
    void renderAtmosphereEditor();
    void renderStats();

    // Scene management
    void newScene();
    void loadDefaultShowcase();
    void saveSceneToFile(const std::string& path);
    void loadSceneFromFile(const std::string& path);
    
    // Spawning helpers
    Entity spawnPrimitive(const std::string& type, const glm::vec3& pos);
    Entity spawnModel(const std::string& name, const std::string& modelPath, const std::string& texPath, const glm::vec3& pos, float scale = 1.0f);
    Entity spawnPointLight(const glm::vec3& pos, const glm::vec3& col = {1.0f, 0.9f, 0.7f}, float intensity = 4.0f, float range = 10.0f);

    // Core renderer
    std::unique_ptr<Shader> m_litShader;
    std::unique_ptr<RenderPipeline> m_pipe;
    Entity m_camera = NullEntity;

    // Fly camera state
    glm::vec3 m_camPos{0.0f, 5.0f, -15.0f};
    float m_camPitch = -15.0f;
    float m_camYaw = 90.0f;
    float m_camSpeed = 12.0f;
    float m_camFov = 65.0f;
    bool m_flycamActive = false;

    // Selection & Editing
    Entity m_selectedEntity = NullEntity;
    int m_gizmoOperation = 0; // 0: Translate, 1: Rotate, 2: Scale
    int m_gizmoMode = 0;      // 0: Local, 1: World
    bool m_useSnap = false;
    float m_snapValue[3] = {0.5f, 0.5f, 0.5f};
    bool m_showUI = true;

    // Search filter in hierarchy
    char m_searchBuf[128] = "";
    char m_sceneFileBuf[128] = "assets/custom_scene.json";
};
