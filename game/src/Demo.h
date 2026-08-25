#pragma once
#include "engine/Engine.h"
#include "engine/Physics/Physics.h"
#include <memory>

// Демка cjoka: рендер + PhysX 5.5 (классика: гравитация, динамика, Character Controller).
class Demo : public Application {
public:
    Demo();
    ~Demo() override;
protected:
    void onInit() override;
    void onUpdate(float dt) override;
    void onShutdown() override;
private:
    void setupWorld();
    void setupPlayer();
    void setupGUI();
    void handlePlayerInput(float dt);
    void updateHUD(float dt, int w, int h);

    std::unique_ptr<Shader> m_litShader;
    Entity m_camera = NullEntity;
    std::unique_ptr<cjoka_phys::World> m_phys;

    // игрок (CCT)
    void* m_cct = nullptr;
    glm::vec3 m_playerPos{0, 1.0f, 4};
    glm::vec3 m_playerVel{0};
    bool m_onGround = false;
    float m_yaw = -90.f;

    // спавн кубиков по клавише
    std::vector<Entity> m_debris;
    std::unique_ptr<RenderPipeline> m_pipe;
    int m_spawned = 0;
};
