#pragma once
// Physics — обёртка NVIDIA PhysX 5.5 над ECS cjoka.
// Классика: гравитация, rigid body (динамика/статика), коллайдеры (box/sphere/capsule/plane/mesh),
// Character Controller (ходьба/прыжок без ракет), рейкасты.
// БЕЗ: машин, ткани, частиц, разрушений.
//
//   Physics::Init();                       // один раз
//   auto e = scene.createCube(...);
//   e.emplace<Rigidbody>{Rigidbody::Dynamic()};  // упадёт
//   Physics::Step(dt); Systems::PhysicsSync(registry);  // в onUpdate
#include "engine/ECS/Components.h"
#include "engine/ECS/Registry.h"
#include <memory>
#include <vector>

namespace physx {
class PxFoundation;
class PxPhysics;
class PxScene;
class PxMaterial;
class PxRigidActor;
class PxRigidDynamic;
class PxShape;
}

namespace cjoka_phys {

// ---------- Компоненты ECS ----------
enum class ColliderType { Box, Sphere, Capsule, Plane };

struct Collider {
    ColliderType type = ColliderType::Box;
    glm::vec3 halfExtents = {0.5f,0.5f,0.5f}; // box
    float radius = 0.5f;                      // sphere/capsule
    float height = 1.0f;                      // capsule (полная)
    float staticFriction = 0.6f;
    float dynamicFriction = 0.5f;
    float restitution = 0.2f;                 // прыгучесть
};

struct Rigidbody {
    enum class Kind { Static, Dynamic, Kinematic };
    Kind kind = Kind::Dynamic;
    float density = 1.0f;      // кг/м³ (масса считается)
    float linearDamping = 0.05f;
    float angularDamping = 0.05f;
    bool gravity = true;
    // runtime
    void* pxActor = nullptr;   // PxRigidActor*
};

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

    void Step(float dt);         // фикс шаг внутри
private:
    Registry* m_reg = nullptr;   // для ThrowFrom/PushAt
public:
    void SyncToECS(Registry& reg);          // PhysX -> Transform
    void BuildFromECS(Registry& reg) { m_reg=&reg; BuildActors(reg); }       // создать акторов для новых MeshRenderer+Rigidbody
    void BuildActors(Registry& reg);

    // Создание вручную (без ECS) — для особых случаев
    void* CreateBoxActor(const glm::vec3& pos, const glm::vec3& halfExtents, bool dynamic, float density);
    void* CreateSphereActor(const glm::vec3& pos, float radius, bool dynamic, float density);
    void* CreateGroundPlane();
    void RemoveActor(void* actor);

    // Character Controller
    void* CreateCharacter(const glm::vec3& pos, float radius, float height);
    bool  MoveCharacter(void* cct, const glm::vec3& disp, float dt, glm::vec3& outPos);
    void  RemoveCharacter(void* cct);

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
