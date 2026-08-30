#include "engine/Physics/Physics.h"
#include <algorithm>
#include "engine/ECS/Registry.h"
#include <glm/gtc/quaternion.hpp>

// PhysX
#include "PxPhysicsAPI.h"
#include "characterkinematic/PxControllerManager.h"
#include "characterkinematic/PxController.h"
#include "cooking/PxCooking.h"

using namespace physx;

namespace cjoka_phys {

// ---------- Rigidbody Helper Methods ----------
void Rigidbody::addForce(const glm::vec3& force) {
    if (!pxActor || kind != Kind::Dynamic) return;
    auto* dyn = static_cast<PxRigidDynamic*>(pxActor);
    dyn->wakeUp();
    dyn->addForce(PxVec3(force.x, force.y, force.z), PxForceMode::eFORCE);
}

void Rigidbody::addImpulse(const glm::vec3& impulse) {
    if (!pxActor || kind != Kind::Dynamic) return;
    auto* dyn = static_cast<PxRigidDynamic*>(pxActor);
    dyn->wakeUp();
    dyn->addForce(PxVec3(impulse.x, impulse.y, impulse.z), PxForceMode::eIMPULSE);
}

void Rigidbody::addTorque(const glm::vec3& torque) {
    if (!pxActor || kind != Kind::Dynamic) return;
    auto* dyn = static_cast<PxRigidDynamic*>(pxActor);
    dyn->wakeUp();
    dyn->addTorque(PxVec3(torque.x, torque.y, torque.z), PxForceMode::eFORCE);
}

void Rigidbody::setLinearVelocity(const glm::vec3& vel) {
    if (!pxActor || kind != Kind::Dynamic) return;
    auto* dyn = static_cast<PxRigidDynamic*>(pxActor);
    dyn->wakeUp();
    dyn->setLinearVelocity(PxVec3(vel.x, vel.y, vel.z));
}

glm::vec3 Rigidbody::getLinearVelocity() const {
    if (!pxActor || kind != Kind::Dynamic) return glm::vec3(0.0f);
    auto* dyn = static_cast<const PxRigidDynamic*>(pxActor);
    PxVec3 v = dyn->getLinearVelocity();
    return glm::vec3(v.x, v.y, v.z);
}

void Rigidbody::setAngularVelocity(const glm::vec3& angVel) {
    if (!pxActor || kind != Kind::Dynamic) return;
    auto* dyn = static_cast<PxRigidDynamic*>(pxActor);
    dyn->wakeUp();
    dyn->setAngularVelocity(PxVec3(angVel.x, angVel.y, angVel.z));
}

glm::vec3 Rigidbody::getAngularVelocity() const {
    if (!pxActor || kind != Kind::Dynamic) return glm::vec3(0.0f);
    auto* dyn = static_cast<const PxRigidDynamic*>(pxActor);
    PxVec3 v = dyn->getAngularVelocity();
    return glm::vec3(v.x, v.y, v.z);
}

// ---------- Аллокатор ----------
namespace {
class DefaultAllocator final : public physx::PxAllocatorCallback {
public:
    void* allocate(size_t size, const char*, const char*, int) override {
        void* p = aligned_alloc(16, (size + 15) & ~size_t(15));
        return p;
    }
    void deallocate(void* ptr) override { free(ptr); }
};

class ErrorHandler final : public physx::PxErrorCallback {
public:
    void reportError(PxErrorCode::Enum, const char* message, const char* file, int line) override {
        fprintf(stderr, "[PhysX] %s (%s:%d)\n", message, file, line);
    }
};

static DefaultAllocator s_alloc;
static ErrorHandler     s_err;
static PxFoundation*    s_foundation = nullptr;
static PxPhysics*       s_physics = nullptr;
}

// ---------- Impl ----------
struct World::Impl {
    PxScene* scene = nullptr;
    PxMaterial* defaultMat = nullptr;
    PxControllerManager* cctMgr = nullptr;
    std::vector<PxRigidActor*> actors;
    CollisionEvents::Pair* eventSink = nullptr;
};

bool World::Init() {
    if (s_physics) return true;
    s_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, s_alloc, s_err);
    if (!s_foundation) return false;
    s_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *s_foundation, PxTolerancesScale());
    if (!s_physics) return false;
    if (!PxInitExtensions(*s_physics, nullptr)) {}
    printf("[Physics] PhysX 5.5 initialized\n");
    return true;
}
void World::Shutdown() {
    if (s_physics) { s_physics->release(); s_physics=nullptr; }
    if (s_foundation) { s_foundation->release(); s_foundation=nullptr; }
}
World& World::Get() { static World w; return w; }

