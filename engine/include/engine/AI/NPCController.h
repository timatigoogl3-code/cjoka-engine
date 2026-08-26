#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/Animation/Animator.h"

namespace AI {

enum class NPCState {
    Walking,
    Idle,
    Alert
};

struct NPCComponent {
    NPCState state = NPCState::Walking;
    std::vector<glm::vec3> waypoints;
    int currentWaypoint = 0;
    float walkSpeed = 1.35f;
    float turnSpeed = 6.0f;
    float arriveRadius = 0.5f;

    float waitTimer = 0.0f;
    float waitDuration = 2.0f;
    bool pingPong = true;
    bool directionForward = true;

    // Реакция на игрока
    bool lookAtPlayerWhenClose = true;
    float awarenessRadius = 2.5f;

    NPCComponent() = default;
    explicit NPCComponent(std::vector<glm::vec3> wps, float speed = 1.35f)
        : waypoints(std::move(wps)), walkSpeed(speed) {}
};

inline void NPCSystem(Registry& reg, float dt, const glm::vec3& playerPos = glm::vec3(999.0f)) {
    for (Entity e : reg.view<Transform, NPCComponent>()) {
        auto& tr = reg.get<Transform>(e);
        auto& npc = reg.get<NPCComponent>(e);
        auto* animComp = reg.try_get<AnimatorComponent>(e);

        if (npc.waypoints.empty()) continue;

        glm::vec3 target = npc.waypoints[static_cast<size_t>(npc.currentWaypoint)];
        glm::vec3 toTarget = target - tr.position;
        toTarget.y = 0.0f; // движение в горизонтальной плоскости
        float distToTarget = glm::length(toTarget);

        // Проверка дистанции до игрока (реакция)
        float distToPlayer = glm::length(glm::vec3(playerPos.x - tr.position.x, 0.0f, playerPos.z - tr.position.z));
        bool playerNearby = npc.lookAtPlayerWhenClose && (distToPlayer < npc.awarenessRadius);

        if (npc.state == NPCState::Walking) {
            if (animComp && animComp->animator) {
                if (!animComp->animator->isPlaying()) animComp->animator->resume();
                // Синхронизация темпа шагов со скоростью передвижения
                animComp->animator->setSpeed(npc.walkSpeed * 0.75f);
            }

            if (distToTarget <= npc.arriveRadius) {
                // Достигли точки -> короткая пауза и следующий вейпоинт
                npc.state = NPCState::Idle;
                npc.waitTimer = 0.0f;
                npc.waitDuration = 0.8f + static_cast<float>(rand() % 150) / 100.0f;

                // Переключение вейпоинта
                if (npc.pingPong) {
                    if (npc.directionForward) {
                        if (npc.currentWaypoint + 1 < static_cast<int>(npc.waypoints.size())) {
                            npc.currentWaypoint++;
                        } else {
                            npc.directionForward = false;
                            npc.currentWaypoint = std::max(0, static_cast<int>(npc.waypoints.size()) - 2);
                        }
                    } else {
                        if (npc.currentWaypoint - 1 >= 0) {
                            npc.currentWaypoint--;
                        } else {
                            npc.directionForward = true;
                            npc.currentWaypoint = std::min(1, static_cast<int>(npc.waypoints.size()) - 1);
                        }
                    }
                } else {
                    npc.currentWaypoint = (npc.currentWaypoint + 1) % static_cast<int>(npc.waypoints.size());
                }
            } else {
                // Шаг к цели
                glm::vec3 dir = glm::normalize(toTarget);
                tr.position += dir * (npc.walkSpeed * dt);

                // Плавный поворот по направлению движения
                float targetAngle = glm::degrees(std::atan2(dir.x, dir.z));
                float curAngle = tr.rotation.y;
                float diff = std::remainder(targetAngle - curAngle, 360.0f);
                tr.rotation.y += diff * std::min(1.0f, 8.0f * dt);
            }
        } else if (npc.state == NPCState::Idle) {
            if (animComp && animComp->animator) {
                // В режиме паузы замедляем анимацию
                animComp->animator->setSpeed(0.15f);
            }

            if (playerNearby) {
                // Поворачиваемся к игроку из любопытства
                glm::vec3 toPlayer = playerPos - tr.position;
                toPlayer.y = 0.0f;
                if (glm::length(toPlayer) > 0.1f) {
                    glm::vec3 pDir = glm::normalize(toPlayer);
                    float pAngle = glm::degrees(std::atan2(pDir.x, pDir.z));
                    float diff = std::remainder(pAngle - tr.rotation.y, 360.0f);
                    tr.rotation.y += diff * std::min(1.0f, npc.turnSpeed * dt * 0.8f);
                }
            }

            npc.waitTimer += dt;
            if (npc.waitTimer >= npc.waitDuration) {
                npc.state = NPCState::Walking;
                if (animComp && animComp->animator) {
                    animComp->animator->setSpeed(npc.walkSpeed * 0.75f);
                    animComp->animator->resume();
                }
            }
        }
    }
}

} // namespace AI
