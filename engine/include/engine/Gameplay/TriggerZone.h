#pragma once
// TriggerZone — Компонент зоны триггера (Box / Sphere) для ивентов и логики
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/Physics/Physics.h"
#include "engine/Gameplay/EventBus.h"
#include "engine/Scripting/ScriptableEntity.h"
#include <glm/glm.hpp>
#include <string>
#include <unordered_set>

enum class TriggerShape {
    Box = 0,
    Sphere = 1
};

struct TriggerZone {
    TriggerShape shape = TriggerShape::Box;
    glm::vec3 halfExtents{1.0f, 1.0f, 1.0f};
    float radius = 1.0f;
    glm::vec3 offset{0.0f};

    std::string enterEvent = "OnTriggerEnter";
    std::string exitEvent = "OnTriggerExit";
    bool oneShot = false;
    bool triggered = false;

    std::unordered_set<Entity> activeVisitors;

    bool contains(const glm::vec3& worldPos, const Transform& zoneTransform) const {
        glm::vec3 center = zoneTransform.position + offset;
        if (shape == TriggerShape::Sphere) {
            return glm::length(worldPos - center) <= radius;
        } else {
            glm::vec3 d = glm::abs(worldPos - center);
            return (d.x <= halfExtents.x && d.y <= halfExtents.y && d.z <= halfExtents.z);
        }
    }
};

namespace Systems {

inline void UpdateTriggerZones(Registry& reg) {
    for (Entity zoneEnt : reg.view<Transform, TriggerZone>()) {
        auto& zoneTr = reg.get<Transform>(zoneEnt);
        auto& zone = reg.get<TriggerZone>(zoneEnt);
        if (zone.oneShot && zone.triggered) continue;

        std::unordered_set<Entity> currentVisitors;

        for (Entity visitor : reg.view<Transform>()) {
            if (visitor == zoneEnt) continue;
            // Check relevant visitors
            bool isRelevant = reg.has<CharacterController>(visitor) || 
                              reg.has<Camera>(visitor) || 
                              reg.has<cjoka_phys::Rigidbody>(visitor);
            if (!isRelevant) continue;

            const auto& visTr = reg.get<Transform>(visitor);
            if (zone.contains(visTr.position, zoneTr)) {
                currentVisitors.insert(visitor);

                // If visitor just entered
                if (zone.activeVisitors.find(visitor) == zone.activeVisitors.end()) {
                    zone.triggered = true;
                    EventBus::Get().emit(zone.enterEvent, zoneEnt, visitor);
                    if (reg.has<NativeScript>(zoneEnt)) {
                        auto& ns = reg.get<NativeScript>(zoneEnt);
                        if (ns.instance) ns.instance->onTriggerEnter(visitor);
                    }
                    if (reg.has<NativeScript>(visitor)) {
                        auto& ns = reg.get<NativeScript>(visitor);
                        if (ns.instance) ns.instance->onTriggerEnter(zoneEnt);
                    }
                }
            }
        }

        // Check for visitors that exited
        for (Entity oldVisitor : zone.activeVisitors) {
            if (currentVisitors.find(oldVisitor) == currentVisitors.end()) {
                EventBus::Get().emit(zone.exitEvent, zoneEnt, oldVisitor);
                if (reg.has<NativeScript>(zoneEnt)) {
                    auto& ns = reg.get<NativeScript>(zoneEnt);
                    if (ns.instance) ns.instance->onTriggerExit(oldVisitor);
                }
                if (reg.has<NativeScript>(oldVisitor)) {
                    auto& ns = reg.get<NativeScript>(oldVisitor);
                    if (ns.instance) ns.instance->onTriggerExit(zoneEnt);
                }
            }
        }

        zone.activeVisitors = std::move(currentVisitors);
    }
}

} // namespace Systems