World::World(const glm::vec3& gravity) : m_impl(std::make_unique<Impl>()) {
    if (!Init()) throw std::runtime_error("PhysX init failed");
    PxSceneDesc desc(s_physics->getTolerancesScale());
    desc.gravity = PxVec3(gravity.x, gravity.y, gravity.z);
    auto cpu = PxDefaultCpuDispatcherCreate(2);
    desc.cpuDispatcher = cpu;
    desc.filterShader = PxDefaultSimulationFilterShader;
    desc.flags |= PxSceneFlag::eENABLE_CCD; // Глобальный CCD
    m_impl->scene = s_physics->createScene(desc);
    m_impl->defaultMat = s_physics->createMaterial(0.6f, 0.5f, 0.2f);
    m_impl->cctMgr = PxCreateControllerManager(*m_impl->scene);
}
World::~World() {
    if (m_impl) {
        if (m_impl->cctMgr) m_impl->cctMgr->release();
        if (m_impl->scene) m_impl->scene->release();
    }
}

void World::Step(float dt) {
    if (!m_impl->scene) return;
    m_impl->scene->simulate(dt);
    m_impl->scene->fetchResults(true);
}

static void SetupDynamic(PxRigidDynamic* d, const glm::vec3& /*he*/, const Rigidbody& rb) {
    if (rb.ccd) {
        d->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
    }
    d->setLinearDamping(rb.linearDamping);
    d->setAngularDamping(rb.angularDamping);
    d->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !rb.gravity);

    // Lock Rotation Flags
    PxRigidDynamicLockFlags lockFlags;
    if (rb.lockRotationX) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_X;
    if (rb.lockRotationY) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y;
    if (rb.lockRotationZ) lockFlags |= PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
    d->setRigidDynamicLockFlags(lockFlags);

    d->setSleepThreshold(0.05f);
    d->setSolverIterationCounts(4, 1);
}

void* World::CreateBoxActor(const glm::vec3& pos, const glm::vec3& he, bool dyn, float density) {
    PxBoxGeometry geo(he.x, he.y, he.z);
    PxTransform t(PxVec3(pos.x,pos.y,pos.z));
    PxRigidActor* a;
    if (dyn) {
        auto* d = PxCreateDynamic(*s_physics, t, geo, *m_impl->defaultMat, density);
        Rigidbody defaultRb; defaultRb.density = density;
        SetupDynamic(d, he, defaultRb);
        a = d;
    } else a = PxCreateStatic(*s_physics, t, geo, *m_impl->defaultMat);
    m_impl->scene->addActor(*a);
    m_impl->actors.push_back(a);
    return a;
}
void* World::CreateSphereActor(const glm::vec3& pos, float r, bool dyn, float density) {
    PxSphereGeometry geo(r);
    PxTransform t(PxVec3(pos.x,pos.y,pos.z));
    PxRigidActor* a;
    if (dyn) {
        auto* d = PxCreateDynamic(*s_physics, t, geo, *m_impl->defaultMat, density);
        Rigidbody defaultRb; defaultRb.density = density;
        SetupDynamic(d, {r,r,r}, defaultRb);
        a = d;
    } else a = PxCreateStatic(*s_physics, t, geo, *m_impl->defaultMat);
    m_impl->scene->addActor(*a);
    m_impl->actors.push_back(a);
    return a;
}
void* World::CreateCapsuleActor(const glm::vec3& pos, float r, float h, bool dyn, float density) {
    float halfHeight = std::max(h * 0.5f - r, 0.01f);
    PxCapsuleGeometry geo(r, halfHeight);
    PxTransform t(PxVec3(pos.x,pos.y,pos.z));
    PxRigidActor* a;
    if (dyn) {
        auto* d = PxCreateDynamic(*s_physics, t, geo, *m_impl->defaultMat, density);
        Rigidbody defaultRb; defaultRb.density = density;
        SetupDynamic(d, {r, halfHeight, r}, defaultRb);
        a = d;
    } else a = PxCreateStatic(*s_physics, t, geo, *m_impl->defaultMat);
    m_impl->scene->addActor(*a);
    m_impl->actors.push_back(a);
    return a;
}

