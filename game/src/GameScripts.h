#pragma once
#include "engine/Scripting/ScriptableEntity.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>

// 1. Rotator Script
class RotatorScript : public ScriptableEntity {
public:
    float speed = 45.0f; // degrees per second
    glm::vec3 axis = {0.0f, 1.0f, 0.0f};

    void onUpdate(float dt) override {
        transform().rotation += axis * speed * dt;
    }

    void onInspectorGUI() override {
        ImGui::Text("Rotator Component");
        ImGui::SliderFloat("Speed (deg/s)", &speed, -360.0f, 360.0f);
        ImGui::DragFloat3("Rotation Axis", &axis.x, 0.05f, -1.0f, 1.0f);
        if (ImGui::Button("Spin Y Axis")) axis = {0.0f, 1.0f, 0.0f};
        ImGui::SameLine();
        if (ImGui::Button("Spin X Axis")) axis = {1.0f, 0.0f, 0.0f};
    }
};
REGISTER_SCRIPT(RotatorScript, "Rotator")

// 2. Police Siren Script
class PoliceSirenScript : public ScriptableEntity {
public:
    float flashSpeed = 9.0f;
    float peakIntensity = 10.0f;
    glm::vec3 redColor = {1.0f, 0.05f, 0.08f};
    glm::vec3 blueColor = {0.08f, 0.35f, 1.0f};
    bool active = true;

    void onUpdate(float dt) override {
        (void)dt;
        if (!active) return;
        float t = (float)glfwGetTime() * flashSpeed;
        float wave = std::sin(t);

        if (has<PointLight>()) {
            auto& pl = get<PointLight>();
            pl.color = (wave > 0.0f) ? redColor : blueColor;
            pl.intensity = std::abs(wave) * peakIntensity;
        }

        if (has<MeshRenderer>()) {
            auto& mr = get<MeshRenderer>();
            mr.material.emissive = (wave > 0.0f ? redColor : blueColor) * std::abs(wave) * (peakIntensity * 1.5f);
        }
    }

    void onInspectorGUI() override {
        ImGui::Text("Police Siren Light FX");
        ImGui::Checkbox("Active", &active);
        ImGui::SliderFloat("Strobe Speed", &flashSpeed, 1.0f, 30.0f);
        ImGui::SliderFloat("Peak Intensity", &peakIntensity, 1.0f, 30.0f);
        ImGui::ColorEdit3("Color A", &redColor.x);
        ImGui::ColorEdit3("Color B", &blueColor.x);
    }
};
REGISTER_SCRIPT(PoliceSirenScript, "Police Siren")

// 3. Flicker Light Script
class FlickerLightScript : public ScriptableEntity {
public:
    float baseIntensity = 5.0f;
    float flickerScale = 2.0f;
    float speed = 12.0f;

    void onUpdate(float dt) override {
        (void)dt;
        if (!has<PointLight>()) return;
        auto& pl = get<PointLight>();
        float t = (float)glfwGetTime() * speed;
        float noise = std::sin(t) * 0.5f + std::sin(t * 2.3f) * 0.3f + std::sin(t * 5.7f) * 0.2f;
        pl.intensity = std::max(0.1f, baseIntensity + noise * flickerScale);
    }

    void onInspectorGUI() override {
        ImGui::Text("Flicker Light FX (Candle / Faulty Neon)");
        ImGui::SliderFloat("Base Intensity", &baseIntensity, 0.5f, 20.0f);
        ImGui::SliderFloat("Flicker Amount", &flickerScale, 0.1f, 10.0f);
        ImGui::SliderFloat("Frequency", &speed, 1.0f, 40.0f);
    }
};
REGISTER_SCRIPT(FlickerLightScript, "Flicker Light")

// 4. Hover Floating Script (Item / Vehicle Hover)
class HoverFloatingScript : public ScriptableEntity {
public:
    float amplitude = 0.35f;
    float frequency = 2.5f;
    float initialY = 0.0f;
    bool inited = false;

    void onStart() override {
        initialY = transform().position.y;
        inited = true;
    }

    void onUpdate(float dt) override {
        (void)dt;
        if (!inited) onStart();
        float t = (float)glfwGetTime() * frequency;
        transform().position.y = initialY + std::sin(t) * amplitude;
        transform().rotation.y += 20.0f * dt;
    }

