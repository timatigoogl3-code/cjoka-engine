#pragma once
#include "engine/Engine.h"
#include "engine/Physics/Vehicle.h"
#include "engine/Physics/ClothCape.h"
#include "engine/Renderer/ParticleSystem.h"
#include "engine/Renderer/WaterSurface.h"
#include <vector>
#include <string>

// ============================================================================
// Gameplay Prefabs & Entity Archetypes (Game-level logic & assets)
// Движок cjoka остаётся чистым и универсальным, а все игровые префабы,
// составные объекты и специфичные настройки живут на уровне игры!
// ============================================================================
namespace Prefabs {

// 1. ИИ-прохожий с анимированной скелетной моделью и логикой патрулирования
inline EntityRef Pedestrian(Scene& scene, const glm::vec3& startPos,
                            const std::vector<glm::vec3>& waypoints,
                            float speed = 1.35f, float animOffset = 0.0f,
                            const std::string& name = "Pedestrian") {
    auto diffTex = Assets::Texture("assets/models/nathan/tex/rp_nathan_animated_003_dif_2k.jpg", true);
    auto glossTex = Assets::Texture("assets/models/nathan/tex/rp_nathan_animated_003_gloss_2k.jpg");
    Material mat = Material::Textured(diffTex, glm::vec3(1.0f), 0.5f, 0.05f);
    if (glossTex && glossTex->valid()) {
        mat.specularMap = glossTex;
        mat.useSpecularMap = true;
    }

    auto skinned = Assets::Skinned("assets/models/nathan/rp_nathan_animated_003_walking.fbx");
    EntityRef ref = scene.create(name, Transform{startPos, {0.0f, 0.0f, 0.0f}, glm::vec3(0.01f)});
    ref.add<SkinnedMeshRenderer>(SkinnedMeshRenderer(skinned, mat));

    if (skinned) {
        auto anim = std::make_shared<Animation::Animator>(skinned);
        anim->setTime(animOffset > 0.0f ? animOffset : 0.25f);
        anim->setSpeed(speed * 0.75f);
        ref.add<AnimatorComponent>(AnimatorComponent(anim));
    }

    ref.add<AI::NPCComponent>(AI::NPCComponent(waypoints, speed));
    return ref;
}

// 2. Уличный фонарь (столб + плафон + точечный свет)
inline EntityRef StreetLamp(Scene& scene, const glm::vec3& pos,
                           bool withPointLight = false,
                           const glm::vec3& lightColor = {1.0f, 0.88f, 0.70f},
                           float intensity = 2.5f) {
    Material poleMat = Material::Metal({0.2f, 0.22f, 0.25f}, 0.35f);
    Material bulbMat = Material::Emissive(lightColor, 5.0f);

    // Столб
    scene.create("LampPole", Transform{pos + glm::vec3{0.0f, 1.8f, 0.0f}, {}, {0.12f, 3.6f, 0.12f}})
         .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), poleMat).setClusterLOD(false));

    // Перекладина
    scene.create("LampArm", Transform{pos + glm::vec3{0.35f, 3.55f, 0.0f}, {}, {0.8f, 0.10f, 0.10f}})
         .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), poleMat).setClusterLOD(false));

    // Плафон
    scene.create("LampBulb", Transform{pos + glm::vec3{0.7f, 3.4f, 0.0f}, {}, {0.3f, 0.25f, 0.3f}})
         .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), bulbMat).setClusterLOD(false));

    if (withPointLight) {
        return scene.createPointLight(Transform{pos + glm::vec3{0.7f, 3.2f, 0.0f}},
                                     PointLight::Warm(lightColor, 14.0f, intensity), "LampLight");
    }
    return scene.create("LampBulbHolder", Transform{pos});
}

// 3. Реалистичная 3D скамейка (фигурный чугун + деревянные рейки)
inline EntityRef Bench(Scene& scene, const glm::vec3& pos, float yaw = 0.0f) {
    Material woodMat = Material::Dielectric({0.55f, 0.28f, 0.12f}, 0.50f);
    auto benchMesh = Assets::Mesh("assets/models/bench.obj");

    EntityRef ref = scene.create("ParkBench", Transform{pos, {0.0f, yaw, 0.0f}, glm::vec3(1.0f)});
    ref.add<MeshRenderer>(MeshRenderer(benchMesh, woodMat).setClusterLOD(false));
    return ref;
}

