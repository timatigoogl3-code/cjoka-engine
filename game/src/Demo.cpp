#include "Demo.h"
#include "Prefabs.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

// ============================================================================
// cjoka Engine -- Grand Highway Tech Showcase
//
// Camera yaw convention (matches Transform::forward / Camera::viewFromTransform):
//   forward = (cos(yaw), sin(pitch), sin(yaw))
//   yaw =  90 deg -> looking along +Z (highway forward)
//   yaw =   0 deg -> looking along +X (right)
//   yaw = 180 deg -> looking along -X (left)
//
// Vehicle yaw convention (Physics::Vehicle):
//   forward = (sin(yaw), 0, cos(yaw))
//   yaw =   0 deg -> moving along +Z
//   yaw =  90 deg -> moving along +X
//
// Conversion: cameraYaw = 90.0 - vehicleYaw
// ============================================================================

Demo::Demo() : Application(1280, 720, "cjoka Engine -- Grand Highway Showcase") {}
Demo::~Demo() = default;

void Demo::onInit() {
    std::cout << "[Demo] Initializing Grand Highway Showcase\n";

    cjoka_phys::Global::Init();
    m_phys = std::make_unique<cjoka_phys::World>(glm::vec3{0.0f, -9.81f, 0.0f});
    m_phys->CreateGroundPlane();

    m_litShader = std::make_unique<Shader>(DefaultShaders::kLitVS, DefaultShaders::kLitFS);
    m_cape = std::make_unique<Physics::ClothCape>(9, 14, 0.55f, 0.95f);
    m_particles = std::make_unique<ParticleFX::ParticleSystem>(2000);

    buildWorld();
    spawnPlayer();

    m_cursorLocked = true;
    Input::SetCursorLocked(true);
}