void World::SetActorPose(void* actor, const glm::vec3& pos, const glm::vec3& rot) {
    if (!actor) return;
    auto* a = static_cast<PxRigidActor*>(actor);
    PxTransform pose(PxVec3(pos.x, pos.y, pos.z));
    glm::quat q = glm::quat(glm::radians(rot));
    pose.q = PxQuat(q.x, q.y, q.z, q.w);
    a->setGlobalPose(pose);
}

void World::SetCharacterPosition(void* cct, const glm::vec3& pos) {
    if (!cct) return;
    auto* ctrl = static_cast<PxController*>(cct);
    ctrl->setPosition(PxExtendedVec3(pos.x, pos.y, pos.z));
}

void* World::CreateGroundPlane() {
    PxRigidStatic* ground = PxCreatePlane(*s_physics, PxPlane(0,1,0,0), *m_impl->defaultMat);
    m_impl->scene->addActor(*ground);
    m_impl->actors.push_back(ground);
    return ground;
}
void World::RemoveActor(void* actor) {
    auto* a = static_cast<PxRigidActor*>(actor);
    if (!a) return;
    m_impl->scene->removeActor(*a);
    a->release();
}

// Character Controller — классика: ходьба, ступеньки, склоны
void* World::CreateCharacter(const glm::vec3& pos, float radius, float height) {
    PxCapsuleControllerDesc d;
    d.position = PxExtendedVec3(pos.x, pos.y, pos.z);
    d.radius = radius;
    d.height = height;
    d.material = m_impl->defaultMat;
    d.upDirection = PxVec3(0,1,0);
    d.slopeLimit = cosf(glm::radians(45.0f));   // не скользит по крутым склонам
    d.stepOffset = 0.4f;                        // ступеньки до 40см
    auto* c = static_cast<PxCapsuleController*>(m_impl->cctMgr->createController(d));
    return c;
}
bool World::MoveCharacter(void* cct, const glm::vec3& disp, float dt, glm::vec3& outPos) {
    auto* c = static_cast<PxController*>(cct);
    if (!c) return false;
    PxControllerCollisionFlags flags = c->move(PxVec3(disp.x, disp.y, disp.z), 0.001f, dt, PxControllerFilters{});
    auto& p = c->getPosition();
    outPos = { (float)p.x, (float)p.y, (float)p.z };
    return (flags & PxControllerCollisionFlag::eCOLLISION_DOWN) == PxControllerCollisionFlag::eCOLLISION_DOWN; // onGround
}
void World::RemoveCharacter(void* cct) {
    if (cct) static_cast<PxController*>(cct)->release();
}

Entity World::Raycast(Registry&, const glm::vec3& o, const glm::vec3& dir, float maxDist, glm::vec3* op, glm::vec3* on) {
    PxRaycastBuffer hit;
    bool ok = m_impl->scene->raycast(
        PxVec3(o.x,o.y,o.z),
        PxVec3(dir.x,dir.y,dir.z).getNormalized(),
        maxDist, hit);
    if (!ok || !hit.hasBlock) return NullEntity;
    if (op) *op = { hit.block.position.x, hit.block.position.y, hit.block.position.z };
    if (on) *on = { hit.block.normal.x, hit.block.normal.y, hit.block.normal.z };
    auto* ud = hit.block.actor ? hit.block.actor->userData : nullptr;
    return ud ? (Entity)(uintptr_t)ud : NullEntity;
}

void World::AddImpulse(Entity e, const glm::vec3& impulse) {
    if (!m_reg || !m_reg->valid(e)) return;
    auto* rb = m_reg->try_get<Rigidbody>(e);
    if (!rb || !rb->pxActor || rb->kind != Rigidbody::Kind::Dynamic) return;
    rb->addImpulse(impulse);
}

void World::ThrowFrom(Entity e, const glm::vec3& vel) {
    if (!m_reg || !m_reg->valid(e)) return;
    auto* rb = m_reg->try_get<Rigidbody>(e);
    if (!rb || !rb->pxActor || rb->kind != Rigidbody::Kind::Dynamic) return;
    rb->setLinearVelocity(vel);
}

