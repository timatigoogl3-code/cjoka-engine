#pragma once
#include "engine/Scene/Scene.h"
#include <vector>
#include <cmath>
#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Physics {

// ============================================================================
// Universal Drivable Vehicle & Traffic Physics System
// Полностью конфигурируемый компонент/класс физики транспортного средства:
// - Управление игроком (акселерация, торможение, реверс, рулевое управление, дрифт)
// - Поворот передних колес и динамика вращения
// - Крен кузова при поворотах (Body Roll) и клевок при торможении (Pitch)
// - Конфигурируемые геометрические оффсеты колес и фар
// - Опциональные границы перемещения (без захардкоженных констант)
// ============================================================================

struct WheelEntity {
    EntityRef entity;
    glm::vec3 localOffset{0.0f};
    float currentAngle = 0.0f;
    bool isFront = false;
};

struct VehicleParams {
    float maxForwardSpeed = 18.0f;
    float maxReverseSpeed = -6.5f;
    float accelRate = 12.0f;
    float brakeRate = 18.0f;
    float coastDrag = 2.5f;
    float maxSteerDegrees = 32.0f;
    float steerSpeed = 85.0f;
    float wheelRadius = 0.35f;
    float bodyScale = 1.25f;

    bool enableBounds = false;
    glm::vec2 boundsX{-1000.0f, 1000.0f};
    glm::vec2 boundsZ{-1000.0f, 1000.0f};

    std::vector<glm::vec3> wheelOffsets = {
        {-0.75f, 0.36f, 0.95f},
        { 0.75f, 0.36f, 0.95f},
        {-0.75f, 0.36f, -0.95f},
        { 0.75f, 0.36f, -0.95f}
    };
    glm::vec3 headlightLeftOffset{-0.55f, 0.45f, 1.8f};
    glm::vec3 headlightRightOffset{0.55f, 0.45f, 1.8f};
};

class Vehicle {
public:
    Vehicle(Scene& scene, const glm::vec3& startPos, float yaw, const std::string& modelPath, float maxSpeed = 18.0f,
            const std::string& wheelMeshPath = "", const std::string& texturePath = "")
        : m_scene(&scene), m_pos(startPos), m_yaw(yaw), m_modelPath(modelPath), m_wheelMeshPath(wheelMeshPath), m_texturePath(texturePath) {
        m_params.maxForwardSpeed = maxSpeed;
        initVehicle();
    }

    Vehicle(Scene& scene, const glm::vec3& startPos, float yaw, const std::string& modelPath, const VehicleParams& params,
            const std::string& wheelMeshPath = "", const std::string& texturePath = "")
        : m_scene(&scene), m_pos(startPos), m_yaw(yaw), m_modelPath(modelPath), m_wheelMeshPath(wheelMeshPath), m_texturePath(texturePath), m_params(params) {
        initVehicle();
    }

    void setPlayerControlled(bool controlled) {
        m_isPlayerControlled = controlled;
        if (!controlled) {
            m_throttleInput = 0.0f;
            m_steerInput = 0.0f;
            m_handbrake = false;
        }
    }

    bool isPlayerControlled() const { return m_isPlayerControlled; }

    void setInputs(float throttle, float steer, bool handbrake) {
        m_throttleInput = std::clamp(throttle, -1.0f, 1.0f);
        m_steerInput = std::clamp(steer, -1.0f, 1.0f);
        m_handbrake = handbrake;
    }

    void setBounds(bool enable, glm::vec2 boundsX, glm::vec2 boundsZ) {
        m_params.enableBounds = enable;
        m_params.boundsX = boundsX;
        m_params.boundsZ = boundsZ;
    }

    VehicleParams& params() { return m_params; }
    const VehicleParams& params() const { return m_params; }

    void update(float dt, cjoka_phys::World* physWorld) {
        if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

        if (m_isPlayerControlled) {
            updatePlayerDriving(dt, physWorld);
        } else {
            updateAITraffic(dt, physWorld);
        }

        syncVisuals(dt);

        // Силовое взаимодействие с препятствиями
        if (physWorld && std::abs(m_speed) > 0.5f) {
            float yawRad = glm::radians(m_yaw);
            glm::vec3 fwd(std::sin(yawRad), 0.0f, std::cos(yawRad));
            float pushRadius = 1.4f;
            float pushForce = std::min(120.0f, std::abs(m_speed) * 8.0f + 25.0f);
            physWorld->PushAt(m_pos + fwd * 1.8f, fwd * (m_speed > 0.0f ? 1.0f : -1.0f), pushRadius, pushForce);
        }
    }

    const glm::vec3& position() const { return m_pos; }
    float yaw() const { return m_yaw; }
    float speed() const { return m_speed; }
    float speedKmH() const { return m_speed * 3.6f; }
    float steerAngle() const { return m_currentSteerAngle; }
    const std::string& modelPath() const { return m_modelPath; }