// ============================================================================
// World construction
// ============================================================================
void Demo::buildWorld() {
    // -- Atmosphere --
    Sky sky;
    sky.top     = {0.18f, 0.40f, 0.82f};
    sky.horizon = {0.62f, 0.75f, 0.92f};
    sky.bottom  = {0.82f, 0.85f, 0.90f};
    sky.exposure = 1.10f;
    scene().createBeautifulAtmosphere(sky, Fog{{0.65f, 0.75f, 0.88f}, 0.0012f});
    scene().createPost(PostProcessSettings::Cinematic());
    scene().create("Ambient").add<AmbientLight>(AmbientLight{{0.40f, 0.44f, 0.50f}, 1.20f});
    scene().createSun({-0.50f, -0.80f, -0.30f}, {1.0f, 0.96f, 0.90f}, 2.5f);

    auto texFloor = Assets::Texture("assets/textures/prototype_floor.png");
    Material asphaltMat = Material::Textured(texFloor, {0.25f, 0.25f, 0.28f}, 0.82f, 0.04f);
    Material curbMat     = Material::Dielectric({0.58f, 0.58f, 0.60f}, 0.70f);
    Material sidewalkMat = Material::Textured(texFloor, {0.66f, 0.66f, 0.68f}, 0.65f, 0.02f);
    Material grassMat    = Material::Textured(texFloor, {0.20f, 0.42f, 0.18f}, 0.90f, 0.0f);

    // -- Highway surface (500m, z from -250 to +250, width 16m) --
    scene().create("Road", Transform{{0.0f, -0.05f, 0.0f}, {}, {16.0f, 0.1f, 500.0f}})
           .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), asphaltMat).setClusterLOD(false));

    // -- Curbs --
    scene().create("CurbW", Transform{{-8.15f, 0.08f, 0.0f}, {}, {0.3f, 0.26f, 500.0f}})
           .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), curbMat).setClusterLOD(false));
    scene().create("CurbE", Transform{{8.15f, 0.08f, 0.0f}, {}, {0.3f, 0.26f, 500.0f}})
           .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), curbMat).setClusterLOD(false));

    // -- Sidewalks --
    scene().create("SidewalkW", Transform{{-10.65f, 0.06f, 0.0f}, {}, {4.7f, 0.22f, 500.0f}})
           .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), sidewalkMat).setClusterLOD(false));
    scene().create("SidewalkE", Transform{{10.65f, 0.06f, 0.0f}, {}, {4.7f, 0.22f, 500.0f}})
           .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), sidewalkMat).setClusterLOD(false));

    // -- Grass lawns --
    scene().create("GrassW", Transform{{-22.0f, -0.04f, 0.0f}, {}, {18.0f, 0.1f, 500.0f}})
           .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), grassMat).setClusterLOD(false));
    scene().create("GrassE", Transform{{22.0f, -0.04f, 0.0f}, {}, {18.0f, 0.1f, 500.0f}})
           .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), grassMat).setClusterLOD(false));

    // -- Center line markings (every 10m) --
    for (int z = -240; z <= 240; z += 10) {
        Prefabs::RoadDecal(scene(), {0.0f, 0.02f, float(z)},
                           {0.25f, 0.15f, 3.5f}, 0.0f, {0.95f, 0.95f, 0.95f, 0.85f});
    }

    // -- Street lamps (emissive only, NO point lights for performance) --
    //    Only a few near the spawn get real point lights for dramatic effect.
    for (int z = -220; z <= 220; z += 35) {
        float fz = float(z);
        Prefabs::StreetLamp(scene(), {-12.0f, 0.2f, fz}, true);
        Prefabs::StreetLamp(scene(), { 12.0f, 0.2f, fz + 17.0f}, true);
    }

    // -- Benches along sidewalks (sparse) --
    for (int z = -200; z <= 200; z += 50) {
        Prefabs::Bench(scene(), {-12.5f, 0.58f, float(z)}, 90.0f);
        Prefabs::Bench(scene(), { 12.5f, 0.58f, float(z) + 25.0f}, -90.0f);
    }

    // -- Buildings along the highway --
    glm::vec3 bCol[4] = {
        {0.82f, 0.80f, 0.76f}, {0.68f, 0.65f, 0.60f},
        {0.50f, 0.54f, 0.58f}, {0.85f, 0.82f, 0.76f}
    };
    for (int i = -7; i <= 7; ++i) {
        float z = float(i) * 32.0f;
        float h1 = 10.0f + float((std::abs(i * 7) % 14));
        float h2 = 12.0f + float((std::abs(i * 11) % 16));
        Prefabs::Building(scene(), {-22.0f, 0.2f, z}, {10.0f, h1, 16.0f}, bCol[std::abs(i) % 4]);
        Prefabs::Building(scene(), { 22.0f, 0.2f, z + 16.0f}, {10.0f, h2, 16.0f}, bCol[(std::abs(i) + 2) % 4]);
    }

    // =====================================================================
    // EXHIBIT ZONES
    // =====================================================================
    
    // -- Forward+ Lighting Test: Floating Colorful Orbs --
    for (int i = 0; i < 40; ++i) {
        float fz = -200.0f + float(i) * 10.0f;
        glm::vec3 col = glm::vec3(
            0.5f + 0.5f * sin(fz * 0.1f),
            0.5f + 0.5f * sin(fz * 0.13f + 2.0f),
            0.5f + 0.5f * sin(fz * 0.17f + 4.0f)
        );
        scene().createPointLight(Transform{{-6.0f + float(i%2)*12.0f, 1.2f, fz}}, PointLight{col, 5.0f, 12.0f});
    }

    // Zone 1: Decals & Drift Chicane (z = -140 to -100)
    Prefabs::RoadDecal(scene(), {0.0f, 0.02f, -140.0f}, {7.5f, 0.2f, 0.8f}, 0.0f, {0.95f, 0.95f, 0.95f, 0.9f});
    Prefabs::RoadDecal(scene(), {0.0f, 0.02f, -138.0f}, {7.5f, 0.2f, 0.8f}, 0.0f, {0.95f, 0.95f, 0.95f, 0.9f});
    Prefabs::SkidMark(scene(), {-3.8f, 0.02f, -125.0f}, 12.0f, 0.5f, -12.0f);
    Prefabs::SkidMark(scene(), {-2.9f, 0.02f, -125.0f}, 12.0f, 0.5f, -12.0f);
    Prefabs::SkidMark(scene(), { 3.2f, 0.02f, -105.0f}, 14.0f, 0.55f, 15.0f);
    Prefabs::SkidMark(scene(), { 4.1f, 0.02f, -105.0f}, 14.0f, 0.55f, 15.0f);

    // Zone 2: PhysX Destruction Arena (z = -50 to -20)
    for (int row = 0; row < 4; ++row) {
        int count = 5 - row;
        for (int bx = 0; bx < count; ++bx) {
            float px = -2.5f + float(bx) * 0.80f + float(row) * 0.40f;
            float py = 0.5f + float(row) * 0.95f;
            if ((bx + row) % 2 == 0) {
                auto b = Prefabs::PhysicsBarrel(scene(), {px, py, -35.0f}, 1.2f);
                m_dynamicObjects.push_back(b.id());
            } else {
                auto c = Prefabs::PhysicsCrate(scene(), {px, py, -35.0f}, {0.6f, 0.6f, 0.6f}, 1.0f);
                m_dynamicObjects.push_back(c.id());
            }
        }
    }

    // Zone 3: Volumetric Fire & Particles (z = 20 to 60)
    m_firePositions = {
        {-7.0f, 0.2f, 30.0f}, {7.0f, 0.2f, 30.0f},
        {-7.0f, 0.2f, 50.0f}, {7.0f, 0.2f, 50.0f}
    };
    for (const auto& fp : m_firePositions) {
        Prefabs::FirePit(scene(), fp);
    }

    // Zone 4: ClusterLOD Dense Foliage (z = 100 to 140)
    //   Only 4 high-poly bushes (137K tris each) to demonstrate LOD without tanking FPS
    Prefabs::HedgeBush(scene(), {-8.0f, 0.0f, 105.0f}, 0.0f, 0.20f);
    Prefabs::HedgeBush(scene(), { 8.0f, 0.0f, 115.0f}, 90.0f, 0.22f);
    Prefabs::HedgeBush(scene(), {-8.0f, 0.0f, 125.0f}, 45.0f, 0.18f);
    Prefabs::HedgeBush(scene(), { 8.0f, 0.0f, 135.0f}, 135.0f, 0.21f);

    // Zone 5: Skeletal Animation & AI (z = 180 to 230)
    Prefabs::Pedestrian(scene(), {-10.5f, 0.2f, 185.0f},
                        {{-10.5f, 0.2f, 185.0f}, {-10.5f, 0.2f, 230.0f}}, 1.35f, 0.0f, "Pedestrian_1");
    Prefabs::Pedestrian(scene(), {10.5f, 0.2f, 225.0f},
                        {{10.5f, 0.2f, 225.0f}, {10.5f, 0.2f, 180.0f}}, 1.45f, 0.7f, "Pedestrian_2");
}

