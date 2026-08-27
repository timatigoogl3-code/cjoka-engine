#pragma once
// Physics — обёртка NVIDIA PhysX 5.5 над ECS cjoka.
// Предоставляет глубокую интеграцию с ECS: компоненты Rigidbody, Collider, CharacterController,
// силовые импульсы, рейкасты и непрерывное обнаружение столкновений (CCD).
#include "engine/ECS/Components.h"
#include "engine/ECS/Registry.h"
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace cjoka_phys {

// ---------- Компоненты ECS ----------
enum class ColliderType { Box, Sphere, Capsule, Plane };

struct Collider {
    ColliderType type = ColliderType::Box;
    glm::vec3 halfExtents = {0.5f,0.5f,0.5f}; // box
    float radius = 0.5f;                      // sphere/capsule
    float height = 1.0f;                      // capsule (полная)
    glm::vec3 centerOffset = {0.0f, 0.0f, 0.0f};
    float staticFriction = 0.6f;
    float dynamicFriction = 0.5f;
    float restitution = 0.2f;                 // прыгучесть
    bool isTrigger = false;

    static Collider Box(const glm::vec3& half = {0.5f,0.5f,0.5f}) {
        Collider c; c.type = ColliderType::Box; c.halfExtents = half; return c;
    }
    static Collider Sphere(float r = 0.5f) {
        Collider c; c.type = ColliderType::Sphere; c.radius = r; return c;
    }
    static Collider Capsule(float r = 0.35f, float h = 1.2f) {
        Collider c; c.type = ColliderType::Capsule; c.radius = r; c.height = h; return c;
    }
};

struct Rigidbody {
    enum class Kind { Static, Dynamic, Kinematic };
    Kind kind = Kind::Dynamic;
    float density = 1.0f;      // кг/м³ (масса считается автоматически)
    float linearDamping = 0.05f;
    float angularDamping = 0.05f;
    bool gravity = true;
    bool ccd = true;           // Continuous Collision Detection (без проваливания сквозь пол)
    bool lockRotationX = false;
    bool lockRotationY = false;
    bool lockRotationZ = false;

    // runtime handle
    void* pxActor = nullptr;   // PxRigidActor*

    static Rigidbody Dynamic(float dens = 1.0f) {
        Rigidbody r; r.kind = Kind::Dynamic; r.density = dens; return r;
    }
    static Rigidbody Static() {
        Rigidbody r; r.kind = Kind::Static; return r;
    }
    static Rigidbody Kinematic() {
        Rigidbody r; r.kind = Kind::Kinematic; return r;
    }

    void addForce(const glm::vec3& force);
    void addImpulse(const glm::vec3& impulse);
    void addTorque(const glm::vec3& torque);
    void setLinearVelocity(const glm::vec3& vel);
    glm::vec3 getLinearVelocity() const;
    void setAngularVelocity(const glm::vec3& angVel);
    glm::vec3 getAngularVelocity() const;
};

// Aliases
using RigidBodyComponent = Rigidbody;
using ColliderComponent = Collider;

// События столкновений (заполняются после Step)
struct CollisionEvents {
    struct Pair { Entity a; Entity b; };
    std::vector<Pair> pairs;
    bool has(Entity e) const {
        for (auto& p : pairs) if (p.a==e || p.b==e) return true;
        return false;
    }
};

// ---------- Мир ----------
class World {
public:
    static bool Init();          // создать foundation+physics+cooking
    static void Shutdown();
    static World& Get();

    // сцена: gravity обычно {0,-9.81,0}
    explicit World(const glm::vec3& gravity = {0,-9.81f,0});
    ~World();

    void Step(float dt);         // фикс шаг PhysX
private:
    Registry* m_reg = nullptr;
public:
    void SyncToECS(Registry& reg);          // PhysX -> Transform
    void BuildFromECS(Registry& reg) { m_reg=&reg; BuildActors(reg); }
    void BuildActors(Registry& reg);

    // Создание вручную (без ECS) — для особых случаев
    void* CreateBoxActor(const glm::vec3& pos, const glm::vec3& halfExtents, bool dynamic, float density);
    void* CreateSphereActor(const glm::vec3& pos, float radius, bool dynamic, float density);
    void* CreateCapsuleActor(const glm::vec3& pos, float radius, float height, bool dynamic, float density);
    void* CreateGroundPlane();
    void RemoveActor(void* actor);
    void SetActorPose(void* actor, const glm::vec3& pos, const glm::vec3& rot = glm::vec3(0.0f));

    // Character Controller
    void* CreateCharacter(const glm::vec3& pos, float radius, float height);
    bool  MoveCharacter(void* cct, const glm::vec3& disp, float dt, glm::vec3& outPos);
    void  RemoveCharacter(void* cct);
    void  SetCharacterPosition(void* cct, const glm::vec3& pos);

    // Рейкаст: вернёт entity hit или NullEntity; outPoint/outNormal
    Entity Raycast(Registry& reg, const glm::vec3& origin, const glm::vec3& dir, float maxDist,
                   glm::vec3* outPoint=nullptr, glm::vec3* outNormal=nullptr);

    void AddImpulse(Entity e, const glm::vec3& impulse);
    void SetEntityUserdata(Registry& reg);  // связать actor<->entity

    // Толкнуть/кинуть: задаёт скорость динамическому телу по entity
    void ThrowFrom(Entity e, const glm::vec3& vel);
    // Импульс от точки (толчок рукой): сила в точке контакта перед камерой
    void PushAt(const glm::vec3& origin, const glm::vec3& dir, float maxDist, float force);

    CollisionEvents events;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ---------- Глобальный API ----------
namespace Global {
    bool Init();
    void Shutdown();
}

} // namespace cjoka_phys