    glm::vec3 forward() const {
        float yawRad = glm::radians(m_yaw);
        return glm::vec3(std::sin(yawRad), 0.0f, std::cos(yawRad));
    }

    glm::vec3 driverExitPosition() const {
        float yawRad = glm::radians(m_yaw);
        glm::vec3 fwd(std::sin(yawRad), 0.0f, std::cos(yawRad));
        glm::vec3 right(fwd.z, 0.0f, -fwd.x);
        return m_pos - right * 1.6f + glm::vec3(0.0f, 0.5f, 0.0f);
    }

private:
    void updatePlayerDriving(float dt, cjoka_phys::World* /*physWorld*/) {
        // 1. Рулевое управление с учетом скорости
        float targetSteerAngle = -m_steerInput * m_params.maxSteerDegrees;
        if (m_currentSteerAngle < targetSteerAngle) {
            m_currentSteerAngle = std::min(targetSteerAngle, m_currentSteerAngle + m_params.steerSpeed * dt);
        } else if (m_currentSteerAngle > targetSteerAngle) {
            m_currentSteerAngle = std::max(targetSteerAngle, m_currentSteerAngle - m_params.steerSpeed * dt);
        }

        // 2. Разгон, торможение, накат
        if (m_handbrake) {
            if (m_speed > 0.0f) m_speed = std::max(0.0f, m_speed - m_params.brakeRate * 1.5f * dt);
            else if (m_speed < 0.0f) m_speed = std::min(0.0f, m_speed + m_params.brakeRate * 1.5f * dt);
        } else if (m_throttleInput > 0.05f) {
            if (m_speed < 0.0f) {
                m_speed += m_params.brakeRate * dt;
            } else {
                m_speed = std::min(m_params.maxForwardSpeed, m_speed + m_params.accelRate * m_throttleInput * dt);
            }
        } else if (m_throttleInput < -0.05f) {
            if (m_speed > 0.0f) {
                m_speed = std::max(0.0f, m_speed - m_params.brakeRate * std::abs(m_throttleInput) * dt);
            } else {
                m_speed = std::max(m_params.maxReverseSpeed, m_speed - m_params.accelRate * 0.6f * std::abs(m_throttleInput) * dt);
            }
        } else {
            if (m_speed > 0.0f) m_speed = std::max(0.0f, m_speed - m_params.coastDrag * dt);
            else if (m_speed < 0.0f) m_speed = std::min(0.0f, m_speed + m_params.coastDrag * dt);
        }

        // 3. Поворот шасси (Yaw)
        float turnFactor = (m_speed / 7.0f);
        if (std::abs(m_speed) > 0.1f) {
            float driftMult = m_handbrake ? 1.8f : 1.0f;
            float yawDelta = m_currentSteerAngle * turnFactor * driftMult * (dt * 2.2f);
            m_yaw += yawDelta;
        }

        // 4. Движение
        float yawRad = glm::radians(m_yaw);
        glm::vec3 fwd(std::sin(yawRad), 0.0f, std::cos(yawRad));
        m_pos += fwd * (m_speed * dt);

        if (m_params.enableBounds) {
            m_pos.x = std::clamp(m_pos.x, m_params.boundsX.x, m_params.boundsX.y);
            m_pos.z = std::clamp(m_pos.z, m_params.boundsZ.x, m_params.boundsZ.y);
        }

        // 5. Динамика кузова (Body Roll & Pitch)
        float targetRoll = -m_currentSteerAngle * (m_speed / std::max(m_params.maxForwardSpeed, 1.0f)) * 0.35f;
        float targetPitch = (m_throttleInput > 0.0f ? -0.8f : 0.0f) + (m_throttleInput < 0.0f ? 1.2f : 0.0f);
        m_bodyRoll = glm::mix(m_bodyRoll, targetRoll, 1.0f - std::exp(-12.0f * dt));
        m_bodyPitch = glm::mix(m_bodyPitch, targetPitch, 1.0f - std::exp(-12.0f * dt));
    }

    void updateAITraffic(float dt, cjoka_phys::World* /*physWorld*/) {
        m_currentSteerAngle = 0.0f;
        m_bodyRoll = 0.0f;
        m_bodyPitch = 0.0f;
        m_speed = 6.5f;

        float yawRad = glm::radians(m_yaw);
        glm::vec3 fwd(std::sin(yawRad), 0.0f, std::cos(yawRad));
        m_pos += fwd * (m_speed * dt);

        if (m_params.enableBounds) {
            if (m_pos.z < m_params.boundsZ.x) {
                m_pos.z = m_params.boundsZ.y;
            } else if (m_pos.z > m_params.boundsZ.y) {
                m_pos.z = m_params.boundsZ.x;
            }
        }
    }