// ============================================================================
// Player & vehicle spawn
// ============================================================================
void Demo::spawnPlayer() {
    m_playerPos = {-3.5f, 1.10f, -202.0f};
    m_cct = m_phys->CreateCharacter(m_playerPos, 0.35f, 1.2f);

    // Player character model
    auto diffTex  = Assets::Texture("assets/models/nathan/tex/rp_nathan_animated_003_dif_2k.jpg", true);
    auto glossTex = Assets::Texture("assets/models/nathan/tex/rp_nathan_animated_003_gloss_2k.jpg");
    Material playerMat = Material::Textured(diffTex, glm::vec3(1.0f), 0.5f, 0.05f);
    if (glossTex && glossTex->valid()) {
        playerMat.specularMap = glossTex;
        playerMat.useSpecularMap = true;
    }

    auto skinned = Assets::Skinned("assets/models/nathan/rp_nathan_animated_003_walking.fbx");
    EntityRef pModel = scene().create("Player",
        Transform{m_playerPos - glm::vec3{0.0f, 0.95f, 0.0f}, {}, glm::vec3(0.01f)});
    pModel.add<SkinnedMeshRenderer>(SkinnedMeshRenderer(skinned, playerMat));

    if (skinned) {
        auto anim = std::make_shared<Animation::Animator>(skinned);
        anim->setTime(0.25f);
        anim->setSpeed(1.0f);
        pModel.add<AnimatorComponent>(AnimatorComponent(anim));
    }
    m_playerModel = pModel.id();

    // Vehicles: sports car near player, police and taxi as AI traffic
    auto car1 = std::make_unique<Physics::Vehicle>(
        scene(), glm::vec3{-3.5f, 0.0f, -198.0f}, 0.0f,
        "assets/models/cars/sedan-sports.obj", 20.0f,
        "assets/models/cars/wheel.obj", "assets/textures/colormap.png");
    car1->setBounds(true, {-7.5f, 7.5f}, {-245.0f, 245.0f});
    m_vehicles.push_back(std::move(car1));

    auto car2 = std::make_unique<Physics::Vehicle>(
        scene(), glm::vec3{3.5f, 0.0f, -10.0f}, 180.0f,
        "assets/models/cars/police.obj", 7.5f,
        "assets/models/cars/wheel.obj", "assets/textures/colormap.png");
    car2->setBounds(true, {-7.5f, 7.5f}, {-245.0f, 245.0f});
    m_vehicles.push_back(std::move(car2));

    auto car3 = std::make_unique<Physics::Vehicle>(
        scene(), glm::vec3{-3.5f, 0.0f, 120.0f}, 0.0f,
        "assets/models/cars/taxi.obj", 7.0f,
        "assets/models/cars/wheel.obj", "assets/textures/colormap.png");
    car3->setBounds(true, {-7.5f, 7.5f}, {-245.0f, 245.0f});
    m_vehicles.push_back(std::move(car3));

    // Camera: yaw=90 -> looking along +Z (forward on highway)
    m_camYaw = 90.0f;
    m_camPitch = -8.0f;
    m_targetYaw = 90.0f;
    m_targetPitch = -8.0f;
    m_smoothCamPos = m_playerPos + glm::vec3{0.0f, 0.8f, -3.5f};
    m_camera = scene().createCamera(
        {m_smoothCamPos, {m_camPitch, m_camYaw, 0.0f}},
        {m_camFov, 0.1f, 400.0f, true},
        "MainCamera");

    m_phys->BuildFromECS(registry());
}

