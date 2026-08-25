#pragma once
// CameraControllers — готовые + свой контроллер.
// Движок не ограничивает: используй готовые или пиши свои через Transform+Camera.
//   Systems::FlyCameraSystem(reg, win, dt);            // полёт (есть по умолчанию)
//   Cam::Orbit(reg, win, dt, target, dist);            // облёт вокруг точки
//   Cam::Follow(reg, dt, targetPos, offset);           // следование за объектом
//   Cam::FirstPerson(reg, win, dt, speed);             // от первого лица с гравитацией пола
//   Свой: бери primary Transform/Camera и делай что хочешь — движок не мешает.
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/Core/Window.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace Cam {

inline Entity PrimaryCam(Registry& reg) {
    for (Entity e : reg.view<Camera, Transform>()) if (reg.get<Camera>(e).primary) return e;
    auto v = reg.view<Camera, Transform>();
    return v.empty() ? NullEntity : v[0];
}
inline glm::vec3 Front(const Transform& t) {
    float yaw = glm::radians(t.rotation.y), pitch = glm::radians(t.rotation.x);
    return glm::normalize(glm::vec3{std::cos(yaw)*std::cos(pitch), std::sin(pitch), std::sin(yaw)*std::cos(pitch)});
}

// Орбита: ЛКМ/ПКМ вращение, колесо зум. target — точка интереса.
inline void Orbit(Registry& reg, Window& win, float dt, glm::vec3 target, float dist = 6.0f, float speed = 40.0f) {
    Entity cam = PrimaryCam(reg);
    if (cam == NullEntity) return;
    static float yaw = -90.0f, pitch = -18.0f;
    static bool dragging = false; static double lx=0, ly=0;
    if (win.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) || win.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
        double x,y; win.getCursorPos(x,y);
        if (!dragging) { dragging=true; lx=x; ly=y; }
        yaw   += float(x-lx)*0.25f;
        pitch += float(y-ly)*0.25f;
        pitch = glm::clamp(pitch, -85.0f, 85.0f);
        lx=x; ly=y;
    } else dragging=false;
    // колесо — зум (через scroll недоступен без callback, юзаем +/-)
    if (win.isKeyPressed(GLFW_KEY_KP_ADD) || win.isKeyPressed(GLFW_KEY_EQUAL)) dist *= (1.0f - dt*1.5f);
    if (win.isKeyPressed(GLFW_KEY_KP_SUBTRACT)|| win.isKeyPressed(GLFW_KEY_MINUS))  dist *= (1.0f + dt*1.5f);
    dist = glm::clamp(dist, 1.5f, 60.0f);
    auto& tr = reg.get<Transform>(cam);
    float yr = glm::radians(yaw), pr = glm::radians(pitch);
    tr.position = target + glm::vec3{ std::cos(yr)*std::cos(pr), std::sin(pr), std::sin(yr)*std::cos(pr) } * dist;
    tr.rotation = {pitch, yaw, 0};
}

// Следование за целью со сглаживанием (3-е лицо)
inline void Follow(Registry& reg, float dt, glm::vec3 targetPos, glm::vec3 offset = {0,2.5f,-5}, float lerp = 5.0f) {
    Entity cam = PrimaryCam(reg);
    if (cam == NullEntity) return;
    auto& tr = reg.get<Transform>(cam);
    glm::vec3 want = targetPos + offset;
    tr.position = glm::mix(tr.position, want, glm::clamp(dt*lerp, 0.0f, 1.0f));
    // смотрим на цель
    glm::vec3 d = targetPos - tr.position;
    tr.rotation.y = glm::degrees(std::atan2(d.z, d.x)) - 90.0f;
    tr.rotation.x = glm::degrees(std::asin(glm::clamp(d.y / glm::length(d), -1.0f, 1.0f)));
}

// От первого лица с полом (y >= floorY), ходьба WASD
inline void FirstPerson(Registry& reg, Window& win, float dt, float speed = 4.0f, float floorY = 0.0f, float eyeH = 1.7f) {
    Entity cam = PrimaryCam(reg);
    if (cam == NullEntity) return;
    auto& tr = reg.get<Transform>(cam);
    glm::vec3 front = Front(tr);
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3{0,1,0}));
    glm::vec3 move{};
    if (win.isKeyPressed(GLFW_KEY_W)) move += front;
    if (win.isKeyPressed(GLFW_KEY_S)) move -= front;
    if (win.isKeyPressed(GLFW_KEY_A)) move -= right;
    if (win.isKeyPressed(GLFW_KEY_D)) move += right;
    if (glm::length(move) > 0.001f) tr.position += glm::normalize(move) * speed * dt;
    tr.position.y = glm::max(tr.position.y, floorY + eyeH);
}

} // namespace Cam