    void onInspectorGUI() override {
        ImGui::Text("Hover / Floating FX");
        ImGui::SliderFloat("Bob Height", &amplitude, 0.05f, 2.0f);
        ImGui::SliderFloat("Bob Speed", &frequency, 0.5f, 10.0f);
        if (ImGui::Button("Reset Base Height")) initialY = transform().position.y;
    }
};
REGISTER_SCRIPT(HoverFloatingScript, "Hover Floating")

// 5. Vehicle WASD Controller Script
class VehicleDriverScript : public ScriptableEntity {
public:
    float maxSpeed = 18.0f;
    float acceleration = 15.0f;
    float steerSpeed = 65.0f;
    float currentSpeed = 0.0f;

    void onUpdate(float dt) override {
        bool forward = Input::IsKeyPressed(GLFW_KEY_I) || Input::IsKeyPressed(GLFW_KEY_UP);
        bool backward = Input::IsKeyPressed(GLFW_KEY_K) || Input::IsKeyPressed(GLFW_KEY_DOWN);
        bool left = Input::IsKeyPressed(GLFW_KEY_J) || Input::IsKeyPressed(GLFW_KEY_LEFT);
        bool right = Input::IsKeyPressed(GLFW_KEY_L) || Input::IsKeyPressed(GLFW_KEY_RIGHT);

        if (forward) currentSpeed = std::min(currentSpeed + acceleration * dt, maxSpeed);
        else if (backward) currentSpeed = std::max(currentSpeed - acceleration * dt, -maxSpeed * 0.5f);
        else currentSpeed *= std::max(0.0f, 1.0f - 3.0f * dt); // drag

        if (left) transform().rotation.y += steerSpeed * dt * (currentSpeed >= 0.0f ? 1.0f : -1.0f);
        if (right) transform().rotation.y -= steerSpeed * dt * (currentSpeed >= 0.0f ? 1.0f : -1.0f);

        float yawRad = glm::radians(transform().rotation.y);
        glm::vec3 dir = { std::sin(yawRad), 0.0f, std::cos(yawRad) };
        transform().position += dir * currentSpeed * dt;
    }

    void onInspectorGUI() override {
        ImGui::Text("Vehicle Controller (Controls: IJKL / Arrows)");
        ImGui::ProgressBar(std::abs(currentSpeed) / maxSpeed, ImVec2(0, 0), "Speed");
        ImGui::SliderFloat("Max Speed", &maxSpeed, 5.0f, 50.0f);
        ImGui::SliderFloat("Acceleration", &acceleration, 5.0f, 40.0f);
        ImGui::SliderFloat("Steering Speed", &steerSpeed, 20.0f, 120.0f);
    }
};
REGISTER_SCRIPT(VehicleDriverScript, "Vehicle Driver")

// 6. First Person Camera Controller Script
class FirstPersonCameraScript : public ScriptableEntity {
public:
    float moveSpeed = 10.0f;
    float sprintMultiplier = 2.0f;
    float mouseSensitivity = 0.12f;
    bool invertY = false;

    void onUpdate(float dt) override {
        // Mouse Look
        if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            glm::vec2 delta = Input::GetMouseDelta();
            transform().rotation.y += delta.x * mouseSensitivity;
            transform().rotation.x += delta.y * mouseSensitivity * (invertY ? 1.0f : -1.0f);
            transform().rotation.x = glm::clamp(transform().rotation.x, -89.0f, 89.0f);
        }

        // Movement
        float speed = moveSpeed * (Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT) ? sprintMultiplier : 1.0f);
        glm::vec3 moveDir{0.0f};
        if (Input::IsKeyPressed(GLFW_KEY_W)) moveDir += transform().forward();
        if (Input::IsKeyPressed(GLFW_KEY_S)) moveDir -= transform().forward();
        if (Input::IsKeyPressed(GLFW_KEY_D)) moveDir += transform().right();
        if (Input::IsKeyPressed(GLFW_KEY_A)) moveDir -= transform().right();
        if (Input::IsKeyPressed(GLFW_KEY_SPACE)) moveDir += glm::vec3(0, 1, 0);
        if (Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL)) moveDir -= glm::vec3(0, 1, 0);

        if (glm::length(moveDir) > 0.0f) {
            transform().position += glm::normalize(moveDir) * speed * dt;
        }
    }

    void onInspectorGUI() override {
        ImGui::Text("First Person Camera Controller");
        ImGui::SliderFloat("Move Speed", &moveSpeed, 1.0f, 50.0f);
        ImGui::SliderFloat("Sprint Multiplier", &sprintMultiplier, 1.0f, 5.0f);
        ImGui::SliderFloat("Sensitivity", &mouseSensitivity, 0.01f, 1.0f);
        ImGui::Checkbox("Invert Y", &invertY);
    }
};
REGISTER_SCRIPT(FirstPersonCameraScript, "First Person Camera")