// ============================================================================
// Input handling
// ============================================================================
void Demo::handleInput(float dt) {
    if (!m_cct || !registry().valid(m_camera)) return;

    // Cursor lock/unlock
    if (Input::IsMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT) && !m_cursorLocked) {
        m_cursorLocked = true;
        Input::SetCursorLocked(true);
    }
    if (Input::IsKeyJustPressed(GLFW_KEY_ESCAPE) && m_cursorLocked) {
        m_cursorLocked = false;
        Input::SetCursorLocked(false);
    }

    // Toggle 1st/3rd person (V)
    if (Input::IsActionJustPressed("ToggleView")) {
        m_thirdPerson = !m_thirdPerson;
        if (registry().valid(m_playerModel))
            registry().get<SkinnedMeshRenderer>(m_playerModel).visible = m_thirdPerson && !m_playerVehicle;
    }

    // Enter/exit vehicle (E)
    if (Input::IsActionJustPressed("Interact")) {
        if (m_playerVehicle) {
            // Exit
            m_playerVehicle->setPlayerControlled(false);
            m_playerPos = m_playerVehicle->driverExitPosition();
            m_playerVel = glm::vec3(0.0f);
            m_phys->MoveCharacter(m_cct, glm::vec3(0.0f), dt, m_playerPos);
            m_playerVehicle = nullptr;
            // Reset camera yaw to face same direction as before
            if (registry().valid(m_playerModel))
                registry().get<SkinnedMeshRenderer>(m_playerModel).visible = m_thirdPerson;
        } else {
            // Try to enter nearest vehicle
            Physics::Vehicle* closest = nullptr;
            float minDist = 5.0f;
            for (auto& v : m_vehicles) {
                float d = glm::distance(m_playerPos, v->position());
                if (d < minDist) { minDist = d; closest = v.get(); }
            }
            if (closest) {
                m_playerVehicle = closest;
                m_playerVehicle->setPlayerControlled(true);
                // Convert vehicle yaw to camera yaw so camera faces same direction as car
                m_targetYaw = 90.0f - m_playerVehicle->yaw();
                m_camYaw = m_targetYaw;
                if (registry().valid(m_playerModel))
                    registry().get<SkinnedMeshRenderer>(m_playerModel).visible = false;
            }
        }
    }

    // Check if near a vehicle (for HUD prompt)
    m_canEnterVehicle = false;
    if (!m_playerVehicle) {
        for (auto& v : m_vehicles) {
            if (glm::distance(m_playerPos, v->position()) < 5.0f) {
                m_canEnterVehicle = true;
                break;
            }
        }
    }

    // ---- Mouse look ----
    if (m_cursorLocked) {
        glm::vec2 delta = Input::GetMouseDelta();
        if (!m_playerVehicle) {
            // Pedestrian: free mouse look
            m_targetYaw   -= delta.x * 0.12f;
            m_targetPitch -= delta.y * 0.12f;
            m_targetPitch = glm::clamp(m_targetPitch, -80.0f, 80.0f);
        } else {
            // Vehicle: mouse orbits around car, but auto-follows heading
            m_targetYaw   -= delta.x * 0.15f;
            m_targetPitch -= delta.y * 0.10f;
            m_targetPitch = glm::clamp(m_targetPitch, -35.0f, 45.0f);
        }
    }

    // ---- Vehicle driving (GTA style) ----
    if (m_playerVehicle) {
        float throttle = Input::GetAxis("Vertical");     // W=+1 (gas), S=-1 (brake/reverse)
        float steer    = Input::GetAxis("Horizontal");   // D=+1 (right), A=-1 (left)
        bool handbrake = Input::IsActionPressed("Jump");  // Space
        m_playerVehicle->setInputs(throttle, steer, handbrake);

        // Auto-follow: gradually pull camera yaw toward vehicle heading
        float vehicleCamYaw = 90.0f - m_playerVehicle->yaw();
        float diff = vehicleCamYaw - m_targetYaw;
        while (diff >  180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        m_targetYaw += diff * (1.0f - std::exp(-3.5f * dt));
        // Gradually return pitch to default driving view
        m_targetPitch = glm::mix(m_targetPitch, -10.0f, 1.0f - std::exp(-2.0f * dt));
        return;
    }

    // ---- Pedestrian movement ----
    // Smooth camera angles
    m_camYaw   = glm::mix(m_camYaw, m_targetYaw, 1.0f - std::exp(-35.0f * dt));
    m_camPitch = glm::mix(m_camPitch, m_targetPitch, 1.0f - std::exp(-35.0f * dt));

    float yawRad = glm::radians(m_camYaw);
    glm::vec3 front(std::cos(yawRad), 0.0f, std::sin(yawRad));
    // Camera right = cross(front, up) = (-sin(yaw), 0, cos(yaw))
    glm::vec3 right(-front.z, 0.0f, front.x);

    glm::vec3 moveDir{0.0f};
    moveDir += front * Input::GetAxis("Vertical");
    moveDir += right * Input::GetAxis("Horizontal");

    bool isMoving = glm::length(moveDir) > 0.01f;
    if (isMoving) moveDir = glm::normalize(moveDir);

    float speed = Input::IsActionPressed("Sprint") ? 7.2f : 3.8f;
    glm::vec3 targetVel = moveDir * speed;
    m_playerMove = glm::mix(m_playerMove, targetVel, 1.0f - std::exp(-20.0f * dt));

    // Gravity and jump
    if (!m_onGround) m_playerVel.y -= 18.0f * dt;
    else {
        m_playerVel.y = -0.5f;
        if (Input::IsActionJustPressed("Jump")) m_playerVel.y = 6.2f;
    }

    glm::vec3 totalDisp = (m_playerMove + glm::vec3{0.0f, m_playerVel.y, 0.0f}) * dt;
    m_onGround = m_phys->MoveCharacter(m_cct, totalDisp, dt, m_playerPos);
    if (m_onGround && m_playerVel.y < 0.0f) m_playerVel.y = 0.0f;

    // Animate and orient player model
    if (registry().valid(m_playerModel)) {
        auto& pTr = registry().get<Transform>(m_playerModel);
        pTr.position = m_playerPos - glm::vec3{0.0f, 0.95f, 0.0f};

        if (isMoving) {
            float targetModelYaw = glm::degrees(std::atan2(m_playerMove.x, m_playerMove.z));
            float diff = targetModelYaw - pTr.rotation.y;
            while (diff < -180.0f) diff += 360.0f;
            while (diff >  180.0f) diff -= 360.0f;
            pTr.rotation.y += diff * (1.0f - std::exp(-15.0f * dt));
        }

        auto* animComp = registry().try_get<AnimatorComponent>(m_playerModel);
        if (animComp && animComp->animator) {
            animComp->animator->setSpeed(isMoving ? (speed > 4.0f ? 1.6f : 1.0f) : 0.0f);
            if (!isMoving) animComp->animator->setTime(0.033f);
        }
    }
}