void World::PushAt(const glm::vec3& origin, const glm::vec3& dir, float maxDist, float force) {
    PxRaycastBuffer hit;
    bool ok = m_impl->scene->raycast(
        PxVec3(origin.x,origin.y,origin.z),
        PxVec3(dir.x,dir.y,dir.z).getNormalized(),
        maxDist, hit);
    if (!ok || !hit.hasBlock) return;
    auto* actor = hit.block.actor;
    if (!actor) return;
    auto* dyn = actor->is<PxRigidDynamic>();
    if (!dyn) return;
    if (dyn->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) return;
    PxVec3 imp(dir.x,dir.y,dir.z);
    imp = imp.getNormalized() * force;
    dyn->addForce(imp, PxForceMode::eIMPULSE);
}

void World::BuildActors(Registry& reg) {
    m_reg = &reg;
    // 1. Entities with Rigidbody
    for (Entity e : reg.view<Transform, Rigidbody>()) {
        auto& rb = reg.get<Rigidbody>(e);
        auto& tr = reg.get<Transform>(e);
        bool dyn = (rb.kind == Rigidbody::Kind::Dynamic);
        if (rb.pxActor) {
            SetActorPose(rb.pxActor, tr.position, tr.rotation);
            continue;
        }
        void* actor = nullptr;
        if (auto* col = reg.try_get<Collider>(e)) {
            glm::vec3 cPos = tr.position + col->centerOffset;
            if (col->type == ColliderType::Sphere)
                actor = CreateSphereActor(cPos, col->radius, dyn, rb.density);
            else if (col->type == ColliderType::Capsule)
                actor = CreateCapsuleActor(cPos, col->radius, col->height, dyn, rb.density);
            else
                actor = CreateBoxActor(cPos, col->halfExtents * tr.scale, dyn, rb.density);
        } else {
            actor = CreateBoxActor(tr.position, tr.scale * 0.5f, dyn, rb.density);
        }
        rb.pxActor = actor;
        if (actor) {
            static_cast<PxRigidActor*>(actor)->userData = (void*)(uintptr_t)e;
            if (dyn) {
                SetupDynamic(static_cast<PxRigidDynamic*>(actor), tr.scale * 0.5f, rb);
            }
        }
    }

    // 2. Entities with Collider but without Rigidbody (Static World Colliders)
    for (Entity e : reg.view<Transform, Collider>()) {
        if (reg.has<Rigidbody>(e)) continue; // Handled above
        auto& col = reg.get<Collider>(e);
        auto& tr = reg.get<Transform>(e);
        glm::vec3 cPos = tr.position + col.centerOffset;
        void* actor = nullptr;
        if (col.type == ColliderType::Sphere)
            actor = CreateSphereActor(cPos, col.radius, false, 1.0f);
        else if (col.type == ColliderType::Capsule)
            actor = CreateCapsuleActor(cPos, col.radius, col.height, false, 1.0f);
        else
            actor = CreateBoxActor(cPos, col.halfExtents * tr.scale, false, 1.0f);
        if (actor) {
            static_cast<PxRigidActor*>(actor)->userData = (void*)(uintptr_t)e;
            SetActorPose(actor, cPos, tr.rotation);
        }
    }
}

void World::SyncToECS(Registry& reg) {
    for (Entity e : reg.view<Transform, Rigidbody>()) {
        auto& rb = reg.get<Rigidbody>(e);
        if (!rb.pxActor || rb.kind != Rigidbody::Kind::Dynamic) continue;
        auto* a = static_cast<PxRigidDynamic*>(rb.pxActor);
        auto p = a->getGlobalPose();
        auto& tr = reg.get<Transform>(e);
        tr.position = { p.p.x, p.p.y, p.p.z };
        PxQuat q = p.q;
        glm::quat gq(q.w, q.x, q.y, q.z);
        glm::vec3 eu = glm::eulerAngles(gq);
        tr.rotation = { glm::degrees(eu.x), glm::degrees(eu.y), glm::degrees(eu.z) };
    }
}

void World::SetEntityUserdata(Registry&) {}

namespace Global {
    bool Init() { return World::Init(); }
    void Shutdown() { World::Shutdown(); }
}

} // namespace cjoka_phys