// 7. Smooth Camera Follow Script
class CameraFollowScript : public ScriptableEntity {
public:
    glm::vec3 offset{0.0f, 3.5f, -8.0f};
    float smoothSpeed = 6.0f;
    float lookAhead = 2.0f;

    void onUpdate(float dt) override {
        // Look for target entity (e.g. car or player in scene)
        Transform* targetTr = nullptr;
        for (Entity e : sceneRegistry().view<Transform>()) {
            if (e != entity()) {
                if (sceneRegistry().has<Name>(e)) {
                    const auto& name = sceneRegistry().get<Name>(e).value;
                    if (name.find("Car") != std::string::npos || name.find("Vehicle") != std::string::npos || name.find("Player") != std::string::npos || name.find("Sedan") != std::string::npos || name.find("Police") != std::string::npos) {
                        targetTr = &sceneRegistry().get<Transform>(e);
                        break;
                    }
                }
            }
        }

        if (targetTr) {
            float yawRad = glm::radians(targetTr->rotation.y);
            glm::vec3 fwd = { std::sin(yawRad), 0.0f, std::cos(yawRad) };
            glm::vec3 desiredPos = targetTr->position - fwd * (-offset.z) + glm::vec3(0, offset.y, 0);
            transform().position = glm::mix(transform().position, desiredPos, std::clamp(smoothSpeed * dt, 0.0f, 1.0f));

            // Look at target + look ahead
            glm::vec3 targetLook = targetTr->position + fwd * lookAhead + glm::vec3(0, 1.0f, 0);
            glm::vec3 dir = glm::normalize(targetLook - transform().position);
            float pitch = glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f)));
            float yaw = glm::degrees(std::atan2(dir.x, dir.z));
            transform().rotation = {pitch, yaw, 0.0f};
        }
    }

    void onInspectorGUI() override {
        ImGui::Text("Smooth Camera Follow");
        ImGui::DragFloat3("Follow Offset", &offset.x, 0.1f);
        ImGui::SliderFloat("Smooth Speed", &smoothSpeed, 1.0f, 20.0f);
        ImGui::SliderFloat("Look Ahead", &lookAhead, 0.0f, 10.0f);
    }
};
REGISTER_SCRIPT(CameraFollowScript, "Camera Follow")

// 8. Orbit Camera Script
class OrbitCameraScript : public ScriptableEntity {
public:
    glm::vec3 target{0.0f, 1.0f, 0.0f};
    float distance = 12.0f;
    float speed = 25.0f; // deg/s
    float height = 4.0f;
    float currentAngle = 0.0f;

    void onUpdate(float dt) override {
        currentAngle += speed * dt;
        float rad = glm::radians(currentAngle);
        transform().position = target + glm::vec3(std::cos(rad) * distance, height, std::sin(rad) * distance);

        glm::vec3 dir = glm::normalize(target - transform().position);
        float pitch = glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f)));
        float yaw = glm::degrees(std::atan2(dir.x, dir.z));
        transform().rotation = {pitch, yaw, 0.0f};
    }

    void onInspectorGUI() override {
        ImGui::Text("Orbit Camera Showcase");
        ImGui::DragFloat3("Orbit Target", &target.x, 0.1f);
        ImGui::SliderFloat("Distance", &distance, 2.0f, 50.0f);
        ImGui::SliderFloat("Height", &height, 0.0f, 20.0f);
        ImGui::SliderFloat("Rotation Speed", &speed, -180.0f, 180.0f);
    }
};
REGISTER_SCRIPT(OrbitCameraScript, "Orbit Camera")