// ============================================================================
// Camera
// ============================================================================
void Demo::updateCamera(float dt) {
    if (!registry().valid(m_camera)) return;

    auto& camTr   = registry().get<Transform>(m_camera);
    auto& camComp = registry().get<Camera>(m_camera);

    if (m_playerVehicle) {
        // -- Chase camera (auto-follow with smooth lag) --
        // Smooth yaw/pitch toward target
        float yawDiff = m_targetYaw - m_camYaw;
        while (yawDiff >  180.0f) yawDiff -= 360.0f;
        while (yawDiff < -180.0f) yawDiff += 360.0f;
        m_camYaw += yawDiff * (1.0f - std::exp(-18.0f * dt));
        m_camPitch = glm::mix(m_camPitch, m_targetPitch, 1.0f - std::exp(-18.0f * dt));

        float yawRad = glm::radians(m_camYaw);
        glm::vec3 camDir(std::cos(yawRad), 0.0f, std::sin(yawRad));
        glm::vec3 carPos = m_playerVehicle->position();

        float spd = std::abs(m_playerVehicle->speedKmH());
        float dist   = 5.5f + glm::clamp(spd * 0.020f, 0.0f, 2.5f);
        float height = 2.2f + glm::clamp(spd * 0.006f, 0.0f, 0.8f);

        glm::vec3 targetPos = carPos - camDir * dist + glm::vec3{0.0f, height, 0.0f};
        m_smoothCamPos = glm::mix(m_smoothCamPos, targetPos, 1.0f - std::exp(-15.0f * dt));

        // Speed-based FOV widening
        m_camFov = glm::mix(m_camFov, 60.0f + glm::clamp(spd * 0.10f, 0.0f, 12.0f),
                            1.0f - std::exp(-8.0f * dt));
        camComp.fov = m_camFov;

        camTr.position = m_smoothCamPos;
        camTr.rotation = {m_camPitch, m_camYaw, 0.0f};
    } else {
        // -- Pedestrian camera --
        float yawRad = glm::radians(m_camYaw);
        glm::vec3 front(std::cos(yawRad), 0.0f, std::sin(yawRad));

        glm::vec3 targetPos = m_thirdPerson
            ? (m_playerPos + glm::vec3{0.0f, 1.3f, 0.0f} - front * 4.5f)
            : (m_playerPos + glm::vec3{0.0f, 0.55f, 0.0f});

        m_smoothCamPos = glm::mix(m_smoothCamPos, targetPos, 1.0f - std::exp(-25.0f * dt));
        m_camFov = glm::mix(m_camFov, 60.0f, 1.0f - std::exp(-12.0f * dt));
        camComp.fov = m_camFov;

        camTr.position = m_smoothCamPos;
        camTr.rotation = {m_camPitch, m_camYaw, 0.0f};
    }
}

