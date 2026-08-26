#pragma once
#include "engine/Engine.h"
#include "engine/Physics/Physics.h"
#include "engine/Physics/ClothCape.h"
#include "engine/Physics/Vehicle.h"
#include "engine/Renderer/ParticleSystem.h"
#include <memory>
#include <vector>

// ============================================================================
// Grand Highway Tech Showcase
// ============================================================================
class Demo : public Application {
public:
    Demo();
    ~Demo() override;

protected:
    void onInit() override;
    void onFixedUpdate(float fixedDt) override;
    void onUpdate(float dt) override;
    void onImGuiRender() override;
    void onShutdown() override;

private:
    void buildWorld();
    void spawnPlayer();
    void handleInput(float dt);
    void updateCamera(float dt);
    void renderHUD(float dt, int w, int h);

    // Core subsystems
    std::unique_ptr<Shader> m_litShader;
    std::unique_ptr<RenderPipeline> m_pipe;
    std::unique_ptr<cjoka_phys::World> m_phys;
    Entity m_camera = NullEntity;

    // Player
    void* m_cct = nullptr;
    glm::vec3 m_playerPos{-3.5f, 1.1f, -202.0f};
    glm::vec3 m_playerVel{0.0f};
    glm::vec3 m_playerMove{0.0f};
    bool m_onGround = false;
    Entity m_playerModel = NullEntity;

    // Vehicles
    std::vector<std::unique_ptr<Physics::Vehicle>> m_vehicles;
    Physics::Vehicle* m_playerVehicle = nullptr;
    bool m_canEnterVehicle = false;

    // Effects
    std::unique_ptr<Physics::ClothCape> m_cape;
    std::unique_ptr<ParticleFX::ParticleSystem> m_particles;
    std::vector<glm::vec3> m_firePositions;
    std::vector<Entity> m_dynamicObjects;

    // Camera
    //   Convention: yaw=90 degrees = looking along +Z (world forward on the highway)
    //   Mouse right -> yaw decreases -> camera looks right
    //   Mouse up    -> pitch increases -> camera looks up
    bool m_cursorLocked = false;
    bool m_thirdPerson = true;
    float m_camPitch = -8.0f;
    float m_camYaw = 90.0f;
    float m_targetPitch = -8.0f;
    float m_targetYaw = 90.0f;
    glm::vec3 m_smoothCamPos{-3.5f, 2.5f, -206.0f};
    float m_camFov = 60.0f;
};
