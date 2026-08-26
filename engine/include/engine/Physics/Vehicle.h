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
// AAA Drivable Vehicle & Traffic Physics System (GTA Style)
// Поддержка:
// - Управление игроком (W/S газ/тормоз/задний ход, A/D плавный руль, Space дрифт)
// - Поворот передних колес (Steering angle)
// - Вращение 4 колес пропорционально скорости
// - Крен кузова при поворотах (Body Roll) и клевок при торможении (Pitch)
// - Реалистичный разгон, инерция, торможение и дрифт
// - Столкновения и расталкивание PhysX объектов с кинетической силой
// - Фары, стоп-сигналы и динамический свет
// ============================================================================

struct WheelEntity {
    EntityRef entity;
    glm::vec3 localOffset;
    float currentAngle = 0.0f;
    bool isFront = false;
};

class Vehicle {
public:
    Vehicle(Scene& scene, const glm::vec3& startPos, float yaw, const std::string& modelPath, float maxSpeed = 18.0f)
        : m_scene(&scene), m_pos(startPos), m_yaw(yaw), m_modelPath(modelPath), m_maxForwardSpeed(maxSpeed) {
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

    void update(float dt, cjoka_phys::World* physWorld) {
        if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

        if (m_isPlayerControlled) {
            updatePlayerDriving(dt, physWorld);
        } else {
            updateAITraffic(dt, physWorld);
        }

        syncVisuals(dt);

        // Силовое взаимодействие с препятствиями и бочками
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
        // Инвертируем m_steerInput, чтобы D (+1) поворачивал вправо (уменьшал yaw)
        float targetSteerAngle = -m_steerInput * m_maxSteerDegrees;
        float steerSpeed = 85.0f; // deg/sec
        if (m_currentSteerAngle < targetSteerAngle) {
            m_currentSteerAngle = std::min(targetSteerAngle, m_currentSteerAngle + steerSpeed * dt);
        } else if (m_currentSteerAngle > targetSteerAngle) {
            m_currentSteerAngle = std::max(targetSteerAngle, m_currentSteerAngle - steerSpeed * dt);
        }

        // 2. Разгон, торможение, накат
        float accelRate = 12.0f;  // м/с^2
        float brakeRate = 18.0f;  // м/с^2
        float coastDrag = 2.5f;   // сопротивление воздуха
        float reverseMax = -6.5f;

        if (m_handbrake) {
            // Ручник: резкое торможение и возможность дрифта
            if (m_speed > 0.0f) m_speed = std::max(0.0f, m_speed - brakeRate * 1.5f * dt);
            else if (m_speed < 0.0f) m_speed = std::min(0.0f, m_speed + brakeRate * 1.5f * dt);
        } else if (m_throttleInput > 0.05f) {
            if (m_speed < 0.0f) {
                // Торможение при движении назад
                m_speed += brakeRate * dt;
            } else {
                m_speed = std::min(m_maxForwardSpeed, m_speed + accelRate * m_throttleInput * dt);
            }
        } else if (m_throttleInput < -0.05f) {
            if (m_speed > 0.0f) {
                // Торможение при движении вперед
                m_speed = std::max(0.0f, m_speed - brakeRate * std::abs(m_throttleInput) * dt);
            } else {
                // Задний ход
                m_speed = std::max(reverseMax, m_speed - accelRate * 0.6f * std::abs(m_throttleInput) * dt);
            }
        } else {
            // Накат с затуханием
            if (m_speed > 0.0f) m_speed = std::max(0.0f, m_speed - coastDrag * dt);
            else if (m_speed < 0.0f) m_speed = std::min(0.0f, m_speed + coastDrag * dt);
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

        // Ограничение по границам сцены
        m_pos.x = std::clamp(m_pos.x, -7.5f, 7.5f);
        m_pos.z = std::clamp(m_pos.z, -245.0f, 245.0f);
        m_pos.y = 0.0f;

        // 5. Динамика кузова (Body Roll & Pitch)
        float targetRoll = -m_currentSteerAngle * (m_speed / m_maxForwardSpeed) * 0.35f;
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

        if (m_pos.z < -240.0f) {
            m_pos.z = 240.0f;
        } else if (m_pos.z > 240.0f) {
            m_pos.z = -240.0f;
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
        float wheelRadius = 0.35f;
        float wheelRotSpeed = (m_speed / wheelRadius) * 57.29578f; // deg/sec
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
            m_leftLight.transform().position = m_pos + glm::vec3(rotM * glm::vec4(-0.55f, 0.45f, 1.6f, 1.0f));
        }
        if (m_rightLight.valid()) {
            m_rightLight.transform().position = m_pos + glm::vec3(rotM * glm::vec4(0.55f, 0.45f, 1.6f, 1.0f));
        }
    }

    void initVehicle() {
        auto colorTex = Assets::Texture("assets/textures/colormap.png", true);
        Material carMat = Material::Textured(colorTex, glm::vec3(1.0f), 0.4f, 0.1f);
        Material wheelMat = Material::Textured(colorTex, glm::vec3(1.0f), 0.3f, 0.3f);

        auto carMesh = Assets::Mesh(m_modelPath);
        auto wheelMesh = Assets::Mesh("assets/models/cars/wheel.obj");

        // 1. Цельный кузов реальной модели машины
        m_chassis = m_scene->create("CarBody", Transform{m_pos, {0.0f, m_yaw, 0.0f}, glm::vec3(1.25f)});
        m_chassis.add<MeshRenderer>(MeshRenderer(carMesh, carMat).setClusterLOD(false));

        // 2. 4 Реалистичных колеса (точная посадка на оси)
        struct WheelDef { glm::vec3 offset; bool isFront; };
        WheelDef wheelDefs[4] = {
            {{-0.75f, 0.36f, 0.95f}, true},  // FL
            {{ 0.75f, 0.36f, 0.95f}, true},  // FR
            {{-0.75f, 0.36f, -0.95f}, false}, // RL
            {{ 0.75f, 0.36f, -0.95f}, false}  // RR
        };

        for (int i = 0; i < 4; ++i) {
            bool isLeft = (wheelDefs[i].offset.x < 0.0f);
            float baseYaw = isLeft ? (m_yaw + 180.0f) : m_yaw;
            EntityRef wRef = m_scene->create("Wheel_" + std::to_string(i), Transform{m_pos + wheelDefs[i].offset, {0.0f, baseYaw, 0.0f}, glm::vec3(1.25f)});
            wRef.add<MeshRenderer>(MeshRenderer(wheelMesh, wheelMat).setClusterLOD(false));
            m_wheels.push_back({wRef, wheelDefs[i].offset, 0.0f, wheelDefs[i].isFront});
        }

        // 3. Источники света фар
        m_leftLight = m_scene->createPointLight(Transform{m_pos + glm::vec3(-0.55f, 0.45f, 1.8f)}, PointLight::Warm({1.0f, 0.95f, 0.85f}, 7.0f, 1.8f), "CarLightL");
        m_rightLight = m_scene->createPointLight(Transform{m_pos + glm::vec3(0.55f, 0.45f, 1.8f)}, PointLight::Warm({1.0f, 0.95f, 0.85f}, 7.0f, 1.8f), "CarLightR");
    }

    Scene* m_scene = nullptr;
    glm::vec3 m_pos{0.0f};
    float m_yaw = 0.0f;
    std::string m_modelPath;
    float m_speed = 0.0f;
    float m_maxForwardSpeed = 18.0f;

    // Управление
    bool m_isPlayerControlled = false;
    float m_throttleInput = 0.0f;
    float m_steerInput = 0.0f;
    bool m_handbrake = false;
    float m_currentSteerAngle = 0.0f;
    float m_maxSteerDegrees = 32.0f;
    float m_bodyRoll = 0.0f;
    float m_bodyPitch = 0.0f;

    EntityRef m_chassis;
    std::vector<WheelEntity> m_wheels;
    EntityRef m_leftLight;
    EntityRef m_rightLight;
};

} // namespace Physics