// ============================================================================
// Fixed update (physics, particles, cloth)
// ============================================================================
void Demo::onFixedUpdate(float fixedDt) {
    for (auto& v : m_vehicles)
        v->update(fixedDt, m_phys.get());

    if (m_cape && registry().valid(m_playerModel) && !m_playerVehicle) {
        auto& pTr = registry().get<Transform>(m_playerModel);
        float pYaw = glm::radians(pTr.rotation.y);
        glm::vec3 pFwd(std::sin(pYaw), 0.0f, std::cos(pYaw));
        glm::vec3 pRight(pFwd.z, 0.0f, -pFwd.x);

        glm::vec3 shoulderL = m_playerPos + glm::vec3(0.0f, 0.46f, 0.0f) - pRight * 0.18f - pFwd * 0.06f;
        glm::vec3 shoulderR = m_playerPos + glm::vec3(0.0f, 0.46f, 0.0f) + pRight * 0.18f - pFwd * 0.06f;
        glm::vec3 wind = -m_playerMove * 1.5f + glm::vec3(0.3f, 0.0f, 0.2f);
        m_cape->update(fixedDt, shoulderL, shoulderR,
                       m_playerPos + glm::vec3(0.0f, 0.10f, 0.0f), 0.26f, wind);
    }

    if (m_particles) {
        for (const auto& fp : m_firePositions)
            m_particles->emitFire(fp + glm::vec3(0.0f, 0.45f, 0.0f), 2, 0.10f);
        m_particles->update(fixedDt);
    }

    if (m_phys) {
        m_phys->Step(fixedDt);
        m_phys->SyncToECS(registry());
    }
}

// ============================================================================
// Main update
// ============================================================================
void Demo::onUpdate(float dt) {
    handleInput(dt);

    // Throw crate (F)
    if (Input::IsActionJustPressed("Throw") && registry().valid(m_camera)) {
        auto& camTr = registry().get<Transform>(m_camera);
        float yaw = glm::radians(m_camYaw), pitch = glm::radians(m_camPitch);
        glm::vec3 dir{std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch)};
        dir = glm::normalize(dir);

        auto e = Prefabs::PhysicsCrate(scene(), camTr.position + dir * 1.5f, glm::vec3{0.5f}, 1.0f);
        m_dynamicObjects.push_back(e.id());
        m_phys->BuildFromECS(registry());
        m_phys->ThrowFrom(e.id(), dir * 18.0f + glm::vec3{0.0f, 2.5f, 0.0f});
    }

    AI::NPCSystem(registry(), dt, m_playerPos);
    Systems::AnimationUpdate(registry(), dt);
    updateCamera(dt);

    // Render pipeline
    int w, h; window().getFramebufferSize(w, h);
    if (!m_pipe) m_pipe = std::make_unique<RenderPipeline>(w, h);
    m_pipe->resize(w, h);
    m_pipe->syncFromRegistry(registry());
    m_pipe->beginFrame();
    ClusteredMesh::ResetStats();
    Systems::Render(registry(), *m_litShader, window());

    // Draw cloth cape when on foot in 3rd person
    if (registry().valid(m_camera) && m_cape && m_thirdPerson && !m_playerVehicle) {
        auto& camComp = registry().get<Camera>(m_camera);
        auto& camTr = registry().get<Transform>(m_camera);
        float aspect = float(w) / float(h ? h : 1);
        glm::mat4 view = Camera::viewFromTransform(camTr);
        glm::mat4 proj = camComp.projection(aspect);

        m_litShader->use();
        m_litShader->setMat4("uView", view);
        m_litShader->setMat4("uProj", proj);
        m_litShader->setMat4("uModel", glm::mat4(1.0f));
        m_litShader->setMat4("uMVP", proj * view);
        m_litShader->setMat3("uNormalMat", glm::mat3(1.0f));
        m_litShader->setVec3("uAlbedo", {0.65f, 0.08f, 0.12f});
        m_litShader->setFloat("uMetallic", 0.05f);
        m_litShader->setFloat("uRoughness", 0.85f);
        m_litShader->setFloat("uAO", 1.0f);
        m_litShader->setVec3("uEmissive", glm::vec3(0.0f));
        m_litShader->setBool("uUseDiffuseMap", false);
        m_litShader->setBool("uUseSpecularMap", false);
        glDisable(GL_CULL_FACE);
        m_cape->draw();
        glEnable(GL_CULL_FACE);
    }

    // Draw particles
    if (m_particles && registry().valid(m_camera)) {
        auto& camComp = registry().get<Camera>(m_camera);
        auto& camTr = registry().get<Transform>(m_camera);
        float aspect = float(w) / float(h ? h : 1);
        m_particles->draw(Camera::viewFromTransform(camTr), camComp.projection(aspect));
    }

    m_pipe->endFrame();
    renderHUD(dt, w, h);
}