// 4. Городское здание / дом
inline EntityRef Building(Scene& scene, const glm::vec3& pos, const glm::vec3& size, const glm::vec3& wallColor, bool lighted = true) {
    Material wallMat = Material::Dielectric(wallColor, 0.85f);
    Material winMat = lighted ? Material::Emissive({1.0f, 0.92f, 0.65f}, 2.5f) : Material::Dielectric({0.15f, 0.20f, 0.25f}, 0.1f);
    Material roofMat = Material::Dielectric({0.18f, 0.18f, 0.20f}, 0.90f);

    // Основной корпус здания
    EntityRef mainB = scene.create("Building", Transform{pos + glm::vec3{0.0f, size.y * 0.5f, 0.0f}, {}, size});
    mainB.add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), wallMat).setClusterLOD(false));

    // Парапет на крыше
    scene.create("Roof", Transform{pos + glm::vec3{0.0f, size.y + 0.3f, 0.0f}, {}, {size.x + 0.4f, 0.6f, size.z + 0.4f}})
         .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), roofMat).setClusterLOD(false));

    // Окна (цельные полосы)
    int floors = static_cast<int>(std::floor(size.y / 3.5f));
    for (int f = 1; f <= floors; ++f) {
        float y = static_cast<float>(f) * 3.5f - 1.0f;
        scene.create("WindowsFront", Transform{pos + glm::vec3{0.0f, y, size.z * 0.5f + 0.05f}, {}, {size.x * 0.75f, 1.2f, 0.05f}})
             .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), winMat).setClusterLOD(false));
    }
    return mainB;
}

// 5. Легкая живая изгородь для аллей
inline EntityRef HedgeBox(Scene& scene, const glm::vec3& pos, const glm::vec3& size = {0.8f, 0.9f, 4.0f}) {
    auto tex = Assets::Texture("assets/textures/prototype_floor.png");
    Material mat = Material::Textured(tex, glm::vec3(0.18f, 0.42f, 0.15f), 0.90f, 0.0f);
    EntityRef ref = scene.create("HedgeBox", Transform{pos + glm::vec3{0.0f, size.y * 0.5f, 0.0f}, {}, size});
    ref.add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), mat).setClusterLOD(false));
    return ref;
}

// 5b. Куст живой изгороди с авто-кластеризацией ClusterLOD (для демонстрационной зоны)
inline EntityRef HedgeBush(Scene& scene, const glm::vec3& pos, float yaw = 0.0f, float scale = 0.20f) {
    auto cm = Assets::Clustered("assets/models/indoor_plant.obj");
    Material mat = Material::Default().withRoughness(0.65f);
    auto tex = Assets::Texture("assets/textures/indoor_plant_COL.jpg", true);
    if (tex && tex->valid()) {
        mat.diffuseMap = tex;
        mat.useDiffuseMap = true;
    }

    EntityRef ref = scene.create("HedgeBush", Transform{pos, {0.0f, yaw, 0.0f}, glm::vec3(scale)});
    ref.add<MeshRenderer>(MeshRenderer(cm, mat));
    return ref;
}

// 5. Физический динамический ящик для бросков и штабелей
inline EntityRef PhysicsCrate(Scene& scene, const glm::vec3& pos, const glm::vec3& size = {0.6f, 0.6f, 0.6f}, float density = 1.0f) {
    auto tex = Assets::Texture("assets/textures/GridBox_Default.png", true);
    Material mat = Material::Textured(tex, glm::vec3(1.0f), 0.5f);

    EntityRef ref = scene.create("PhysicsCrate", Transform{pos, {}, size});
    ref.add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), mat).setClusterLOD(false));
    ref.add<cjoka_phys::Rigidbody>(cjoka_phys::Rigidbody::Dynamic(density));
    return ref;
}

