#pragma once
#include "engine/Engine.h"
#include "engine/Renderer/RenderPipeline.h"
#include "engine/Renderer/Shader.h"
#include "engine/Renderer/LightProbeGrid.h"
#include "engine/Scene/ChunkManager.h"
#include "engine/Animation/AnimFile.h"
#include "GameScripts.h"
#include <memory>
#include <vector>
#include <string>
#include <filesystem>

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
    void updateScripts(float dt);
    void pickObjectUnderMouse();
    void onFilesDropped(const std::vector<std::string>& paths);
    void renderUI();
    void renderGizmo();
    void renderMenuBar();
    void renderHierarchy();
    void renderInspector();
    void renderAssetBrowser();
    void renderAtmosphereEditor();
    void renderStats();
    void renderConsole();
    void renderCameraPreview();
    void renderGraphicsSettings();
    void renderClusterLODSettings();
    void renderWorldStreaming();
    void renderSceneCameraGizmos();
    void renderColliderGizmos();
    void renderPlayModeOverlay();
    void syncTransformToPhysics(Entity e);
    void propagateTransformDeltaToChildren(Entity parent, const glm::vec3& deltaPos, const glm::vec3& deltaRot);
    void buildStandaloneGame();
    void renderBuildModal();
    void renderDeleteAssetModal();
    void stopPlayMode();

    // Scene management
    void newScene();
    void loadDefaultShowcase();
    void saveSceneToFile(const std::string& path);
    void loadSceneFromFile(const std::string& path);
    void saveSceneToStream(std::ostream& out);
    void loadSceneFromStream(std::istream& in);
    void saveSceneToString(std::string& out);
    void loadSceneFromString(const std::string& in);
    std::string m_playModeSceneBackup;
    void refreshAvailableScenes();
    Entity duplicateEntity(Entity e);
    Entity assembleModularVehicle(const glm::vec3& pos);
    void createNewScriptFile(const std::string& scriptName);
    
    // Spawning helpers
    Entity spawnPrimitive(const std::string& type, const glm::vec3& pos);
    Entity spawnModel(const std::string& name, const std::string& modelPath, const std::string& texPath, const glm::vec3& pos, float scale = 1.0f);
    Entity spawnPointLight(const glm::vec3& pos, const glm::vec3& col = {1.0f, 0.9f, 0.7f}, float intensity = 4.0f, float range = 10.0f);

    void renderMaterialPalette();
    void renderSettingsWindow();
    void renderProfilerWindow();
    void renderWeatherWindow();
    void renderGIWindow();
    void renderUndoHistoryWindow();
    void renderSequencerWindow();
    void renderCreateAssetModal();
    void renderRenameAssetModal();
    void applyLayoutPreset(int presetId);
    void applyMaterialToEntity(Entity e, const std::string& matPath);
    void loadMaterialFromFile(const std::string& path, Material& outMat);
    void saveMaterialToFile(const std::string& path, const Material& mat);
    void createNewMaterialFile(const std::string& matName, const Material& templateMat = Material::Default());
    void createNewFolder(const std::string& folderName);
    void createNewSceneFile(const std::string& sceneName);

    // Core renderer
    std::unique_ptr<Shader> m_litShader;
    std::unique_ptr<RenderPipeline> m_pipe;

    // Fly camera state
    glm::vec3 m_camPos{0.0f, 5.0f, -15.0f};
    float m_camPitch = -15.0f;
    float m_camYaw = 90.0f;
    float m_camSpeed = 12.0f;
    float m_camFov = 65.0f;
    bool m_flycamActive = false;
    bool m_invertY = false;

    // Selection & Editing
    Entity m_selectedEntity = NullEntity;
    int m_gizmoOperation = 0; // 0: Translate, 1: Rotate, 2: Scale
    int m_gizmoMode = 0;      // 0: Local, 1: World
    bool m_useSnap = false;
    float m_snapValue[3] = {0.5f, 0.5f, 0.5f};
    bool m_showUI = true;
    bool m_isPlaying = false; // Simulation / Play Mode

    // Window Visibility Flags (controlled via Windows menu & Layout presets)
    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showAssetBrowser = true;
    bool m_showMaterialPalette = false;
    bool m_showAtmosphereEditor = false;
    bool m_showGraphicsSettings = false;
    bool m_showClusterLODSettings = false;
    bool m_showStats = false;
    bool m_showProfiler = false;
    bool m_showCameraPreview = true;
    bool m_showColliders = true;
    bool m_showConsole = true;
    bool m_showWorldStreaming = false;
    bool m_showWeather = false;
    bool m_showGI = false;
    bool m_showUndoHistory = false;
    bool m_showSequencer = false;
    float m_seqPixelsPerSecond = 80.0f;
    float m_seqScrollTime = 0.0f;
    int m_seqActiveTab = 0; // 0: Dopesheet, 1: Curves
    std::string m_activeAnimClipPath = "";
    std::shared_ptr<Animation::AnimFile> m_activeAnimClip;
    char m_newAnimNameBuf[64] = "MyAnimation";
    char m_eventInputBuf[64] = "CutsceneEvent";
    bool m_seqShowNewAnimModal = false;
    bool m_showSettings = false; // Unified Render & Environment Settings
    int m_settingsTab = 0;
    ChunkManager m_chunkManager;

    Transform m_gizmoStartTransform;
    bool m_gizmoWasUsing = false;

    // Asset Browser filesystem state
    std::filesystem::path m_currentDirectory = "assets";
    std::filesystem::path m_selectedAssetPath = "";
    std::filesystem::path m_assetToDelete = "";
    std::filesystem::path m_assetToRename = "";
    char m_renameBuf[128] = "";
    bool m_showDeleteAssetModal = false;
    bool m_showRenameAssetModal = false;
    bool m_showCreateAssetModal = false;
    int m_createAssetType = 0; // 0: Material, 1: Folder, 2: Scene, 3: Script
    char m_createAssetNameBuf[128] = "NewAsset";
    char m_assetSearchBuf[64] = "";
    std::vector<std::string> m_availableScenes;
    std::vector<std::string> m_availableMaterials;

    // Material Editor selected material
    std::string m_selectedMaterialFile = "";
    Material m_editingMaterial;
    std::unordered_map<std::string, Material> m_materialCache;
    std::vector<std::string> m_cachedMaterialPaletteFiles;
    char m_matSearchBuf[64] = "";

    // 3D Real-time Material Preview
    GLuint m_previewFBO = 0;
    GLuint m_previewColorTex = 0;
    GLuint m_previewDepthRBO = 0;
    std::unique_ptr<Shader> m_previewShader;
    std::shared_ptr<Mesh3D> m_previewSphereMesh;
    float m_previewRotX = 0.35f;
    float m_previewRotY = 0.5f;

    void initMaterialPreview();
    void renderMaterialPreviewSphere(const Material& mat);

    // Search filter in hierarchy
    char m_searchBuf[128] = "";
    char m_sceneFileBuf[128] = "assets/custom_scene.json";
    bool m_showBuildModal = false;
    std::string m_buildLog = "";
    bool m_buildSuccess = false;

    std::unique_ptr<class cjoka_phys::World> m_phys;
};