// ============================================================================
// HUD
// ============================================================================
void Demo::renderHUD(float dt, int w, int h) {
    kGUI::BeginFrame();

    float cx = float(w) * 0.5f;
    float cy = float(h) * 0.5f;

    float currentZ = m_playerVehicle ? m_playerVehicle->position().z : m_playerPos.z;
    std::string zone = "Start";
    if      (currentZ >= -170.0f && currentZ < -80.0f) zone = "Zone 1: Projected Decals";
    else if (currentZ >=  -80.0f && currentZ <   0.0f) zone = "Zone 2: PhysX Destruction";
    else if (currentZ >=    0.0f && currentZ <  80.0f) zone = "Zone 3: Volumetric Fire";
    else if (currentZ >=   80.0f && currentZ < 170.0f) zone = "Zone 4: ClusterLOD Foliage";
    else if (currentZ >=  170.0f)                      zone = "Zone 5: Skeletal Animation";

    // Top info bar
    kGUI::Panel(16.0f, 16.0f, 520.0f, 68.0f, {0.04f, 0.06f, 0.09f, 0.85f}, 8.0f);
    kGUI::Text(28.0f, 24.0f, 0.82f, "cjoka Engine | Grand Highway Showcase", {1.0f, 0.94f, 0.84f, 1.0f});

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(0) << (dt > 0.0f ? 1.0f / dt : 0.0f) << " FPS | "
       << (m_playerVehicle ? "Vehicle" : (m_thirdPerson ? "3rd Person" : "1st Person"))
       << " | " << zone;
    kGUI::Text(28.0f, 50.0f, 0.46f, ss.str(), {0.80f, 0.86f, 0.95f, 1.0f});

    if (m_playerVehicle) {
        // Speedometer
        float speed = m_playerVehicle->speedKmH();
        kGUI::Panel(float(w) - 240.0f, float(h) - 95.0f, 224.0f, 80.0f, {0.04f, 0.06f, 0.09f, 0.88f}, 8.0f);

        std::ostringstream spd;
        spd << std::fixed << std::setprecision(0) << std::abs(speed) << " KM/H";
        kGUI::Text(float(w) - 222.0f, float(h) - 82.0f, 1.0f, spd.str(), {1.0f, 0.90f, 0.20f, 1.0f});
        kGUI::Text(float(w) - 222.0f, float(h) - 48.0f, 0.42f, "Space: Drift | E: Exit Car",
                   {0.78f, 0.83f, 0.94f, 1.0f});

        // Controls hint
        kGUI::Panel(16.0f, float(h) - 36.0f, 520.0f, 26.0f, {0.0f, 0.0f, 0.0f, 0.60f}, 4.0f);
        kGUI::Text(24.0f, float(h) - 28.0f, 0.42f,
                   "W/S Gas/Brake | A/D Steer | Space Drift | E Exit | LMB Lock Mouse",
                   {1.0f, 1.0f, 1.0f, 0.92f});
    } else {
        // Crosshair
        kGUI::Panel(cx - 3.0f, cy - 1.0f, 6.0f, 2.0f, {1.0f, 1.0f, 1.0f, 0.70f});
        kGUI::Panel(cx - 1.0f, cy - 3.0f, 2.0f, 6.0f, {1.0f, 1.0f, 1.0f, 0.70f});

        if (m_canEnterVehicle) {
            kGUI::Panel(cx - 150.0f, cy + 65.0f, 300.0f, 40.0f, {0.06f, 0.10f, 0.16f, 0.90f}, 6.0f);
            kGUI::Text(cx - 118.0f, cy + 77.0f, 0.62f, "Press [E] to Enter Vehicle",
                       {1.0f, 0.92f, 0.35f, 1.0f});
        }

        kGUI::Panel(16.0f, float(h) - 36.0f, 550.0f, 26.0f, {0.0f, 0.0f, 0.0f, 0.60f}, 4.0f);
        kGUI::Text(24.0f, float(h) - 28.0f, 0.42f,
                   "WASD Move | Space Jump | F Throw | V View | E Enter Car | LMB Lock",
                   {1.0f, 1.0f, 1.0f, 0.92f});
    }

    kGUI::EndFrame();
}