    void syncVisuals(float dt) {
        // 1. Позиция и ориентация кузова
        if (m_chassis.valid()) {
            auto& tr = m_chassis.transform();
            tr.position = m_pos;
            tr.rotation = {m_bodyPitch, m_yaw, m_bodyRoll};
        }

        // 2. Вращение и поворот передних колес
        float wheelRotSpeed = (m_speed / std::max(m_params.wheelRadius, 0.05f)) * 57.29578f; // deg/sec
        glm::mat4 rotM = glm::rotate(glm::mat4(1.0f), glm::radians(m_yaw), glm::vec3(0.0f, 1.0f, 0.0f));

        for (size_t i = 0; i < m_wheels.size(); ++i) {
            auto& w = m_wheels[i];
            if (!w.entity.valid()) continue;
            w.currentAngle += wheelRotSpeed * dt;
            auto& tr = w.entity.transform();

            glm::vec3 worldOffset = glm::vec3(rotM * glm::vec4(w.localOffset, 1.0f));
            tr.position = m_pos + worldOffset;

            bool isLeft = (w.localOffset.x < 0.0f);
            float steer = w.isFront ? m_currentSteerAngle : 0.0f;
            float baseYaw = isLeft ? (m_yaw + 180.0f + steer) : (m_yaw + steer);
            float spin = isLeft ? -w.currentAngle : w.currentAngle;
            tr.rotation = glm::vec3(spin, baseYaw, 0.0f);
        }

        // 3. Передние фары
        if (m_leftLight.valid()) {
            m_leftLight.transform().position = m_pos + glm::vec3(rotM * glm::vec4(m_params.headlightLeftOffset, 1.0f));
        }
        if (m_rightLight.valid()) {
            m_rightLight.transform().position = m_pos + glm::vec3(rotM * glm::vec4(m_params.headlightRightOffset, 1.0f));
        }
    }

    void initVehicle() {
        Material carMat = Material::Default();
        Material wheelMat = Material::Default();
        if (!m_texturePath.empty()) {
            auto colorTex = Assets::Texture(m_texturePath, true);
            if (colorTex && colorTex->valid()) {
                carMat = Material::Textured(colorTex, glm::vec3(1.0f), 0.4f, 0.1f);
                wheelMat = Material::Textured(colorTex, glm::vec3(1.0f), 0.3f, 0.3f);
            }
        }

        auto carMesh = (!m_modelPath.empty()) ? Assets::Mesh(m_modelPath) : Assets::Cube(1.0f);
        std::shared_ptr<Mesh3D> wheelMesh = (!m_wheelMeshPath.empty()) ? Assets::Mesh(m_wheelMeshPath) : Assets::Sphere(m_params.wheelRadius);

        // 1. Кузов
        m_chassis = m_scene->create("CarBody", Transform{m_pos, {0.0f, m_yaw, 0.0f}, glm::vec3(m_params.bodyScale)});
        m_chassis.add<MeshRenderer>(MeshRenderer(carMesh, carMat).setClusterLOD(false));

        // 2. Колеса
        for (size_t i = 0; i < m_params.wheelOffsets.size(); ++i) {
            const auto& offset = m_params.wheelOffsets[i];
            bool isFront = (offset.z > 0.0f);
            bool isLeft = (offset.x < 0.0f);
            float baseYaw = isLeft ? (m_yaw + 180.0f) : m_yaw;
            EntityRef wRef = m_scene->create("Wheel_" + std::to_string(i), Transform{m_pos + offset, {0.0f, baseYaw, 0.0f}, glm::vec3(m_params.bodyScale)});
            wRef.add<MeshRenderer>(MeshRenderer(wheelMesh, wheelMat).setClusterLOD(false));
            m_wheels.push_back({wRef, offset, 0.0f, isFront});
        }

        // 3. Фары
        m_leftLight = m_scene->createPointLight(Transform{m_pos + m_params.headlightLeftOffset}, PointLight::Warm({1.0f, 0.95f, 0.85f}, 7.0f, 1.8f), "CarLightL");
        m_rightLight = m_scene->createPointLight(Transform{m_pos + m_params.headlightRightOffset}, PointLight::Warm({1.0f, 0.95f, 0.85f}, 7.0f, 1.8f), "CarLightR");
    }

    Scene* m_scene = nullptr;
    glm::vec3 m_pos{0.0f};
    float m_yaw = 0.0f;
    std::string m_modelPath;
    std::string m_wheelMeshPath;
    std::string m_texturePath;
    VehicleParams m_params;

    float m_speed = 0.0f;
    bool m_isPlayerControlled = false;
    float m_throttleInput = 0.0f;
    float m_steerInput = 0.0f;
    bool m_handbrake = false;
    float m_currentSteerAngle = 0.0f;
    float m_bodyRoll = 0.0f;
    float m_bodyPitch = 0.0f;

    EntityRef m_chassis;
    std::vector<WheelEntity> m_wheels;
    EntityRef m_leftLight;
    EntityRef m_rightLight;
};

} // namespace Physics