// 5b. Реалистичная металлическая бочка с опасными отходами
inline EntityRef PhysicsBarrel(Scene& scene, const glm::vec3& pos, float density = 1.2f) {
    auto tex = Assets::Texture("assets/textures/barrel.png", true);
    Material mat = Material::Textured(tex, glm::vec3(1.0f), 0.4f, 0.4f);
    auto barrelMesh = Assets::Mesh("assets/models/barrel.obj");

    EntityRef ref = scene.create("PhysicsBarrel", Transform{pos, {}, glm::vec3(1.0f)});
    ref.add<MeshRenderer>(MeshRenderer(barrelMesh, mat).setClusterLOD(false));
    ref.add<cjoka_phys::Rigidbody>(cjoka_phys::Rigidbody::Dynamic(density));
    return ref;
}

// 6. Чаша с живым огнем (Fire Pit + Torch)
inline EntityRef FirePit(Scene& scene, const glm::vec3& pos) {
    Material bowlMat = Material::Metal({0.25f, 0.25f, 0.28f}, 0.5f);
    Material emberMat = Material::Emissive({1.0f, 0.45f, 0.05f}, 1.2f);

    // Каменная чаша
    scene.create("FireBowl", Transform{pos + glm::vec3{0.0f, 0.35f, 0.0f}, {}, {0.8f, 0.35f, 0.8f}})
         .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), bowlMat).setClusterLOD(false));

    // Угли в чаше
    scene.create("FireEmbers", Transform{pos + glm::vec3{0.0f, 0.45f, 0.0f}, {}, {0.6f, 0.08f, 0.6f}})
         .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), emberMat).setClusterLOD(false));

    // Мягкий динамический свет огня
    return scene.createPointLight(Transform{pos + glm::vec3{0.0f, 0.75f, 0.0f}},
                                 PointLight::Warm({1.0f, 0.65f, 0.2f}, 5.0f, 1.2f), "FireLight");
}

// 7. Мраморный фонтан с водой
inline EntityRef FountainBasin(Scene& scene, const glm::vec3& pos, float radius = 2.4f) {
    Material marbleMat = Material::Dielectric({0.92f, 0.90f, 0.88f}, 0.25f);
    Material baseMat = Material::Dielectric({0.75f, 0.73f, 0.70f}, 0.40f);

    // Основание фонтана
    scene.create("FountainBase", Transform{pos + glm::vec3{0.0f, 0.2f, 0.0f}, {}, {radius * 2.1f, 0.4f, radius * 2.1f}})
         .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), baseMat).setClusterLOD(false));

    // Центральная колонна гейзера
    scene.create("FountainColumn", Transform{pos + glm::vec3{0.0f, 0.65f, 0.0f}, {}, {0.5f, 0.9f, 0.5f}})
         .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), marbleMat).setClusterLOD(false));

    return scene.create("FountainCenter", Transform{pos});
}

// 8. Проекционные декали (следы шин, разметка, трещины на асфальте, граффити)
inline EntityRef RoadDecal(Scene& scene, const glm::vec3& pos, const glm::vec3& size = {1.5f, 0.4f, 2.5f}, float yaw = 0.0f, const glm::vec4& tint = {1.0f, 1.0f, 1.0f, 0.85f}) {
    auto tex = Assets::Texture("assets/textures/prototype_floor.png", true);
    EntityRef ref = scene.create("RoadDecal", Transform{pos, {0.0f, yaw, 0.0f}, glm::vec3(1.0f)});
    ref.add<Decal>(Decal::Create(tex, size, tint));
    return ref;
}

inline EntityRef SkidMark(Scene& scene, const glm::vec3& pos, float length = 3.0f, float width = 0.6f, float yaw = 0.0f) {
    auto tex = Assets::Texture("assets/textures/prototype_floor.png", true);
    EntityRef ref = scene.create("TireSkidMark", Transform{pos, {0.0f, yaw, 0.0f}, glm::vec3(1.0f)});
    ref.add<Decal>(Decal::Create(tex, {width, 0.3f, length}, {0.1f, 0.1f, 0.12f, 0.90f}));
    return ref;
}

} // namespace Prefabs