// ============================================================================
// ImGui Inspector
// ============================================================================
void Demo::onImGuiRender() {
    static bool showInspector = true;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("cjoka")) {
            ImGui::MenuItem("Inspector", nullptr, &showInspector);
            ImGui::EndMenu();
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 260);
        ImGui::TextDisabled("Grand Highway Showcase | PhysX 5.5");
        ImGui::EndMainMenuBar();
    }

    if (!showInspector) return;

    ImGui::SetNextWindowSize(ImVec2(320, 380), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(16, 100), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Inspector", &showInspector)) {
        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("FPS: %.1f (%.2f ms)", ImGui::GetIO().Framerate,
                        1000.0f / std::max(ImGui::GetIO().Framerate, 1.0f));
            ImGui::Text("Entities: %zu", registry().aliveCount());
            ImGui::Text("Meshes: %zu | Skinned: %zu",
                        registry().count<MeshRenderer>(), registry().count<SkinnedMeshRenderer>());
            ImGui::Text("Physics: %zu | Decals: %zu",
                        registry().count<cjoka_phys::Rigidbody>(), registry().count<Decal>());
            ImGui::Text("Point Lights: %zu", registry().count<PointLight>());
        }

        if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", &m_playerPos.x, 0.1f);
            if (ImGui::Button("Reset to Spawn")) {
                m_playerPos = {-3.5f, 1.10f, -202.0f};
                m_playerVel = glm::vec3(0.0f);
                m_phys->MoveCharacter(m_cct, glm::vec3(0.0f), 0.016f, m_playerPos);
                m_targetYaw = 90.0f;
                m_camYaw = 90.0f;
            }
        }

        if (ImGui::CollapsingHeader("Spawner")) {
            if (ImGui::Button("Spawn Crate") && registry().valid(m_camera)) {
                auto& ct = registry().get<Transform>(m_camera);
                float yaw = glm::radians(m_camYaw), pitch = glm::radians(m_camPitch);
                glm::vec3 dir{std::cos(yaw)*std::cos(pitch), std::sin(pitch), std::sin(yaw)*std::cos(pitch)};
                dir = glm::normalize(dir);
                auto e = Prefabs::PhysicsCrate(scene(), ct.position + dir * 1.5f, glm::vec3(0.5f), 1.0f);
                m_dynamicObjects.push_back(e.id());
                m_phys->BuildFromECS(registry());
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Debris")) {
                for (Entity e : m_dynamicObjects) {
                    if (registry().valid(e)) {
                        auto* rb = registry().try_get<cjoka_phys::Rigidbody>(e);
                        if (rb && rb->pxActor) m_phys->RemoveActor(rb->pxActor);
                        scene().destroy(e);
                    }
                }
                m_dynamicObjects.clear();
            }
        }

        if (ImGui::CollapsingHeader("Environment")) {
            if (auto v = registry().view<DirectionalLight>(); v.begin() != v.end()) {
                auto& sun = registry().get<DirectionalLight>(*v.begin());
                ImGui::DragFloat3("Sun Dir", &sun.direction.x, 0.02f, -1.0f, 1.0f);
                ImGui::SliderFloat("Sun Intensity", &sun.intensity, 0.0f, 5.0f);
            }
            if (auto v = registry().view<Fog>(); v.begin() != v.end()) {
                auto& fog = registry().get<Fog>(*v.begin());
                ImGui::SliderFloat("Fog Density", &fog.density, 0.0f, 0.01f, "%.4f");
            }
            if (auto v = registry().view<PostProcessSettings>(); v.begin() != v.end()) {
                auto& post = registry().get<PostProcessSettings>(*v.begin());
                ImGui::Checkbox("Bloom", &post.bloom);
                ImGui::SliderFloat("Bloom Threshold", &post.bloomThreshold, 0.5f, 2.0f);
                ImGui::SliderFloat("Exposure", &post.exposure, 0.1f, 3.0f);
            }
        }
    }
    ImGui::End();
}

// ============================================================================
// Shutdown
// ============================================================================
void Demo::onShutdown() {
    if (m_cct) { m_phys->RemoveCharacter(m_cct); m_cct = nullptr; }
    m_phys.reset();
    cjoka_phys::Global::Shutdown();
    std::cout << "[Demo] Shutdown complete\n";
}
