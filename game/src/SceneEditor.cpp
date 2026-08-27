#include "SceneEditor.h"
#include "engine/Core/Input.h"
#include "engine/ECS/Systems.h"
#include "engine/Renderer/DefaultShaders.h"
#include "engine/Assets/AssetManager.h"
#include "engine/Physics/Physics.h"
#include "Prefabs.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace {
// Ray vs AABB intersection in local space (Slab Method)
bool RayIntersectAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& minB, const glm::vec3& maxB, float& tOut) {
    float tmin = 0.0f;
    float tmax = 1e9f;

    for (int i = 0; i < 3; ++i) {
        if (std::abs(rayDir[i]) < 1e-6f) {
            if (rayOrigin[i] < minB[i] || rayOrigin[i] > maxB[i]) return false;
        } else {
            float ood = 1.0f / rayDir[i];
            float t1 = (minB[i] - rayOrigin[i]) * ood;
            float t2 = (maxB[i] - rayOrigin[i]) * ood;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    tOut = tmin;
    return true;
}
}

SceneEditor::SceneEditor()
    : Application(1600, 900, "cjoka Engine | Scene Editor & Graphics Sandbox") {
}

SceneEditor::~SceneEditor() = default;

void SceneEditor::onInit() {
    std::cout << "[SceneEditor] Initializing Editor & Graphics Sandbox\n";

    cjoka_phys::Global::Init();
    m_phys = std::make_unique<cjoka_phys::World>(glm::vec3{0.0f, -9.81f, 0.0f});

    m_litShader = std::make_unique<Shader>(DefaultShaders::kLitVS, DefaultShaders::kLitFS);

    // Hook up OS Drag & Drop callback
    window().setDropCallback([this](const std::vector<std::string>& paths) {
        onFilesDropped(paths);
    });

    refreshAvailableScenes();

    // Check if custom_scene.json exists
    std::ifstream check("assets/custom_scene.json");
    if (check.good()) {
        check.close();
        loadSceneFromFile("assets/custom_scene.json");
    } else {
        loadDefaultShowcase();
    }
}

void SceneEditor::refreshAvailableScenes() {
    m_availableScenes.clear();
    if (std::filesystem::exists("assets")) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator("assets")) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                m_availableScenes.push_back(entry.path().string());
            }
        }
    }
}

void SceneEditor::onFilesDropped(const std::vector<std::string>& paths) {
    glm::vec3 spawnPos = m_camPos + glm::vec3(0, 0, 4);
    for (const auto& pathStr : paths) {
        std::filesystem::path p(pathStr);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        std::string stem = p.stem().string();

        if (ext == ".obj") {
            std::cout << "[DragDrop] Spawning 3D Model: " << pathStr << "\n";
            std::string tex = "";
            std::string candidateTex = p.parent_path().string() + "/" + stem + ".png";
            if (std::filesystem::exists(candidateTex)) tex = candidateTex;
            else if (std::filesystem::exists("assets/textures/colormap.png")) tex = "assets/textures/colormap.png";

            m_selectedEntity = spawnModel(stem, pathStr, tex, spawnPos, 1.0f);
            spawnPos.x += 2.5f;
        } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
            std::cout << "[DragDrop] Applying Texture: " << pathStr << "\n";
            if (registry().valid(m_selectedEntity) && registry().has<MeshRenderer>(m_selectedEntity)) {
                auto& mr = registry().get<MeshRenderer>(m_selectedEntity);
                mr.texturePath = pathStr;
                mr.material.diffuseMap = Assets::Texture(pathStr, true);
                mr.material.useDiffuseMap = (mr.material.diffuseMap && mr.material.diffuseMap->valid());
            }
        } else if (ext == ".json") {
            std::cout << "[DragDrop] Loading Scene: " << pathStr << "\n";
            loadSceneFromFile(pathStr);
            strncpy(m_sceneFileBuf, pathStr.c_str(), sizeof(m_sceneFileBuf));
        }
    }
}

void SceneEditor::updateScripts(float dt) {
    if (!m_isPlaying) return;

    for (Entity e : registry().view<NativeScript>()) {
        auto& ns = registry().get<NativeScript>(e);
        if (!ns.instance && ns.instantiate) {
            ns.instance = ns.instantiate();
            if (ns.instance) {
                ns.instance->_init(e, &registry());
                ns.instance->onCreate();
                ns.instance->onStart();
            }
        }
        if (ns.instance) {
            ns.instance->onUpdate(dt);
        }
    }
}

void SceneEditor::loadDefaultShowcase() {
    scene().clear();
    m_selectedEntity = NullEntity;

    // Create Main Game Camera
    auto camRef = scene().create("MainCamera", Transform{{0.0f, 3.5f, -12.0f}, {-10.0f, 0.0f, 0.0f}, glm::vec3(1.0f)});
    camRef.add<Camera>(Camera{65.0f, 0.1f, 1000.0f, true, true, 10.0f});

    // Atmosphere
    scene().create("Sky").add<Sky>(Sky::Sunset());
    scene().create("Fog").add<Fog>(Fog{{0.2f, 0.22f, 0.28f}, 0.0035f});
    scene().create("Sun").add<DirectionalLight>(DirectionalLight{glm::normalize(glm::vec3{-0.4f, -0.8f, -0.3f}), {1.0f, 0.92f, 0.82f}, 2.5f});
    scene().create("Ambient").add<AmbientLight>(AmbientLight{{0.12f, 0.14f, 0.18f}, 1.0f});
    scene().create("PostProcess").add<PostProcessSettings>(PostProcessSettings::Cinematic());

    // Large floor grid
    auto floorTex = Assets::Texture("assets/textures/prototype_floor.png", true);
    Material floorMat = Material::Textured(floorTex, {0.9f, 0.9f, 0.9f}, 0.1f, 0.8f);
    scene().create("GroundPlane", Transform{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {120.0f, 0.1f, 120.0f}})
           .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), floorMat).setClusterLOD(false));

    // Centerpiece: Exhibition platform with lighting and cars
    Material podiumMat;
    podiumMat.albedo = {0.15f, 0.16f, 0.18f};
    podiumMat.metallic = 0.85f;
    podiumMat.roughness = 0.2f;
    scene().create("ExhibitionPodium", Transform{{0.0f, 0.15f, 0.0f}, {}, {24.0f, 0.3f, 24.0f}})
           .add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), podiumMat).setClusterLOD(false));

    // Cars on podium
    spawnModel("SportsSedan", "assets/models/cars/sedan-sports.obj", "assets/textures/colormap.png", {-5.0f, 0.3f, 0.0f}, 1.4f);
    spawnModel("PoliceCruiser", "assets/models/cars/police.obj", "assets/textures/colormap.png", {5.0f, 0.3f, 0.0f}, 1.4f);
    spawnModel("TaxiCab", "assets/models/cars/taxi.obj", "assets/textures/colormap.png", {0.0f, 0.3f, -5.0f}, 1.4f);

    // Decorative props
    spawnModel("Barrel_1", "assets/models/barrel.obj", "assets/textures/barrel.png", {-8.0f, 0.3f, 6.0f}, 1.2f);
    spawnModel("Barrel_2", "assets/models/barrel.obj", "assets/textures/barrel.png", {-7.2f, 0.3f, 6.5f}, 1.2f);
    spawnModel("Plant_1", "assets/models/indoor_plant.obj", "assets/textures/indoor_plant_COL.jpg", {8.0f, 0.3f, 6.0f}, 0.25f);
    spawnModel("Bench_1", "assets/models/bench.obj", "", {0.0f, 0.3f, 7.0f}, 1.3f);

    // Forward+ Lighting showcase: Ring of vibrant lights
    for (int i = 0; i < 12; ++i) {
        float angle = (float(i) / 12.0f) * glm::two_pi<float>();
        float radius = 10.0f;
        glm::vec3 pos{std::cos(angle) * radius, 2.5f, std::sin(angle) * radius};
        glm::vec3 col{
            0.5f + 0.5f * std::sin(angle),
            0.5f + 0.5f * std::sin(angle + 2.0f),
            0.5f + 0.5f * std::sin(angle + 4.0f)
        };
        spawnPointLight(pos, col, 6.0f, 15.0f);
    }
}

void SceneEditor::newScene() {
    scene().clear();
    m_selectedEntity = NullEntity;
    auto camRef = scene().create("MainCamera", Transform{{0.0f, 3.5f, -12.0f}, {-10.0f, 0.0f, 0.0f}, glm::vec3(1.0f)});
    camRef.add<Camera>(Camera{65.0f, 0.1f, 1000.0f, true, true, 10.0f});
    scene().create("Sky").add<Sky>(Sky::Sunset());
    scene().create("Sun").add<DirectionalLight>(DirectionalLight{glm::normalize(glm::vec3{-0.4f, -0.8f, -0.3f}), {1.0f, 0.92f, 0.82f}, 2.0f});
    scene().create("Ambient").add<AmbientLight>(AmbientLight{{0.15f, 0.15f, 0.15f}, 1.0f});
    scene().create("PostProcess").add<PostProcessSettings>(PostProcessSettings::Cinematic());
}

Entity SceneEditor::spawnPrimitive(const std::string& type, const glm::vec3& pos) {
    Transform tr{pos, {}, glm::vec3(1.0f)};
    Material mat;
    mat.albedo = {0.8f, 0.8f, 0.85f};
    mat.metallic = 0.2f;
    mat.roughness = 0.5f;

    if (type == "Cube") {
        auto ref = scene().createCube(tr, mat, "Cube");
        ref.get<MeshRenderer>().assetPath = "primitive:cube";
        return ref.id();
    }
    if (type == "Sphere") {
        auto ref = scene().createSphere(tr, mat, 0.5f, "Sphere");
        ref.get<MeshRenderer>().assetPath = "primitive:sphere";
        return ref.id();
    }
    if (type == "Quad" || type == "Plane") {
        tr.scale = {10.0f, 0.1f, 10.0f};
        auto ref = scene().createCube(tr, mat, "Plane");
        ref.get<MeshRenderer>().assetPath = "primitive:plane";
        return ref.id();
    }
    auto ref = scene().createCube(tr, mat, type);
    ref.get<MeshRenderer>().assetPath = "primitive:cube";
    return ref.id();
}

Entity SceneEditor::spawnModel(const std::string& name, const std::string& modelPath, const std::string& texPath, const glm::vec3& pos, float scale) {
    Transform tr{pos, {}, glm::vec3(scale)};
    Material mat;
    mat.albedo = {1.0f, 1.0f, 1.0f};
    mat.metallic = 0.2f;
    mat.roughness = 0.6f;
    if (!texPath.empty()) {
        mat.diffuseMap = Assets::Texture(texPath, true);
        mat.useDiffuseMap = (mat.diffuseMap && mat.diffuseMap->valid());
    }
    auto ref = scene().create(name, tr);
    MeshRenderer mr(Assets::Mesh(modelPath), mat);
    mr.assetPath = modelPath;
    mr.texturePath = texPath;
    mr.setClusterLOD(false);
    ref.add<MeshRenderer>(mr);
    return ref.id();
}

Entity SceneEditor::spawnPointLight(const glm::vec3& pos, const glm::vec3& col, float intensity, float range) {
    Transform tr{pos, {}, glm::vec3(0.2f)};
    Material orbMat = Material::Emissive(col, intensity * 2.0f);
    auto ref = scene().create("PointLight", tr);
    ref.add<MeshRenderer>(MeshRenderer(Assets::Sphere(0.15f), orbMat).setClusterLOD(false));
    ref.add<PointLight>(PointLight{col, intensity, range});
    return ref.id();
}

void SceneEditor::pickObjectUnderMouse() {
    double mouseX, mouseY;
    window().getCursorPos(mouseX, mouseY);
    int w, h; window().getFramebufferSize(w, h);
    if (w <= 0 || h <= 0) return;

    float ndcX = (2.0f * (float)mouseX) / (float)w - 1.0f;
    float ndcY = 1.0f - (2.0f * (float)mouseY) / (float)h;

    float aspect = float(w) / float(h ? h : 1);
    float yawRad = glm::radians(m_camYaw);
    float pitchRad = glm::radians(m_camPitch);
    glm::vec3 front{
        std::cos(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        std::sin(yawRad) * std::cos(pitchRad)
    };
    front = glm::normalize(front);
    glm::mat4 view = glm::lookAt(m_camPos, m_camPos + front, glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(m_camFov), aspect, 0.1f, 2000.0f);
    glm::mat4 invVP = glm::inverse(proj * view);

    glm::vec4 nearPoint = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint  /= farPoint.w;

    glm::vec3 rayOrigin = glm::vec3(nearPoint);
    glm::vec3 rayDir = glm::normalize(glm::vec3(farPoint - nearPoint));

    Entity closestEntity = NullEntity;
    float closestT = 1e9f;

    for (Entity e : registry().view<Transform>()) {
        auto& tr = registry().get<Transform>(e);
        glm::mat4 model = tr.matrix();
        glm::mat4 invModel = glm::inverse(model);

        glm::vec3 localRayOrigin = glm::vec3(invModel * glm::vec4(rayOrigin, 1.0f));
        glm::vec3 localRayDir = glm::normalize(glm::vec3(invModel * glm::vec4(rayDir, 0.0f)));

        glm::vec3 minB{-0.5f}, maxB{0.5f};

        if (registry().has<MeshRenderer>(e)) {
            auto& mr = registry().get<MeshRenderer>(e);
            if (mr.mesh && !mr.mesh->empty()) {
                minB = mr.mesh->minExtents();
                maxB = mr.mesh->maxExtents();
            }
        } else if (registry().has<PointLight>(e)) {
            minB = glm::vec3(-0.3f);
            maxB = glm::vec3(0.3f);
        } else if (registry().has<Camera>(e)) {
            minB = glm::vec3(-0.4f);
            maxB = glm::vec3(0.4f);
        } else {
            continue;
        }

        float t = 0.0f;
        if (RayIntersectAABB(localRayOrigin, localRayDir, minB, maxB, t)) {
            glm::vec3 worldHit = glm::vec3(model * glm::vec4(localRayOrigin + localRayDir * t, 1.0f));
            float dist = glm::distance(rayOrigin, worldHit);
            if (dist < closestT) {
                closestT = dist;
                closestEntity = e;
            }
        }
    }

    if (closestEntity != NullEntity) {
        m_selectedEntity = closestEntity;
        std::cout << "[SceneEditor] Selected Entity: " << (uint32_t)m_selectedEntity << "\n";
    }
}

void SceneEditor::updateCamera(float dt) {
    ImGuiIO& io = ImGui::GetIO();

    if (m_isPlaying) {
        Entity gameCam = NullEntity;
        for (Entity e : registry().view<Camera, Transform>()) {
            if (registry().get<Camera>(e).primary) {
                gameCam = e;
                break;
            }
            if (gameCam == NullEntity) gameCam = e;
        }

        if (registry().valid(gameCam) && registry().has<Transform>(gameCam)) {
            auto& tr = registry().get<Transform>(gameCam);

            if (window().isKeyPressed(GLFW_KEY_ESCAPE)) {
                m_isPlaying = false;
                loadSceneFromFile("assets/.play_mode_backup.json");
                window().setCursorMode(GLFW_CURSOR_NORMAL);
                m_flycamActive = false;
                return;
            }

            // Mouse look in play mode
            bool lookActive = window().isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) ||
                              window().isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) ||
                              m_flycamActive;

            static bool wasPlayLook = false;
            static double lastPlayX = 0, lastPlayY = 0;

            if (lookActive) {
                double curX, curY;
                window().getCursorPos(curX, curY);
                if (!wasPlayLook) {
                    wasPlayLook = true;
                    lastPlayX = curX;
                    lastPlayY = curY;
                    window().setCursorMode(GLFW_CURSOR_DISABLED);
                } else {
                    float dx = static_cast<float>(curX - lastPlayX);
                    float dy = static_cast<float>(curY - lastPlayY);
                    lastPlayX = curX;
                    lastPlayY = curY;

                    tr.rotation.y += dx * 0.15f;
                    tr.rotation.x -= dy * 0.15f * (m_invertY ? -1.0f : 1.0f);
                    tr.rotation.x = glm::clamp(tr.rotation.x, -89.0f, 89.0f);
                }
            } else {
                if (wasPlayLook) {
                    wasPlayLook = false;
                    window().setCursorMode(GLFW_CURSOR_NORMAL);
                }
            }

            // Game movement (WASD + Arrows)
            if (!io.WantTextInput) {
                float yawRad = glm::radians(tr.rotation.y);
                float pitchRad = glm::radians(tr.rotation.x);
                glm::vec3 front{
                    std::cos(yawRad) * std::cos(pitchRad),
                    std::sin(pitchRad),
                    std::sin(yawRad) * std::cos(pitchRad)
                };
                front = glm::normalize(front);
                glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));

                glm::vec3 moveDir{0.0f};
                if (window().isKeyPressed(GLFW_KEY_W) || window().isKeyPressed(GLFW_KEY_UP)) moveDir += front;
                if (window().isKeyPressed(GLFW_KEY_S) || window().isKeyPressed(GLFW_KEY_DOWN)) moveDir -= front;
                if (window().isKeyPressed(GLFW_KEY_D) || window().isKeyPressed(GLFW_KEY_RIGHT)) moveDir += right;
                if (window().isKeyPressed(GLFW_KEY_A) || window().isKeyPressed(GLFW_KEY_LEFT)) moveDir -= right;

                moveDir.y = 0.0f;
                if (glm::length(moveDir) > 0.0f) moveDir = glm::normalize(moveDir);

                float speed = 10.0f;
                if (window().isKeyPressed(GLFW_KEY_LEFT_SHIFT)) speed *= 2.0f;

                if (registry().has<CharacterController>(gameCam)) {
                    auto& cc = registry().get<CharacterController>(gameCam);
                    if (!cc.pxController && m_phys) {
                        cc.pxController = m_phys->CreateCharacter(tr.position, cc.radius, cc.height);
                        cc.velocity = glm::vec3(0.0f);
                        cc.onGround = false;
                    }

                    glm::vec3 disp = moveDir * cc.speed * dt;
                    cc.velocity.y -= 9.81f * dt;
                    if (cc.onGround && window().isKeyPressed(GLFW_KEY_SPACE)) {
                        cc.velocity.y = cc.jumpForce;
                    }
                    disp.y += cc.velocity.y * dt;

                    glm::vec3 newPos = tr.position;
                    if (cc.pxController && m_phys) {
                        cc.onGround = m_phys->MoveCharacter(cc.pxController, disp, dt, newPos);
                        if (cc.onGround) cc.velocity.y = 0.0f;
                    } else {
                        newPos += disp;
                    }
                    tr.position = newPos;
                } else {
                    tr.position += moveDir * speed * dt;
                    if (window().isKeyPressed(GLFW_KEY_SPACE) || window().isKeyPressed(GLFW_KEY_E)) tr.position.y += speed * dt;
                    if (window().isKeyPressed(GLFW_KEY_LEFT_CONTROL) || window().isKeyPressed(GLFW_KEY_Q)) tr.position.y -= speed * dt;
                }
            }
            return;
        }
    }

    // =========================================================================
    // EDITOR CAMERA NAVIGATION
    // =========================================================================
    if (Input::IsKeyJustPressed(GLFW_KEY_TAB)) {
        m_showUI = !m_showUI;
    }

    // Toggle continuous flycam with 'C' key
    if (Input::IsKeyJustPressed(GLFW_KEY_C) && !io.WantTextInput) {
        m_flycamActive = !m_flycamActive;
        window().setCursorMode(m_flycamActive ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    // 1. Right Mouse Button Look
    bool rmb = window().isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    static bool wasRmb = false;
    static double lastRmbX = 0, lastRmbY = 0;

    if (rmb) {
        double curX, curY;
        window().getCursorPos(curX, curY);
        if (!wasRmb) {
            wasRmb = true;
            lastRmbX = curX;
            lastRmbY = curY;
            window().setCursorMode(GLFW_CURSOR_DISABLED);
        } else {
            float dx = static_cast<float>(curX - lastRmbX);
            float dy = static_cast<float>(curY - lastRmbY);
            lastRmbX = curX;
            lastRmbY = curY;

            m_camYaw   += dx * 0.15f;
            m_camPitch -= dy * 0.15f * (m_invertY ? -1.0f : 1.0f);
            m_camPitch = glm::clamp(m_camPitch, -89.0f, 89.0f);
        }
    } else {
        if (wasRmb) {
            wasRmb = false;
            if (!m_flycamActive) {
                window().setCursorMode(GLFW_CURSOR_NORMAL);
            }
        }
    }

    // 2. Continuous flycam (C key) active
    if (m_flycamActive && !rmb) {
        static double lastFlyX = 0, lastFlyY = 0;
        static bool firstFly = true;
        double curX, curY;
        window().getCursorPos(curX, curY);
        if (firstFly) {
            lastFlyX = curX; lastFlyY = curY; firstFly = false;
        } else {
            float dx = static_cast<float>(curX - lastFlyX);
            float dy = static_cast<float>(curY - lastFlyY);
            lastFlyX = curX; lastFlyY = curY;

            m_camYaw   += dx * 0.15f;
            m_camPitch -= dy * 0.15f * (m_invertY ? -1.0f : 1.0f);
            m_camPitch = glm::clamp(m_camPitch, -89.0f, 89.0f);
        }
    }

    // 3. Left Mouse Button in Scene (Drag to Rotate, or Alt+LMB to Orbit)
    bool lmb = window().isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    bool altHeld = window().isKeyPressed(GLFW_KEY_LEFT_ALT) || window().isKeyPressed(GLFW_KEY_RIGHT_ALT);
    static bool wasLmb = false;
    static double lastLmbX = 0, lastLmbY = 0;

    if (lmb && !rmb && !m_flycamActive && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && !io.WantCaptureMouse) {
        double curX, curY;
        window().getCursorPos(curX, curY);
        if (!wasLmb) {
            wasLmb = true;
            lastLmbX = curX;
            lastLmbY = curY;
        } else {
            float dx = static_cast<float>(curX - lastLmbX);
            float dy = static_cast<float>(curY - lastLmbY);
            lastLmbX = curX;
            lastLmbY = curY;

            if (altHeld) {
                glm::vec3 pivot{0.0f};
                if (registry().valid(m_selectedEntity) && registry().has<Transform>(m_selectedEntity)) {
                    pivot = registry().get<Transform>(m_selectedEntity).position;
                }
                float dist = glm::distance(m_camPos, pivot);
                if (dist < 0.5f) dist = 5.0f;

                m_camYaw   += dx * 0.25f;
                m_camPitch -= dy * 0.25f;
                m_camPitch = glm::clamp(m_camPitch, -89.0f, 89.0f);

                float newYawRad = glm::radians(m_camYaw);
                float newPitchRad = glm::radians(m_camPitch);
                glm::vec3 newFront{
                    std::cos(newYawRad) * std::cos(newPitchRad),
                    std::sin(newPitchRad),
                    std::sin(newYawRad) * std::cos(newPitchRad)
                };
                m_camPos = pivot - glm::normalize(newFront) * dist;
            } else {
                m_camYaw   += dx * 0.15f;
                m_camPitch -= dy * 0.15f;
                m_camPitch = glm::clamp(m_camPitch, -89.0f, 89.0f);
            }
        }
    } else {
        wasLmb = false;
    }

    // 4. Middle Mouse Button (MMB) Pan
    bool mmb = window().isMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE);
    static bool wasMmb = false;
    static double lastMmbX = 0, lastMmbY = 0;
    if (mmb) {
        double curX, curY;
        window().getCursorPos(curX, curY);
        if (!wasMmb) {
            wasMmb = true;
            lastMmbX = curX;
            lastMmbY = curY;
        } else {
            float dx = static_cast<float>(curX - lastMmbX);
            float dy = static_cast<float>(curY - lastMmbY);
            lastMmbX = curX;
            lastMmbY = curY;

            float yawRad = glm::radians(m_camYaw);
            float pitchRad = glm::radians(m_camPitch);
            glm::vec3 fwd{
                std::cos(yawRad) * std::cos(pitchRad),
                std::sin(pitchRad),
                std::sin(yawRad) * std::cos(pitchRad)
            };
            fwd = glm::normalize(fwd);
            glm::vec3 r = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
            glm::vec3 u = glm::vec3(0, 1, 0);

            m_camPos -= r * (dx * 0.025f);
            m_camPos += u * (dy * 0.025f);
        }
    } else {
        wasMmb = false;
    }

    // Direction vectors
    float yawRad = glm::radians(m_camYaw);
    float pitchRad = glm::radians(m_camPitch);
    glm::vec3 front{
        std::cos(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        std::sin(yawRad) * std::cos(pitchRad)
    };
    front = glm::normalize(front);
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
    glm::vec3 up = glm::vec3(0, 1, 0);

    // 5. Mouse Wheel Zoom
    if (io.MouseWheel != 0.0f && (!io.WantCaptureMouse || rmb || m_flycamActive)) {
        if (rmb || m_flycamActive) {
            m_camSpeed = glm::clamp(m_camSpeed + io.MouseWheel * 3.0f, 2.0f, 150.0f);
        } else {
            m_camPos += front * (io.MouseWheel * 3.0f);
        }
    }

    // 6. WASD Movement (ALWAYS ACTIVE unless typing in an ImGui text input box)
    if (!io.WantTextInput) {
        float speed = m_camSpeed;
        if (window().isKeyPressed(GLFW_KEY_LEFT_SHIFT)) speed *= 2.5f;
        if (window().isKeyPressed(GLFW_KEY_LEFT_CONTROL)) speed *= 0.3f;

        if (window().isKeyPressed(GLFW_KEY_W) || window().isKeyPressed(GLFW_KEY_UP)) m_camPos += front * speed * dt;
        if (window().isKeyPressed(GLFW_KEY_S) || window().isKeyPressed(GLFW_KEY_DOWN)) m_camPos -= front * speed * dt;
        if (window().isKeyPressed(GLFW_KEY_D) || window().isKeyPressed(GLFW_KEY_RIGHT)) m_camPos += right * speed * dt;
        if (window().isKeyPressed(GLFW_KEY_A) || window().isKeyPressed(GLFW_KEY_LEFT)) m_camPos -= right * speed * dt;
        if (window().isKeyPressed(GLFW_KEY_E) || window().isKeyPressed(GLFW_KEY_SPACE)) m_camPos += up * speed * dt;
        if (window().isKeyPressed(GLFW_KEY_Q)) m_camPos -= up * speed * dt;
    }

    // Focus Camera on Selected Entity with Key F
    if (Input::IsKeyJustPressed(GLFW_KEY_F) && registry().valid(m_selectedEntity) && registry().has<Transform>(m_selectedEntity)) {
        auto& tr = registry().get<Transform>(m_selectedEntity);
        m_camPos = tr.position - front * 6.0f + glm::vec3(0, 2, 0);
    }

    // Duplicate selected with Ctrl+D
    if (window().isKeyPressed(GLFW_KEY_LEFT_CONTROL) && Input::IsKeyJustPressed(GLFW_KEY_D)) {
        if (registry().valid(m_selectedEntity)) {
            duplicateEntity(m_selectedEntity);
        }
    }

    // Object picking via Left Mouse Click (only when not dragging)
    if (Input::IsMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT) && !altHeld && !rmb && !m_flycamActive) {
        if (!io.WantCaptureMouse && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
            pickObjectUnderMouse();
        }
    }

    // Window Toggle Shortcuts (Alt+1 .. Alt+8)
    if (window().isKeyPressed(GLFW_KEY_LEFT_ALT) || window().isKeyPressed(GLFW_KEY_RIGHT_ALT)) {
        if (Input::IsKeyJustPressed(GLFW_KEY_1)) m_showHierarchy = !m_showHierarchy;
        if (Input::IsKeyJustPressed(GLFW_KEY_2)) m_showInspector = !m_showInspector;
        if (Input::IsKeyJustPressed(GLFW_KEY_3)) m_showAssetBrowser = !m_showAssetBrowser;
        if (Input::IsKeyJustPressed(GLFW_KEY_4)) m_showAtmosphereEditor = !m_showAtmosphereEditor;
        if (Input::IsKeyJustPressed(GLFW_KEY_5)) m_showGraphicsSettings = !m_showGraphicsSettings;
        if (Input::IsKeyJustPressed(GLFW_KEY_6)) m_showClusterLODSettings = !m_showClusterLODSettings;
        if (Input::IsKeyJustPressed(GLFW_KEY_7)) m_showStats = !m_showStats;
        if (Input::IsKeyJustPressed(GLFW_KEY_8)) m_showCameraPreview = !m_showCameraPreview;
    }

    // Keyboard Shortcuts for Gizmo (1, 2, 3 without Alt)
    if (!m_flycamActive && !rmb && !altHeld && !io.WantTextInput) {
        if (Input::IsKeyJustPressed(GLFW_KEY_1)) m_gizmoOperation = 0; // Translate
        if (Input::IsKeyJustPressed(GLFW_KEY_2)) m_gizmoOperation = 1; // Rotate
        if (Input::IsKeyJustPressed(GLFW_KEY_3)) m_gizmoOperation = 2; // Scale
        if (Input::IsKeyJustPressed(GLFW_KEY_DELETE) && registry().valid(m_selectedEntity)) {
            scene().destroy(m_selectedEntity);
            m_selectedEntity = NullEntity;
        }
    }
}

void SceneEditor::onUpdate(float dt) {
    if (m_isPlaying && m_phys) {
        m_phys->Step(dt);
        m_phys->SyncToECS(registry());
    }

    updateCamera(dt);
    updateScripts(dt);

    int w, h;
    window().getFramebufferSize(w, h);
    if (!m_pipe) m_pipe = std::make_unique<RenderPipeline>(w, h);
    m_pipe->resize(w, h);
    m_pipe->syncFromRegistry(registry());

    float aspect = float(w) / float(h ? h : 1);
    glm::mat4 viewMatrix(1.0f);
    glm::mat4 projMatrix(1.0f);
    glm::vec3 viewPos{0.0f};

    Entity activeCam = NullEntity;
    if (m_isPlaying) {
        for (Entity ent : registry().view<Camera, Transform>()) {
            if (registry().get<Camera>(ent).primary) {
                activeCam = ent;
                break;
            }
            if (activeCam == NullEntity) activeCam = ent;
        }
    }

    if (m_isPlaying && registry().valid(activeCam) && registry().has<Transform>(activeCam)) {
        auto& camTr = registry().get<Transform>(activeCam);
        auto& camComp = registry().get<Camera>(activeCam);
        viewMatrix = Camera::viewFromTransform(camTr);
        projMatrix = camComp.projection(aspect);
        viewPos = camTr.position;
    } else {
        // Editor Freefly Camera
        float yawRad = glm::radians(m_camYaw);
        float pitchRad = glm::radians(m_camPitch);
        glm::vec3 front{
            std::cos(yawRad) * std::cos(pitchRad),
            std::sin(pitchRad),
            std::sin(yawRad) * std::cos(pitchRad)
        };
        front = glm::normalize(front);
        viewMatrix = glm::lookAt(m_camPos, m_camPos + front, glm::vec3(0, 1, 0));
        projMatrix = glm::perspective(glm::radians(m_camFov), aspect, 0.1f, 2000.0f);
        viewPos = m_camPos;
    }

    m_pipe->setCameraMatrices(viewMatrix, projMatrix);

    m_pipe->beginFrame();
    Systems::RenderWithCamera(registry(), *m_litShader, window(), viewMatrix, projMatrix, viewPos, m_pipe->prevViewProj());
    m_pipe->endFrame();
}

void SceneEditor::onImGuiRender() {
    if (!m_showUI) return;

    if (m_isPlaying) {
        // Pure game preview mode: hide all editor windows & gizmos, show minimal HUD
        renderPlayModeOverlay();
        return;
    }

    renderMenuBar();
    renderGizmo();
    renderSceneCameraGizmos();
    if (m_showColliders) renderColliderGizmos();

    if (m_showHierarchy) renderHierarchy();
    if (m_showInspector) renderInspector();
    if (m_showAssetBrowser) renderAssetBrowser();
    if (m_showMaterialPalette) renderMaterialPalette();
    if (m_showAtmosphereEditor) renderAtmosphereEditor();
    if (m_showGraphicsSettings) renderGraphicsSettings();
    if (m_showClusterLODSettings) renderClusterLODSettings();
    if (m_showStats) renderStats();
    if (m_showCameraPreview) renderCameraPreview();
    if (m_showBuildModal) renderBuildModal();
    if (m_showDeleteAssetModal) renderDeleteAssetModal();
    if (m_showCreateAssetModal) renderCreateAssetModal();
    if (m_showRenameAssetModal) renderRenameAssetModal();
}

void SceneEditor::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) newScene();
            if (ImGui::MenuItem("Load Default Showcase")) loadDefaultShowcase();
            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) saveSceneToFile(m_sceneFileBuf);
            if (ImGui::MenuItem("Load Scene", "Ctrl+O")) loadSceneFromFile(m_sceneFileBuf);
            ImGui::Separator();
            if (ImGui::MenuItem("New PBR Material...")) {
                m_createAssetType = 0;
                strncpy(m_createAssetNameBuf, "NewPBRMaterial", sizeof(m_createAssetNameBuf));
                m_showCreateAssetModal = true;
            }
            if (ImGui::MenuItem("New C++ Script Template...")) {
                m_createAssetType = 3;
                strncpy(m_createAssetNameBuf, "CustomGameScript", sizeof(m_createAssetNameBuf));
                m_showCreateAssetModal = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Build Standalone Game Executable...", "F7")) buildStandaloneGame();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) close();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scenes")) {
            refreshAvailableScenes();
            for (const auto& scPath : m_availableScenes) {
                bool isCurrent = (m_sceneFileBuf == scPath);
                if (ImGui::MenuItem(scPath.c_str(), nullptr, isCurrent)) {
                    loadSceneFromFile(scPath);
                    strncpy(m_sceneFileBuf, scPath.c_str(), sizeof(m_sceneFileBuf));
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Refresh Scene List")) refreshAvailableScenes();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Layouts")) {
            if (ImGui::MenuItem("Studio Default")) applyLayoutPreset(0);
            if (ImGui::MenuItem("Level Design & World Building")) applyLayoutPreset(1);
            if (ImGui::MenuItem("Material & Shading Artist")) applyLayoutPreset(2);
            if (ImGui::MenuItem("Game Testing / Minimal HUD")) applyLayoutPreset(3);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset to Default Layout")) applyLayoutPreset(0);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Gizmo")) {
            if (ImGui::MenuItem("Translate", "W", m_gizmoOperation == 0)) m_gizmoOperation = 0;
            if (ImGui::MenuItem("Rotate", "E", m_gizmoOperation == 1)) m_gizmoOperation = 1;
            if (ImGui::MenuItem("Scale", "R", m_gizmoOperation == 2)) m_gizmoOperation = 2;
            ImGui::Separator();
            if (ImGui::MenuItem("Local Space", nullptr, m_gizmoMode == 0)) m_gizmoMode = 0;
            if (ImGui::MenuItem("World Space", nullptr, m_gizmoMode == 1)) m_gizmoMode = 1;
            ImGui::Separator();
            if (ImGui::MenuItem("Grid Snapping", nullptr, &m_useSnap)) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Hierarchy", "Alt+1", &m_showHierarchy);
            ImGui::MenuItem("Inspector", "Alt+2", &m_showInspector);
            ImGui::MenuItem("Asset Browser", "Alt+3", &m_showAssetBrowser);
            ImGui::MenuItem("Material Palette / PBR Library", "Alt+9", &m_showMaterialPalette);
            ImGui::MenuItem("Atmosphere & Sky", "Alt+4", &m_showAtmosphereEditor);
            ImGui::MenuItem("Graphics & RTX / GI Settings", "Alt+5", &m_showGraphicsSettings);
            ImGui::MenuItem("ClusterLOD / Nanite Settings", "Alt+6", &m_showClusterLODSettings);
            ImGui::MenuItem("Performance Metrics", "Alt+7", &m_showStats);
            ImGui::MenuItem("Game Camera Preview", "Alt+8", &m_showCameraPreview);
            ImGui::Separator();
            if (ImGui::MenuItem("Show Default Panels")) applyLayoutPreset(0);
            if (ImGui::MenuItem("Show All Windows")) applyLayoutPreset(1);
            if (ImGui::MenuItem("Hide All Windows")) applyLayoutPreset(3);
            ImGui::Separator();
            ImGui::MenuItem("Show 3D Colliders (Translucent)", nullptr, &m_showColliders);
            ImGui::MenuItem("Toggle Full Editor UI", "Tab", &m_showUI);
            ImGui::MenuItem("Invert Mouse Y-Axis", nullptr, &m_invertY);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Build")) {
            if (ImGui::MenuItem("Build Standalone Game (Release)", "F7")) {
                buildStandaloneGame();
            }
            ImGui::EndMenu();
        }

        // Center Play / Simulation Toolbar Button
        ImGui::SameLine(ImGui::GetWindowWidth() * 0.5f - 160.0f);
        if (!m_isPlaying) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.25f, 1.0f));
            if (ImGui::Button(" [>] Play Game ", ImVec2(140, 0))) {
                saveSceneToFile("assets/.play_mode_backup.json");
                m_isPlaying = true;
                if (m_phys) {
                    m_phys->BuildFromECS(registry());
                    for (Entity e : registry().view<CharacterController, Transform>()) {
                        auto& cc = registry().get<CharacterController>(e);
                        auto& tr = registry().get<Transform>(e);
                        cc.pxController = m_phys->CreateCharacter(tr.position, cc.radius, cc.height);
                        cc.velocity = glm::vec3(0.0f);
                        cc.onGround = false;
                    }
                }
                // Initialize all scripts on play
                for (Entity e : registry().view<NativeScript>()) {
                    auto& ns = registry().get<NativeScript>(e);
                    if (ns.instantiate && !ns.instance) {
                        ns.instance = ns.instantiate();
                        ns.instance->_init(e, &registry());
                        ns.instance->onCreate();
                        ns.instance->onStart();
                    }
                }
                std::cout << "[SceneEditor] Started Play Mode (Game View Active)\n";
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.22f, 0.2f, 1.0f));
            if (ImGui::Button(" [x] Stop Game ", ImVec2(140, 0))) {
                m_isPlaying = false;
                loadSceneFromFile("assets/.play_mode_backup.json");
                window().setCursorMode(GLFW_CURSOR_NORMAL);
                std::cout << "[SceneEditor] Stopped Play Mode, scene restored\n";
            }
            ImGui::PopStyleColor();
        }

        // Build Standalone Game Toolbar Button
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.85f, 1.0f));
        if (ImGui::Button(" [ Build Standalone ] ", ImVec2(150, 0))) {
            buildStandaloneGame();
        }
        ImGui::PopStyleColor();

        // Flycam Toggle Toolbar Button
        ImGui::SameLine();
        if (m_flycamActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.5f, 0.1f, 1.0f));
            if (ImGui::Button(" [📹 Flycam: ON (C)] ", ImVec2(155, 0))) {
                m_flycamActive = false;
                window().setCursorMode(GLFW_CURSOR_NORMAL);
            }
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button(" [📹 Flycam: OFF (C)] ", ImVec2(155, 0))) {
                m_flycamActive = true;
                window().setCursorMode(GLFW_CURSOR_DISABLED);
            }
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 340.0f);
        ImGui::TextDisabled("Click Mesh to Select | [RMB] Fly | [Tab] UI");
        ImGui::EndMainMenuBar();
    }
}

void SceneEditor::renderGizmo() {
    if (!registry().valid(m_selectedEntity) || !registry().has<Transform>(m_selectedEntity)) return;
    if (m_flycamActive) return;

    int w, h; window().getFramebufferSize(w, h);
    float aspect = float(w) / float(h ? h : 1);

    float yawRad = glm::radians(m_camYaw);
    float pitchRad = glm::radians(m_camPitch);
    glm::vec3 front{
        std::cos(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        std::sin(yawRad) * std::cos(pitchRad)
    };
    front = glm::normalize(front);
    glm::mat4 view = glm::lookAt(m_camPos, m_camPos + front, glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(m_camFov), aspect, 0.1f, 2000.0f);

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Enable(true);

    auto& tr = registry().get<Transform>(m_selectedEntity);
    glm::mat4 model = tr.matrix();

    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    if (m_gizmoOperation == 1) op = ImGuizmo::ROTATE;
    else if (m_gizmoOperation == 2) op = ImGuizmo::SCALE;

    ImGuizmo::MODE mode = (m_gizmoMode == 0) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    float* snap = m_useSnap ? m_snapValue : nullptr;
    if (m_gizmoOperation == 1 && m_useSnap) {
        static float rotSnap = 15.0f;
        snap = &rotSnap;
    }

    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), op, mode, glm::value_ptr(model), nullptr, snap)) {
        float matrixTranslation[3], matrixRotation[3], matrixScale[3];
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), matrixTranslation, matrixRotation, matrixScale);
        glm::vec3 oldPos = tr.position;
        glm::vec3 oldRot = tr.rotation;
        tr.position = glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]);
        tr.rotation = glm::vec3(matrixRotation[0], matrixRotation[1], matrixRotation[2]);
        tr.scale    = glm::vec3(matrixScale[0], matrixScale[1], matrixScale[2]);

        propagateTransformDeltaToChildren(m_selectedEntity, tr.position - oldPos, tr.rotation - oldRot);
        syncTransformToPhysics(m_selectedEntity);
    }
}

void SceneEditor::renderPlayModeOverlay() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - 190.0f, 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(380.0f, 44.0f), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.14f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.75f, 0.4f, 0.9f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

    if (ImGui::Begin("##PlayOverlay", nullptr, flags)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[GAME VIEW]");
        ImGui::SameLine();
        ImGui::TextDisabled("| FPS: %.0f", io.Framerate);
        ImGui::SameLine(ImGui::GetWindowWidth() - 165.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.18f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button("Stop Game (ESC)", ImVec2(150, 26))) {
            m_isPlaying = false;
            loadSceneFromFile("assets/.play_mode_backup.json");
            window().setCursorMode(GLFW_CURSOR_NORMAL);
            std::cout << "[SceneEditor] Stopped Play Mode, scene restored\n";
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void SceneEditor::renderSceneCameraGizmos() {
    if (m_isPlaying) return;

    int w, h; window().getFramebufferSize(w, h);
    float aspect = float(w) / float(h ? h : 1);

    float yawRad = glm::radians(m_camYaw);
    float pitchRad = glm::radians(m_camPitch);
    glm::vec3 front{
        std::cos(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        std::sin(yawRad) * std::cos(pitchRad)
    };
    front = glm::normalize(front);
    glm::mat4 view = glm::lookAt(m_camPos, m_camPos + front, glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(m_camFov), aspect, 0.1f, 2000.0f);
    glm::mat4 vp = proj * view;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    auto worldToScreen = [&](const glm::vec3& p, ImVec2& outPt) -> bool {
        glm::vec4 clip = vp * glm::vec4(p, 1.0f);
        if (clip.w <= 0.05f) return false;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
        outPt.x = (ndc.x * 0.5f + 0.5f) * io.DisplaySize.x;
        outPt.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * io.DisplaySize.y;
        return true;
    };

    for (Entity e : registry().view<Camera, Transform>()) {
        auto& tr = registry().get<Transform>(e);
        auto& cam = registry().get<Camera>(e);
        bool isSelected = (m_selectedEntity == e);

        ImVec2 screenPos;
        if (worldToScreen(tr.position, screenPos)) {
            float sz = 16.0f;
            ImU32 iconColor = isSelected ? IM_COL32(60, 210, 255, 255) : IM_COL32(235, 235, 235, 230);
            ImU32 bgColor = isSelected ? IM_COL32(20, 50, 75, 240) : IM_COL32(30, 34, 42, 210);

            // Rounded icon badge
            drawList->AddRectFilled(
                ImVec2(screenPos.x - sz - 4, screenPos.y - sz - 4),
                ImVec2(screenPos.x + sz + 4, screenPos.y + sz + 4),
                bgColor, 6.0f);
            drawList->AddRect(
                ImVec2(screenPos.x - sz - 4, screenPos.y - sz - 4),
                ImVec2(screenPos.x + sz + 4, screenPos.y + sz + 4),
                iconColor, 6.0f, 0, isSelected ? 2.5f : 1.5f);

            // Billboard Camera Glyph
            // Camera body
            drawList->AddRectFilled(
                ImVec2(screenPos.x - 9, screenPos.y - 6),
                ImVec2(screenPos.x + 3, screenPos.y + 6),
                iconColor, 2.0f);
            // Camera Top Reels
            drawList->AddCircleFilled(ImVec2(screenPos.x - 5, screenPos.y - 8), 2.5f, iconColor);
            drawList->AddCircleFilled(ImVec2(screenPos.x - 1, screenPos.y - 8), 2.5f, iconColor);
            // Camera lens cone
            drawList->AddTriangleFilled(
                ImVec2(screenPos.x + 3, screenPos.y - 2),
                ImVec2(screenPos.x + 9, screenPos.y - 6),
                ImVec2(screenPos.x + 9, screenPos.y + 6),
                iconColor);
            drawList->AddTriangleFilled(
                ImVec2(screenPos.x + 3, screenPos.y - 2),
                ImVec2(screenPos.x + 9, screenPos.y + 6),
                ImVec2(screenPos.x + 3, screenPos.y + 2),
                iconColor);

            // Label
            std::string label = registry().has<Name>(e) ? registry().get<Name>(e).value : "Camera";
            if (cam.primary) label += " (Primary)";
            drawList->AddText(ImVec2(screenPos.x - sz - 2, screenPos.y + sz + 5), iconColor, label.c_str());

            // Click to select
            if (!m_flycamActive && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive()) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ImVec2 mouse = io.MousePos;
                    float dx = mouse.x - screenPos.x;
                    float dy = mouse.y - screenPos.y;
                    if (dx * dx + dy * dy <= (sz + 12) * (sz + 12)) {
                        m_selectedEntity = e;
                    }
                }
            }
        }

        // Draw 3D Camera Frustum Wireframe when selected
        if (isSelected) {
            float fovY = glm::radians(cam.fov);
            float fovX = 2.0f * std::atan(std::tan(fovY * 0.5f) * aspect);
            float d = 4.5f;
            float hHalf = std::tan(fovY * 0.5f) * d;
            float wHalf = std::tan(fovX * 0.5f) * d;

            glm::mat4 m = tr.matrix();
            glm::vec3 cOrigin = tr.position;
            glm::vec4 localTL(-wHalf,  hHalf, d, 1.0f);
            glm::vec4 localTR( wHalf,  hHalf, d, 1.0f);
            glm::vec4 localBR( wHalf, -hHalf, d, 1.0f);
            glm::vec4 localBL(-wHalf, -hHalf, d, 1.0f);

            glm::vec3 wTL = glm::vec3(m * localTL);
            glm::vec3 wTR = glm::vec3(m * localTR);
            glm::vec3 wBR = glm::vec3(m * localBR);
            glm::vec3 wBL = glm::vec3(m * localBL);

            ImVec2 sOrigin, sTL, sTR, sBR, sBL;
            bool okO = worldToScreen(cOrigin, sOrigin);
            bool okTL = worldToScreen(wTL, sTL);
            bool okTR = worldToScreen(wTR, sTR);
            bool okBR = worldToScreen(wBR, sBR);
            bool okBL = worldToScreen(wBL, sBL);

            ImU32 frustumColor = IM_COL32(40, 220, 255, 220);
            if (okO && okTL) drawList->AddLine(sOrigin, sTL, frustumColor, 1.5f);
            if (okO && okTR) drawList->AddLine(sOrigin, sTR, frustumColor, 1.5f);
            if (okO && okBR) drawList->AddLine(sOrigin, sBR, frustumColor, 1.5f);
            if (okO && okBL) drawList->AddLine(sOrigin, sBL, frustumColor, 1.5f);

            if (okTL && okTR) drawList->AddLine(sTL, sTR, frustumColor, 2.0f);
            if (okTR && okBR) drawList->AddLine(sTR, sBR, frustumColor, 2.0f);
            if (okBR && okBL) drawList->AddLine(sBR, sBL, frustumColor, 2.0f);
            if (okBL && okTL) drawList->AddLine(sBL, sTL, frustumColor, 2.0f);
        }
    }
}

void SceneEditor::propagateTransformDeltaToChildren(Entity parent, const glm::vec3& deltaPos, const glm::vec3& deltaRot) {
    if (!registry().valid(parent)) return;
    for (Entity child : registry().view<Transform, Hierarchy>()) {
        if (registry().get<Hierarchy>(child).parent == parent) {
            auto& tr = registry().get<Transform>(child);
            tr.position += deltaPos;
            tr.rotation += deltaRot;
            syncTransformToPhysics(child);
            propagateTransformDeltaToChildren(child, deltaPos, deltaRot);
        }
    }
}

void SceneEditor::syncTransformToPhysics(Entity e) {
    if (!m_phys || !registry().valid(e) || !registry().has<Transform>(e)) return;
    auto& tr = registry().get<Transform>(e);

    if (registry().has<cjoka_phys::Rigidbody>(e)) {
        auto& rb = registry().get<cjoka_phys::Rigidbody>(e);
        if (rb.pxActor) {
            m_phys->SetActorPose(rb.pxActor, tr.position, tr.rotation);
        }
    }

    if (registry().has<CharacterController>(e)) {
        auto& cc = registry().get<CharacterController>(e);
        if (cc.pxController) {
            m_phys->SetCharacterPosition(cc.pxController, tr.position);
        }
    }
}

void SceneEditor::renderColliderGizmos() {
    int fbw, fbh; window().getFramebufferSize(fbw, fbh);
    float aspect = float(fbw) / float(fbh ? fbh : 1);
    float yawRad = glm::radians(m_camYaw);
    float pitchRad = glm::radians(m_camPitch);
    glm::vec3 front{
        std::cos(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        std::sin(yawRad) * std::cos(pitchRad)
    };
    front = glm::normalize(front);
    glm::mat4 view = glm::lookAt(m_camPos, m_camPos + front, glm::vec3(0, 1, 0));
    glm::mat4 proj = glm::perspective(glm::radians(m_camFov), aspect, 0.1f, 2000.0f);

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    auto worldToScreen = [&](const glm::vec3& p, ImVec2& outPt) -> bool {
        glm::vec4 clip = proj * view * glm::vec4(p, 1.0f);
        if (clip.w <= 0.05f) return false;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
        outPt.x = (ndc.x * 0.5f + 0.5f) * io.DisplaySize.x;
        outPt.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * io.DisplaySize.y;
        return true;
    };

    // Draw parent-child connection lines (golden dotted/solid lines)
    for (Entity e : registry().view<Transform, Hierarchy>()) {
        Entity parent = registry().get<Hierarchy>(e).parent;
        if (registry().valid(parent) && registry().has<Transform>(parent)) {
            ImVec2 p0, p1;
            if (worldToScreen(registry().get<Transform>(parent).position, p0) &&
                worldToScreen(registry().get<Transform>(e).position, p1)) {
                drawList->AddLine(p0, p1, IM_COL32(255, 215, 0, 180), 1.5f);
            }
        }
    }

    // Iterate over entities with colliders or character controllers
    for (Entity e : registry().view<Transform>()) {
        bool hasCol = registry().has<cjoka_phys::Collider>(e);
        bool hasCC = registry().has<CharacterController>(e);
        if (!hasCol && !hasCC) continue;

        bool isSelected = (m_selectedEntity == e);
        ImU32 fillCol = isSelected ? IM_COL32(40, 255, 120, 50) : IM_COL32(30, 220, 180, 25);
        ImU32 wireCol = isSelected ? IM_COL32(60, 255, 140, 230) : IM_COL32(40, 220, 180, 130);
        float lineThick = isSelected ? 2.0f : 1.0f;

        auto& tr = registry().get<Transform>(e);
        glm::mat4 m(1.0f);
        m = glm::translate(m, tr.position);
        m = glm::rotate(m, glm::radians(tr.rotation.z), glm::vec3(0,0,1));
        m = glm::rotate(m, glm::radians(tr.rotation.y), glm::vec3(0,1,0));
        m = glm::rotate(m, glm::radians(tr.rotation.x), glm::vec3(1,0,0));

        if (hasCol) {
            auto& col = registry().get<cjoka_phys::Collider>(e);
            if (col.type == cjoka_phys::ColliderType::Box) {
                glm::vec3 h = col.halfExtents * tr.scale;
                glm::vec3 c = col.centerOffset;
                glm::vec3 pts[8] = {
                    glm::vec3(m * glm::vec4(c + glm::vec3(-h.x, -h.y, -h.z), 1.0f)),
                    glm::vec3(m * glm::vec4(c + glm::vec3( h.x, -h.y, -h.z), 1.0f)),
                    glm::vec3(m * glm::vec4(c + glm::vec3( h.x,  h.y, -h.z), 1.0f)),
                    glm::vec3(m * glm::vec4(c + glm::vec3(-h.x,  h.y, -h.z), 1.0f)),
                    glm::vec3(m * glm::vec4(c + glm::vec3(-h.x, -h.y,  h.z), 1.0f)),
                    glm::vec3(m * glm::vec4(c + glm::vec3( h.x, -h.y,  h.z), 1.0f)),
                    glm::vec3(m * glm::vec4(c + glm::vec3( h.x,  h.y,  h.z), 1.0f)),
                    glm::vec3(m * glm::vec4(c + glm::vec3(-h.x,  h.y,  h.z), 1.0f))
                };
                ImVec2 s[8];
                bool allVis = true;
                for (int i = 0; i < 8; ++i) {
                    if (!worldToScreen(pts[i], s[i])) { allVis = false; break; }
                }
                if (allVis) {
                    drawList->AddQuadFilled(s[4], s[5], s[6], s[7], fillCol);
                    drawList->AddQuadFilled(s[0], s[1], s[2], s[3], fillCol);
                    drawList->AddQuadFilled(s[0], s[4], s[7], s[3], fillCol);
                    drawList->AddQuadFilled(s[1], s[5], s[6], s[2], fillCol);
                    drawList->AddQuadFilled(s[3], s[2], s[6], s[7], fillCol);
                    drawList->AddQuadFilled(s[0], s[1], s[5], s[4], fillCol);

                    drawList->AddLine(s[0], s[1], wireCol, lineThick);
                    drawList->AddLine(s[1], s[2], wireCol, lineThick);
                    drawList->AddLine(s[2], s[3], wireCol, lineThick);
                    drawList->AddLine(s[3], s[0], wireCol, lineThick);

                    drawList->AddLine(s[4], s[5], wireCol, lineThick);
                    drawList->AddLine(s[5], s[6], wireCol, lineThick);
                    drawList->AddLine(s[6], s[7], wireCol, lineThick);
                    drawList->AddLine(s[7], s[4], wireCol, lineThick);

                    drawList->AddLine(s[0], s[4], wireCol, lineThick);
                    drawList->AddLine(s[1], s[5], wireCol, lineThick);
                    drawList->AddLine(s[2], s[6], wireCol, lineThick);
                    drawList->AddLine(s[3], s[7], wireCol, lineThick);
                }
            } else if (col.type == cjoka_phys::ColliderType::Sphere) {
                glm::vec3 cPos = glm::vec3(m * glm::vec4(col.centerOffset, 1.0f));
                float r = col.radius * std::max({tr.scale.x, tr.scale.y, tr.scale.z});
                const int numSegs = 24;
                ImVec2 ringXZ[numSegs], ringXY[numSegs], ringYZ[numSegs];
                bool okXZ = true, okXY = true, okYZ = true;
                for (int i = 0; i < numSegs; ++i) {
                    float a = (float(i) / float(numSegs)) * glm::two_pi<float>();
                    glm::vec3 pXZ = cPos + glm::vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
                    glm::vec3 pXY = cPos + glm::vec3(std::cos(a) * r, std::sin(a) * r, 0.0f);
                    glm::vec3 pYZ = cPos + glm::vec3(0.0f, std::sin(a) * r, std::cos(a) * r);
                    if (!worldToScreen(pXZ, ringXZ[i])) okXZ = false;
                    if (!worldToScreen(pXY, ringXY[i])) okXY = false;
                    if (!worldToScreen(pYZ, ringYZ[i])) okYZ = false;
                }
                if (okXZ) {
                    for (int i = 0; i < numSegs; ++i) {
                        drawList->AddLine(ringXZ[i], ringXZ[(i+1)%numSegs], wireCol, lineThick);
                    }
                }
                if (okXY) {
                    for (int i = 0; i < numSegs; ++i) {
                        drawList->AddLine(ringXY[i], ringXY[(i+1)%numSegs], wireCol, lineThick);
                    }
                }
                if (okYZ) {
                    for (int i = 0; i < numSegs; ++i) {
                        drawList->AddLine(ringYZ[i], ringYZ[(i+1)%numSegs], wireCol, lineThick);
                    }
                }
                ImVec2 cScreen;
                if (worldToScreen(cPos, cScreen)) {
                    ImVec2 edgeScreen;
                    if (worldToScreen(cPos + front * 0.01f + glm::vec3(r, 0, 0), edgeScreen)) {
                        float sRad = std::hypot(edgeScreen.x - cScreen.x, edgeScreen.y - cScreen.y);
                        if (sRad > 2.0f) {
                            drawList->AddCircleFilled(cScreen, sRad, fillCol, 24);
                        }
                    }
                }
            } else if (col.type == cjoka_phys::ColliderType::Capsule) {
                glm::vec3 cPos = glm::vec3(m * glm::vec4(col.centerOffset, 1.0f));
                float r = col.radius * tr.scale.x;
                float h = col.height * tr.scale.y;
                float halfCyl = std::max(h * 0.5f - r, 0.01f);
                glm::vec3 topC = cPos + glm::vec3(0, halfCyl, 0);
                glm::vec3 botC = cPos - glm::vec3(0, halfCyl, 0);

                const int numSegs = 20;
                ImVec2 ringTop[numSegs], ringBot[numSegs];
                bool okTop = true, okBot = true;
                for (int i = 0; i < numSegs; ++i) {
                    float a = (float(i) / float(numSegs)) * glm::two_pi<float>();
                    glm::vec3 pTop = topC + glm::vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
                    glm::vec3 pBot = botC + glm::vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
                    if (!worldToScreen(pTop, ringTop[i])) okTop = false;
                    if (!worldToScreen(pBot, ringBot[i])) okBot = false;
                }
                if (okTop && okBot) {
                    for (int i = 0; i < numSegs; ++i) {
                        drawList->AddLine(ringTop[i], ringTop[(i+1)%numSegs], wireCol, lineThick);
                        drawList->AddLine(ringBot[i], ringBot[(i+1)%numSegs], wireCol, lineThick);
                    }
                    drawList->AddLine(ringTop[0], ringBot[0], wireCol, lineThick);
                    drawList->AddLine(ringTop[numSegs/4], ringBot[numSegs/4], wireCol, lineThick);
                    drawList->AddLine(ringTop[numSegs/2], ringBot[numSegs/2], wireCol, lineThick);
                    drawList->AddLine(ringTop[3*numSegs/4], ringBot[3*numSegs/4], wireCol, lineThick);

                    drawList->AddQuadFilled(ringTop[0], ringTop[numSegs/2], ringBot[numSegs/2], ringBot[0], fillCol);
                }
            }
        }

        if (hasCC) {
            auto& cc = registry().get<CharacterController>(e);
            glm::vec3 cPos = tr.position;
            float r = cc.radius * tr.scale.x;
            float h = cc.height * tr.scale.y;
            float halfCyl = std::max(h * 0.5f - r, 0.01f);
            glm::vec3 topC = cPos + glm::vec3(0, halfCyl, 0);
            glm::vec3 botC = cPos - glm::vec3(0, halfCyl, 0);

            const int numSegs = 20;
            ImVec2 ringTop[numSegs], ringBot[numSegs];
            bool okTop = true, okBot = true;
            for (int i = 0; i < numSegs; ++i) {
                float a = (float(i) / float(numSegs)) * glm::two_pi<float>();
                glm::vec3 pTop = topC + glm::vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
                glm::vec3 pBot = botC + glm::vec3(std::cos(a) * r, 0.0f, std::sin(a) * r);
                if (!worldToScreen(pTop, ringTop[i])) okTop = false;
                if (!worldToScreen(pBot, ringBot[i])) okBot = false;
            }
            if (okTop && okBot) {
                for (int i = 0; i < numSegs; ++i) {
                    drawList->AddLine(ringTop[i], ringTop[(i+1)%numSegs], wireCol, lineThick);
                    drawList->AddLine(ringBot[i], ringBot[(i+1)%numSegs], wireCol, lineThick);
                }
                drawList->AddLine(ringTop[0], ringBot[0], wireCol, lineThick);
                drawList->AddLine(ringTop[numSegs/4], ringBot[numSegs/4], wireCol, lineThick);
                drawList->AddLine(ringTop[numSegs/2], ringBot[numSegs/2], wireCol, lineThick);
                drawList->AddLine(ringTop[3*numSegs/4], ringBot[3*numSegs/4], wireCol, lineThick);

                drawList->AddQuadFilled(ringTop[0], ringTop[numSegs/2], ringBot[numSegs/2], ringBot[0], fillCol);
            }
        }
    }
}

void SceneEditor::renderHierarchy() {
    ImGui::SetNextWindowPos(ImVec2(16, 36), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 480), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Scene Hierarchy", &m_showHierarchy)) {
        ImGui::InputTextWithHint("##Search", "Search entities...", m_searchBuf, sizeof(m_searchBuf));
        ImGui::Separator();

        if (ImGui::Button("+ Entity", ImVec2(65, 0))) {
            auto ref = scene().create("NewEntity", Transform{m_camPos + glm::vec3(0, 0, 5), {}, {1,1,1}});
            m_selectedEntity = ref.id();
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate", ImVec2(75, 0)) && registry().valid(m_selectedEntity)) {
            duplicateEntity(m_selectedEntity);
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(65, 0)) && registry().valid(m_selectedEntity)) {
            scene().destroy(m_selectedEntity);
            m_selectedEntity = NullEntity;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear", ImVec2(55, 0))) {
            newScene();
        }

        ImGui::Separator();
        ImGui::BeginChild("EntityList");

        std::string filter = m_searchBuf;
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

        auto drawEntityNode = [&](auto self, Entity e) -> void {
            std::string name = "Entity_" + std::to_string((uint32_t)e);
            if (registry().has<Name>(e)) name = registry().get<Name>(e).value;

            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (!filter.empty() && lowerName.find(filter) == std::string::npos) return;

            std::string tag = "[Obj] ";
            if (registry().has<Camera>(e)) tag = "[Cam] ";
            else if (registry().has<PointLight>(e)) tag = "[Light] ";
            else if (registry().has<DirectionalLight>(e)) tag = "[Sun] ";
            else if (registry().has<Sky>(e)) tag = "[Sky] ";
            else if (registry().has<CharacterController>(e)) tag = "[Player] ";
            else if (registry().has<NativeScript>(e)) tag = "[Script] ";

            std::vector<Entity> children;
            for (Entity c : registry().view<Transform, Hierarchy>()) {
                if (registry().get<Hierarchy>(c).parent == e) children.push_back(c);
            }

            bool isSelected = (m_selectedEntity == e);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;
            if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

            bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)e, flags, "%s%s", tag.c_str(), name.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                m_selectedEntity = e;
            }

            // Drag & Drop Source: drag this entity
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &e, sizeof(Entity));
                ImGui::Text("Move %s", name.c_str());
                ImGui::EndDragDropSource();
            }

            // Drag & Drop Target: drop another entity onto this one to make it a child
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                    Entity dropped = *(const Entity*)payload->Data;
                    if (dropped != e && registry().valid(dropped)) {
                        auto& h = registry().has<Hierarchy>(dropped) ? registry().get<Hierarchy>(dropped) : registry().emplace<Hierarchy>(dropped);
                        h.parent = e;
                    }
                }
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH")) {
                    const char* matPath = (const char*)payload->Data;
                    applyMaterialToEntity(e, matPath);
                }
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    const char* assetPath = (const char*)payload->Data;
                    std::string p = assetPath;
                    if (p.find(".mat.json") != std::string::npos) {
                        applyMaterialToEntity(e, p);
                    } else if (p.find(".png") != std::string::npos || p.find(".jpg") != std::string::npos) {
                        if (registry().has<MeshRenderer>(e)) {
                            auto& mr = registry().get<MeshRenderer>(e);
                            mr.material.diffuseMapPath = p;
                            mr.material.diffuseMap = Assets::Texture(p, true);
                            mr.material.useDiffuseMap = (mr.material.diffuseMap && mr.material.diffuseMap->valid());
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (opened) {
                for (Entity c : children) {
                    self(self, c);
                }
                ImGui::TreePop();
            }
        };

        for (Entity e : registry().view<Transform>()) {
            bool hasParent = registry().has<Hierarchy>(e) && registry().valid(registry().get<Hierarchy>(e).parent);
            if (!hasParent) {
                drawEntityNode(drawEntityNode, e);
            }
        }

        // Empty space drag drop target: drop here to detach/unparent
        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 35.0f));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                Entity dropped = *(const Entity*)payload->Data;
                if (registry().valid(dropped) && registry().has<Hierarchy>(dropped)) {
                    registry().remove<Hierarchy>(dropped);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::EndChild();
    }
    ImGui::End();
}

void SceneEditor::renderInspector() {
    ImGui::SetNextWindowPos(ImVec2(1600 - 380, 36), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 680), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Inspector", &m_showInspector)) {
        if (!registry().valid(m_selectedEntity)) {
            ImGui::TextDisabled("Click any object in 3D viewport or hierarchy to inspect.");
            ImGui::End();
            return;
        }

        Entity e = m_selectedEntity;

        // 1. Name & Top Action Bar
        if (registry().has<Name>(e)) {
            auto& n = registry().get<Name>(e);
            char nameBuf[128];
            strncpy(nameBuf, n.value.c_str(), sizeof(nameBuf));
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                n.value = nameBuf;
            }
        }
        if (ImGui::Button("Duplicate (Ctrl+D)", ImVec2(160, 0))) {
            duplicateEntity(e);
            ImGui::End();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(80, 0))) {
            scene().destroy(e);
            m_selectedEntity = NullEntity;
            ImGui::End();
            return;
        }

        ImGui::Separator();

        // 2. Transform Component
        if (registry().has<Transform>(e) && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& tr = registry().get<Transform>(e);
            glm::vec3 oldPos = tr.position;
            glm::vec3 oldRot = tr.rotation;
            bool changed = false;

            if (ImGui::DragFloat3("Position", &tr.position.x, 0.05f)) changed = true;
            if (ImGui::DragFloat3("Rotation", &tr.rotation.x, 0.5f)) changed = true;
            if (ImGui::DragFloat3("Scale", &tr.scale.x, 0.02f, 0.001f, 100.0f)) changed = true;
            
            glm::vec3 fwd = tr.forward();
            glm::vec3 right = tr.right();
            ImGui::TextDisabled("Fwd: (%.2f, %.2f, %.2f) | Right: (%.2f, %.2f, %.2f)", fwd.x, fwd.y, fwd.z, right.x, right.y, right.z);

            if (ImGui::Button("Reset Pos")) { tr.position = {0,0,0}; changed = true; }
            ImGui::SameLine();
            if (ImGui::Button("Reset Rot")) { tr.rotation = {0,0,0}; changed = true; }
            ImGui::SameLine();
            if (ImGui::Button("Reset Scale")) { tr.scale = {1,1,1}; changed = true; }

            if (changed) {
                propagateTransformDeltaToChildren(e, tr.position - oldPos, tr.rotation - oldRot);
                syncTransformToPhysics(e);
            }
        }

        // 2b. Hierarchy & Parenting (Наследование трансформа)
        if (ImGui::CollapsingHeader("Hierarchy / Parenting (Наследование)", ImGuiTreeNodeFlags_DefaultOpen)) {
            Entity currentParent = NullEntity;
            if (registry().has<Hierarchy>(e)) {
                currentParent = registry().get<Hierarchy>(e).parent;
            }

            std::string parentName = "None (Root Entity)";
            if (registry().valid(currentParent) && registry().has<Name>(currentParent)) {
                parentName = registry().get<Name>(currentParent).value + " (#" + std::to_string((uint32_t)currentParent) + ")";
            }

            ImGui::Text("Parent: %s", parentName.c_str());
            if (ImGui::BeginCombo("Select Parent Entity", parentName.c_str())) {
                if (ImGui::Selectable("None (Root Entity)", !registry().valid(currentParent))) {
                    if (registry().has<Hierarchy>(e)) {
                        registry().remove<Hierarchy>(e);
                    }
                }
                for (Entity other : registry().view<Transform>()) {
                    if (other == e) continue;
                    bool isDescendant = false;
                    Entity check = other;
                    while (registry().valid(check) && registry().has<Hierarchy>(check)) {
                        check = registry().get<Hierarchy>(check).parent;
                        if (check == e) { isDescendant = true; break; }
                    }
                    if (isDescendant) continue;

                    std::string oName = "Entity_" + std::to_string((uint32_t)other);
                    if (registry().has<Name>(other)) oName = registry().get<Name>(other).value;
                    bool isSel = (currentParent == other);
                    if (ImGui::Selectable((oName + "##parent_" + std::to_string((uint32_t)other)).c_str(), isSel)) {
                        auto& h = registry().has<Hierarchy>(e) ? registry().get<Hierarchy>(e) : registry().emplace<Hierarchy>(e);
                        h.parent = other;
                    }
                    if (isSel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (registry().valid(currentParent)) {
                ImGui::SameLine();
                if (ImGui::Button("Detach (Clear Parent)")) {
                    registry().remove<Hierarchy>(e);
                }
            }
        }

        // 3. Camera Component
        if (registry().has<Camera>(e) && ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& cam = registry().get<Camera>(e);
            ImGui::SliderFloat("Field of View", &cam.fov, 10.0f, 130.0f, "%.1f deg");
            ImGui::DragFloat("Near Clip Plane", &cam.nearPlane, 0.01f, 0.001f, 10.0f);
            ImGui::DragFloat("Far Clip Plane", &cam.farPlane, 1.0f, 1.0f, 10000.0f);
            ImGui::Checkbox("Primary Camera", &cam.primary);
            ImGui::Checkbox("Perspective", &cam.perspective);
            if (!cam.perspective) {
                ImGui::DragFloat("Ortho Size", &cam.orthoSize, 0.1f, 0.1f, 100.0f);
            }

            if (ImGui::Button("Align Editor View to this Camera")) {
                auto& tr = registry().get<Transform>(e);
                m_camPos = tr.position;
                m_camPitch = tr.rotation.x;
                m_camYaw = tr.rotation.y;
                m_camFov = cam.fov;
            }
            ImGui::SameLine();
            if (ImGui::Button("Align Camera to Editor View")) {
                auto& tr = registry().get<Transform>(e);
                tr.position = m_camPos;
                tr.rotation = {m_camPitch, m_camYaw, 0.0f};
                cam.fov = m_camFov;
            }

            if (ImGui::Button("Remove Camera Component")) {
                registry().remove<Camera>(e);
            }
        }

        // 4. MeshRenderer & PBR Material
        if (registry().has<MeshRenderer>(e) && ImGui::CollapsingHeader("Mesh & PBR Material", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& mr = registry().get<MeshRenderer>(e);
            ImGui::Checkbox("Visible", &mr.visible);
            ImGui::SameLine();
            ImGui::Checkbox("Cast Shadows", &mr.castShadow);
            ImGui::SameLine();
            ImGui::Checkbox("ClusterLOD", &mr.clusterLOD);

            char assetBuf[256];
            strncpy(assetBuf, mr.assetPath.c_str(), sizeof(assetBuf));
            if (ImGui::InputText("Model Path", assetBuf, sizeof(assetBuf))) {
                mr.assetPath = assetBuf;
            }

            ImGui::SeparatorText("Material Asset");
            std::string matDisplayName = mr.material.materialPath.empty() ? "[Custom / Embedded Material]" : std::filesystem::path(mr.material.materialPath).stem().string();
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Material: %s", matDisplayName.c_str());

            // DragDrop Target for material assignment
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH")) {
                    const char* matPath = (const char*)payload->Data;
                    loadMaterialFromFile(matPath, mr.material);
                }
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    const char* assetPath = (const char*)payload->Data;
                    std::string p = assetPath;
                    if (p.find(".mat.json") != std::string::npos) {
                        loadMaterialFromFile(p, mr.material);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Material Quick Picker Dropdown
            if (ImGui::BeginCombo("Select Material", matDisplayName.c_str())) {
                if (std::filesystem::exists("assets/materials")) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator("assets/materials")) {
                        if (entry.is_regular_file() && (entry.path().extension() == ".json" || entry.path().string().find(".mat") != std::string::npos)) {
                            std::string stem = entry.path().stem().string();
                            bool isSel = (mr.material.materialPath == entry.path().string());
                            if (ImGui::Selectable(stem.c_str(), isSel)) {
                                loadMaterialFromFile(entry.path().string(), mr.material);
                            }
                            if (isSel) ImGui::SetItemDefaultFocus();
                        }
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SeparatorText("PBR Material Parameters");
            ImGui::ColorEdit3("Albedo Color", &mr.material.albedo.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel);
            ImGui::SliderFloat("Metallic", &mr.material.metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &mr.material.roughness, 0.04f, 1.0f);
            ImGui::SliderFloat("AO Intensity", &mr.material.ao, 0.0f, 1.0f);
            ImGui::ColorEdit3("Emissive Color", &mr.material.emissive.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

            ImGui::SeparatorText("Texture Maps (via Material)");
            if (!mr.material.diffuseMapPath.empty()) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ALB] %s", std::filesystem::path(mr.material.diffuseMapPath).filename().string().c_str());
            } else {
                ImGui::TextDisabled("[ALB] No Albedo Map (Uniform Color)");
            }
            if (!mr.material.normalMapPath.empty()) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[NRM] %s", std::filesystem::path(mr.material.normalMapPath).filename().string().c_str());
                ImGui::SameLine();
                ImGui::Checkbox("Use Normal Map", &mr.material.useNormalMap);
            } else {
                ImGui::TextDisabled("[NRM] No Normal Map");
            }
            if (!mr.material.specularMapPath.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "[SPEC] %s", std::filesystem::path(mr.material.specularMapPath).filename().string().c_str());
                ImGui::SameLine();
                ImGui::Checkbox("Use Spec/Rough Map", &mr.material.useSpecularMap);
            } else {
                ImGui::TextDisabled("[SPEC] No Specular/Roughness Map");
            }

            ImGui::Separator();
            if (ImGui::Button("Remove Mesh Component")) {
                registry().remove<MeshRenderer>(e);
            }
        }

        // 5. Point Light Component
        if (registry().has<PointLight>(e) && ImGui::CollapsingHeader("Point Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& pl = registry().get<PointLight>(e);
            ImGui::ColorEdit3("Color", &pl.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel);
            ImGui::SliderFloat("Intensity", &pl.intensity, 0.0f, 100.0f, "%.1f");
            ImGui::SliderFloat("Range (Radius)", &pl.range, 0.1f, 100.0f, "%.1f m");
            ImGui::DragFloat("Constant Atten", &pl.constant, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Linear Atten", &pl.linear, 0.005f, 0.0f, 2.0f);
            ImGui::DragFloat("Quadratic Atten", &pl.quadratic, 0.001f, 0.0f, 1.0f);

            ImGui::SeparatorText("Presets");
            if (ImGui::Button("3200K Halogen")) { pl.color = {1.0f, 0.82f, 0.62f}; pl.intensity = 6.0f; }
            ImGui::SameLine();
            if (ImGui::Button("6500K Daylight")) { pl.color = {0.95f, 0.98f, 1.0f}; pl.intensity = 6.0f; }
            ImGui::SameLine();
            if (ImGui::Button("Cyber Blue")) { pl.color = {0.05f, 0.6f, 1.0f}; pl.intensity = 10.0f; }
            
            if (ImGui::Button("Siren Red")) { pl.color = {1.0f, 0.05f, 0.1f}; pl.intensity = 10.0f; }
            ImGui::SameLine();
            if (ImGui::Button("Emerald")) { pl.color = {0.1f, 1.0f, 0.35f}; pl.intensity = 8.0f; }
            ImGui::SameLine();
            if (ImGui::Button("Candle")) { pl.color = {1.0f, 0.55f, 0.15f}; pl.intensity = 3.0f; }

            ImGui::Separator();
            if (ImGui::Button("Remove Light Component")) {
                registry().remove<PointLight>(e);
            }
        }

        // 6. Directional Light (Sun) Component
        if (registry().has<DirectionalLight>(e) && ImGui::CollapsingHeader("Directional Light (Sun)", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& dl = registry().get<DirectionalLight>(e);
            ImGui::DragFloat3("Direction", &dl.direction.x, 0.02f, -1.0f, 1.0f);
            if (ImGui::Button("Normalize Direction")) dl.direction = glm::normalize(dl.direction);
            ImGui::ColorEdit3("Sun Color", &dl.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel);
            ImGui::SliderFloat("Intensity (Lux)", &dl.intensity, 0.0f, 20.0f, "%.2f");

            ImGui::SeparatorText("Sun Presets");
            if (ImGui::Button("Noon Sun")) { dl.direction = glm::normalize(glm::vec3(-0.2f, -0.95f, -0.2f)); dl.color = {1.0f, 0.98f, 0.92f}; dl.intensity = 3.0f; }
            ImGui::SameLine();
            if (ImGui::Button("Sunset")) { dl.direction = glm::normalize(glm::vec3(-0.85f, -0.2f, -0.3f)); dl.color = {1.0f, 0.55f, 0.25f}; dl.intensity = 3.5f; }
            ImGui::SameLine();
            if (ImGui::Button("Dawn")) { dl.direction = glm::normalize(glm::vec3(0.85f, -0.15f, 0.3f)); dl.color = {0.9f, 0.65f, 0.75f}; dl.intensity = 2.5f; }

            ImGui::Separator();
            if (ImGui::Button("Remove Sun Component")) {
                registry().remove<DirectionalLight>(e);
            }
        }

        // 7. Spot Light Component
        if (registry().has<SpotLight>(e) && ImGui::CollapsingHeader("Spot Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& sl = registry().get<SpotLight>(e);
            ImGui::DragFloat3("Direction", &sl.direction.x, 0.02f, -1.0f, 1.0f);
            ImGui::ColorEdit3("Color", &sl.color.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Intensity", &sl.intensity, 0.0f, 100.0f);
            ImGui::SliderFloat("Cutoff Angle", &sl.cutoff, 1.0f, 89.0f);
            ImGui::SliderFloat("Outer Cutoff", &sl.outerCutoff, 1.0f, 89.0f);

            ImGui::Separator();
            if (ImGui::Button("Remove Spot Light")) {
                registry().remove<SpotLight>(e);
            }
        }

        // 8. PhysX Rigidbody Component
        if (registry().has<cjoka_phys::Rigidbody>(e) && ImGui::CollapsingHeader("PhysX Rigidbody Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& rb = registry().get<cjoka_phys::Rigidbody>(e);
            const char* kinds[] = { "Static", "Dynamic", "Kinematic" };
            int currentKind = (int)rb.kind;
            if (ImGui::Combo("Body Type", &currentKind, kinds, IM_ARRAYSIZE(kinds))) {
                rb.kind = (cjoka_phys::Rigidbody::Kind)currentKind;
            }
            ImGui::DragFloat("Density (kg/m3)", &rb.density, 0.1f, 0.01f, 10000.0f);
            ImGui::DragFloat("Linear Damping", &rb.linearDamping, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Angular Damping", &rb.angularDamping, 0.01f, 0.0f, 10.0f);
            ImGui::Checkbox("Use Gravity", &rb.gravity);
            ImGui::Checkbox("Continuous CD (CCD)", &rb.ccd);

            ImGui::SeparatorText("Lock Rotation Constraints");
            ImGui::Checkbox("Lock X", &rb.lockRotationX); ImGui::SameLine();
            ImGui::Checkbox("Lock Y", &rb.lockRotationY); ImGui::SameLine();
            ImGui::Checkbox("Lock Z", &rb.lockRotationZ);

            ImGui::Separator();
            if (ImGui::Button("Remove Rigidbody")) {
                registry().remove<cjoka_phys::Rigidbody>(e);
            }
        }

        // 9. PhysX Collider Component
        if (registry().has<cjoka_phys::Collider>(e) && ImGui::CollapsingHeader("PhysX Collider Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& col = registry().get<cjoka_phys::Collider>(e);
            const char* shapeTypes[] = { "Box", "Sphere", "Capsule", "Plane" };
            int currentType = (int)col.type;
            if (ImGui::Combo("Collider Shape", &currentType, shapeTypes, IM_ARRAYSIZE(shapeTypes))) {
                col.type = (cjoka_phys::ColliderType)currentType;
                syncTransformToPhysics(e);
            }
            if (col.type == cjoka_phys::ColliderType::Box) {
                if (ImGui::DragFloat3("Half Extents", &col.halfExtents.x, 0.05f, 0.01f, 100.0f)) syncTransformToPhysics(e);
            } else if (col.type == cjoka_phys::ColliderType::Sphere) {
                if (ImGui::DragFloat("Radius", &col.radius, 0.05f, 0.01f, 100.0f)) syncTransformToPhysics(e);
            } else if (col.type == cjoka_phys::ColliderType::Capsule) {
                if (ImGui::DragFloat("Capsule Radius", &col.radius, 0.05f, 0.01f, 50.0f)) syncTransformToPhysics(e);
                if (ImGui::DragFloat("Capsule Height", &col.height, 0.05f, 0.01f, 50.0f)) syncTransformToPhysics(e);
            }

            if (ImGui::DragFloat3("Center Offset", &col.centerOffset.x, 0.05f)) syncTransformToPhysics(e);
            ImGui::SliderFloat("Static Friction", &col.staticFriction, 0.0f, 1.5f);
            ImGui::SliderFloat("Dynamic Friction", &col.dynamicFriction, 0.0f, 1.5f);
            ImGui::SliderFloat("Restitution (Bounciness)", &col.restitution, 0.0f, 1.0f);
            ImGui::Checkbox("Is Trigger", &col.isTrigger);

            ImGui::Separator();
            if (ImGui::Button("Remove Collider")) {
                registry().remove<cjoka_phys::Collider>(e);
            }
        }

        // 10. Character Controller Component
        if (registry().has<CharacterController>(e) && ImGui::CollapsingHeader("Character Controller Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& cc = registry().get<CharacterController>(e);
            if (ImGui::DragFloat("Radius", &cc.radius, 0.02f, 0.1f, 5.0f)) syncTransformToPhysics(e);
            if (ImGui::DragFloat("Height", &cc.height, 0.05f, 0.1f, 10.0f)) syncTransformToPhysics(e);
            ImGui::DragFloat("Walk Speed", &cc.speed, 0.1f, 0.0f, 50.0f);
            ImGui::DragFloat("Jump Force", &cc.jumpForce, 0.1f, 0.0f, 30.0f);
            ImGui::Text("Grounded: %s | Vel Y: %.2f", cc.onGround ? "YES" : "NO", cc.velocity.y);

            ImGui::Separator();
            if (ImGui::Button("Remove Character Controller")) {
                registry().remove<CharacterController>(e);
            }
        }

        // 10b. C++ Native Script Component
        if (registry().has<NativeScript>(e) && ImGui::CollapsingHeader("C++ Game Script Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& ns = registry().get<NativeScript>(e);
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "[Attached Script] %s", ns.scriptName.c_str());

            auto& allScripts = ScriptRegistry::Get().allScripts();
            if (ImGui::BeginCombo("Change Script", ns.scriptName.c_str())) {
                for (const auto& [name, factory] : allScripts) {
                    bool isSel = (ns.scriptName == name);
                    if (ImGui::Selectable(name.c_str(), isSel)) {
                        ns.scriptName = name;
                        ns.instantiate = factory;
                        ns.instance = factory();
                        if (ns.instance) {
                            ns.instance->_init(e, &registry());
                            ns.instance->onCreate();
                        }
                    }
                    if (isSel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();
            if (ImGui::Button("Remove Script Component")) {
                registry().remove<NativeScript>(e);
            }
        }

        // 11. Atmosphere / Sky Component
        if (registry().has<Sky>(e) && ImGui::CollapsingHeader("Sky & Atmosphere Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& sky = registry().get<Sky>(e);
            ImGui::ColorEdit3("Top Color", &sky.top.x, ImGuiColorEditFlags_Float);
            ImGui::ColorEdit3("Horizon Color", &sky.horizon.x, ImGuiColorEditFlags_Float);
            ImGui::ColorEdit3("Bottom Color", &sky.bottom.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Exposure", &sky.exposure, 0.1f, 5.0f);

            if (ImGui::Button("Clear Day")) sky = Sky::ClearDay();
            ImGui::SameLine();
            if (ImGui::Button("Sunset")) sky = Sky::Sunset();
            ImGui::SameLine();
            if (ImGui::Button("Night")) sky = Sky::Night();

            ImGui::Separator();
            if (ImGui::Button("Remove Sky Component")) {
                registry().remove<Sky>(e);
            }
        }

        // 12. Fog Component
        if (registry().has<Fog>(e) && ImGui::CollapsingHeader("Fog Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& fog = registry().get<Fog>(e);
            ImGui::ColorEdit3("Fog Color", &fog.color.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Fog Density", &fog.density, 0.0001f, 0.05f, "%.5f");

            ImGui::Separator();
            if (ImGui::Button("Remove Fog Component")) {
                registry().remove<Fog>(e);
            }
        }

        // 13. Post Process Settings Component
        if (registry().has<PostProcessSettings>(e) && ImGui::CollapsingHeader("Post Process Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& pp = registry().get<PostProcessSettings>(e);
            ImGui::SliderFloat("Bloom Threshold", &pp.bloomThreshold, 0.0f, 3.0f);
            ImGui::SliderFloat("Bloom Intensity", &pp.bloomIntensity, 0.0f, 3.0f);
            ImGui::SliderFloat("Exposure", &pp.exposure, 0.1f, 5.0f);
            ImGui::SliderFloat("Vignette", &pp.vignette, 0.0f, 1.0f);

            ImGui::Separator();
            if (ImGui::Button("Remove Post Process")) {
                registry().remove<PostProcessSettings>(e);
            }
        }

        // 14. Audio Source Component
        if (registry().has<Audio::AudioSourceComponent>(e) && ImGui::CollapsingHeader("Audio Source Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& as = registry().get<Audio::AudioSourceComponent>(e);
            char fileBuf[256];
            strncpy(fileBuf, as.filepath.c_str(), sizeof(fileBuf));
            if (ImGui::InputText("Sound File", fileBuf, sizeof(fileBuf))) {
                as.filepath = fileBuf;
            }
            ImGui::SliderFloat("Volume", &as.volume, 0.0f, 2.0f);
            ImGui::SliderFloat("Pitch", &as.pitch, 0.1f, 3.0f);
            ImGui::Checkbox("Looping", &as.loop);
            ImGui::Checkbox("3D Spatial Audio", &as.spatial);
            if (as.spatial) {
                ImGui::DragFloat("Min Distance", &as.minDistance, 0.1f, 0.1f, 50.0f);
                ImGui::DragFloat("Max Distance", &as.maxDistance, 0.5f, 1.0f, 500.0f);
            }

            if (ImGui::Button("Play Audio", ImVec2(120, 0))) {
                as.play();
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop Audio", ImVec2(120, 0))) {
                as.stop();
            }

            ImGui::Separator();
            if (ImGui::Button("Remove Audio Source")) {
                registry().remove<Audio::AudioSourceComponent>(e);
            }
        }

        // 15. Native C++ Script Component
        if (registry().has<NativeScript>(e) && ImGui::CollapsingHeader("C++ Script Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& ns = registry().get<NativeScript>(e);
            ImGui::Text("Script: %s", ns.scriptName.c_str());
            if (!ns.instance && ns.instantiate) {
                ns.instance = ns.instantiate();
                ns.instance->_init(e, &registry());
                ns.instance->onCreate();
            }
            if (ns.instance) {
                ImGui::Separator();
                ns.instance->onInspectorGUI();
            }
            ImGui::Separator();
            if (ImGui::Button("Remove Script Component")) {
                registry().remove<NativeScript>(e);
            }
        }

        // 16. Big, Categorized & Searchable "+ Add Component" Button
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.75f, 1.0f));
        if (ImGui::Button("+ Add Component", ImVec2(-1, 38))) {
            ImGui::OpenPopup("AddComponentPopup");
        }
        ImGui::PopStyleColor();

        if (ImGui::BeginPopup("AddComponentPopup")) {
            static char compSearch[64] = "";
            ImGui::InputTextWithHint("##CompSearch", "Search components...", compSearch, sizeof(compSearch));
            ImGui::Separator();

            std::string filter = compSearch;
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

            auto matches = [&](const std::string& name) {
                if (filter.empty()) return true;
                std::string lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                return lower.find(filter) != std::string::npos;
            };

            // Mesh & Rendering
            if (ImGui::BeginMenu("Mesh & Rendering")) {
                if (!registry().has<MeshRenderer>(e) && matches("Mesh Renderer") && ImGui::MenuItem("Mesh Renderer")) {
                    Material m; m.albedo = {0.85f, 0.85f, 0.88f}; m.metallic = 0.2f; m.roughness = 0.4f;
                    MeshRenderer mr(Assets::Cube(1.0f), m);
                    mr.assetPath = "primitive:cube";
                    registry().emplace<MeshRenderer>(e, mr);
                }
                if (!registry().has<Decal>(e) && matches("Decal") && ImGui::MenuItem("Decal Projector")) {
                    registry().emplace<Decal>(e);
                }
                ImGui::EndMenu();
            }

            // Physics
            if (ImGui::BeginMenu("Physics (PhysX 5.5)")) {
                if (!registry().has<cjoka_phys::Rigidbody>(e) && matches("Rigidbody Dynamic") && ImGui::MenuItem("Rigidbody (Dynamic)")) {
                    registry().emplace<cjoka_phys::Rigidbody>(e, cjoka_phys::Rigidbody::Dynamic(1.0f));
                }
                if (!registry().has<cjoka_phys::Rigidbody>(e) && matches("Rigidbody Static") && ImGui::MenuItem("Rigidbody (Static)")) {
                    registry().emplace<cjoka_phys::Rigidbody>(e, cjoka_phys::Rigidbody::Static());
                }
                if (!registry().has<cjoka_phys::Rigidbody>(e) && matches("Rigidbody Kinematic") && ImGui::MenuItem("Rigidbody (Kinematic)")) {
                    registry().emplace<cjoka_phys::Rigidbody>(e, cjoka_phys::Rigidbody::Kinematic());
                }
                if (!registry().has<cjoka_phys::Collider>(e) && matches("Box Collider") && ImGui::MenuItem("Box Collider")) {
                    registry().emplace<cjoka_phys::Collider>(e, cjoka_phys::Collider::Box({0.5f, 0.5f, 0.5f}));
                }
                if (!registry().has<cjoka_phys::Collider>(e) && matches("Sphere Collider") && ImGui::MenuItem("Sphere Collider")) {
                    registry().emplace<cjoka_phys::Collider>(e, cjoka_phys::Collider::Sphere(0.5f));
                }
                if (!registry().has<cjoka_phys::Collider>(e) && matches("Capsule Collider") && ImGui::MenuItem("Capsule Collider")) {
                    registry().emplace<cjoka_phys::Collider>(e, cjoka_phys::Collider::Capsule(0.35f, 1.2f));
                }
                if (!registry().has<CharacterController>(e) && matches("Character Controller") && ImGui::MenuItem("Character Controller")) {
                    registry().emplace<CharacterController>(e);
                }
                ImGui::EndMenu();
            }

            // Lighting & Atmosphere
            if (ImGui::BeginMenu("Lighting & Environment")) {
                if (!registry().has<PointLight>(e) && matches("Point Light") && ImGui::MenuItem("Point Light")) {
                    registry().emplace<PointLight>(e, PointLight{{1.0f, 0.9f, 0.8f}, 6.0f, 16.0f});
                }
                if (!registry().has<DirectionalLight>(e) && matches("Directional Light") && ImGui::MenuItem("Directional Light (Sun)")) {
                    registry().emplace<DirectionalLight>(e, DirectionalLight{glm::normalize(glm::vec3{-0.4f, -0.8f, -0.3f}), {1.0f, 0.95f, 0.85f}, 2.5f});
                }
                if (!registry().has<SpotLight>(e) && matches("Spot Light") && ImGui::MenuItem("Spot Light")) {
                    registry().emplace<SpotLight>(e);
                }
                if (!registry().has<Sky>(e) && matches("Sky Atmosphere") && ImGui::MenuItem("Sky & Atmosphere")) {
                    registry().emplace<Sky>(e, Sky::ClearDay());
                }
                if (!registry().has<Fog>(e) && matches("Fog") && ImGui::MenuItem("Fog Effect")) {
                    registry().emplace<Fog>(e, Fog{{0.2f, 0.22f, 0.28f}, 0.003f});
                }
                if (!registry().has<PostProcessSettings>(e) && matches("Post Process Settings") && ImGui::MenuItem("Post Process Settings")) {
                    registry().emplace<PostProcessSettings>(e, PostProcessSettings::Cinematic());
                }
                ImGui::EndMenu();
            }

            // Camera & Audio
            if (ImGui::BeginMenu("Camera & Audio")) {
                if (!registry().has<Camera>(e) && matches("Camera") && ImGui::MenuItem("Camera")) {
                    registry().emplace<Camera>(e, Camera{65.0f, 0.1f, 1000.0f, true, true, 10.0f});
                }
                if (!registry().has<Audio::AudioSourceComponent>(e) && matches("Audio Source") && ImGui::MenuItem("Audio Source")) {
                    registry().emplace<Audio::AudioSourceComponent>(e);
                }
                if (!registry().has<Audio::AudioListenerComponent>(e) && matches("Audio Listener") && ImGui::MenuItem("Audio Listener")) {
                    registry().emplace<Audio::AudioListenerComponent>(e);
                }
                ImGui::EndMenu();
            }

            // C++ Game Scripts
            if (ImGui::BeginMenu("C++ Game Scripts")) {
                for (const auto& [scriptName, factory] : ScriptRegistry::Get().allScripts()) {
                    if (matches(scriptName) && ImGui::MenuItem(scriptName.c_str())) {
                        auto& ns = registry().has<NativeScript>(e) ? registry().get<NativeScript>(e) : registry().emplace<NativeScript>(e);
                        ns.scriptName = scriptName;
                        ns.instantiate = factory;
                        ns.instance = factory();
                        if (ns.instance) {
                            ns.instance->_init(e, &registry());
                            ns.instance->onCreate();
                            ns.instance->onStart();
                        }
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void SceneEditor::renderCameraPreview() {
    if (!registry().valid(m_selectedEntity) || !registry().has<Camera>(m_selectedEntity)) return;

    auto& camComp = registry().get<Camera>(m_selectedEntity);
    auto& camTr = registry().get<Transform>(m_selectedEntity);
    std::string name = registry().has<Name>(m_selectedEntity) ? registry().get<Name>(m_selectedEntity).value : "Camera";

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 380.0f - 290.0f, io.DisplaySize.y - 215.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(280.0f, 195.0f), ImGuiCond_Always);

    if (ImGui::Begin("Camera Preview", &m_showCameraPreview, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Preview: %s", name.c_str());
        ImGui::TextDisabled("FOV: %.1f | Near: %.2f | Far: %.1f", camComp.fov, camComp.nearPlane, camComp.farPlane);

        glm::vec3 fwd = camTr.forward();
        ImGui::Text("Direction: (%.2f, %.2f, %.2f)", fwd.x, fwd.y, fwd.z);

        if (ImGui::Button("Set Primary Camera", ImVec2(-1, 28))) {
            for (Entity ent : registry().view<Camera>()) {
                registry().get<Camera>(ent).primary = (ent == m_selectedEntity);
            }
        }

        if (ImGui::Button("Match Scene View to Camera", ImVec2(-1, 28))) {
            m_camPos = camTr.position;
            m_camPitch = camTr.rotation.x;
            m_camYaw = camTr.rotation.y;
            m_camFov = camComp.fov;
        }
    }
    ImGui::End();
}

void SceneEditor::renderAssetBrowser() {
    int w, h; window().getFramebufferSize(w, h);
    ImGui::SetNextWindowPos(ImVec2(16.0f, (float)h - 225.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)w - 32.0f, 210.0f), ImGuiCond_Always);

    if (ImGui::Begin("Asset Browser", &m_showAssetBrowser)) {
        glm::vec3 spawnPos = m_camPos + glm::vec3(0, 0, 4);

        if (ImGui::BeginTabBar("AssetCategories")) {
            if (ImGui::BeginTabItem("File Explorer")) {
                // Top Bar: Navigation, Search, Create Asset, Actions
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.35f, 1.0f));
                if (m_currentDirectory != "assets") {
                    if (ImGui::Button("[ < Back ]")) {
                        m_currentDirectory = m_currentDirectory.parent_path();
                        m_selectedAssetPath.clear();
                    }
                    ImGui::SameLine();
                }
                ImGui::PopStyleColor();

                // [+] Create Asset Button with popup menu
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.70f, 0.45f, 1.0f));
                if (ImGui::Button("[+] Add / Create...")) {
                    ImGui::OpenPopup("CreateAssetDropdown");
                }
                ImGui::PopStyleColor(2);

                if (ImGui::BeginPopup("CreateAssetDropdown")) {
                    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Create New Asset in: %s", m_currentDirectory.filename().string().c_str());
                    ImGui::Separator();
                    if (ImGui::MenuItem("PBR Material (.mat.json)")) {
                        m_createAssetType = 0;
                        strncpy(m_createAssetNameBuf, "NewPBRMaterial", sizeof(m_createAssetNameBuf));
                        m_showCreateAssetModal = true;
                    }
                    if (ImGui::MenuItem("Directory / Folder")) {
                        m_createAssetType = 1;
                        strncpy(m_createAssetNameBuf, "NewFolder", sizeof(m_createAssetNameBuf));
                        m_showCreateAssetModal = true;
                    }
                    if (ImGui::MenuItem("Scene File (.json)")) {
                        m_createAssetType = 2;
                        strncpy(m_createAssetNameBuf, "NewScene", sizeof(m_createAssetNameBuf));
                        m_showCreateAssetModal = true;
                    }
                    if (ImGui::MenuItem("C++ Script Template (.h)")) {
                        m_createAssetType = 3;
                        strncpy(m_createAssetNameBuf, "CustomGameScript", sizeof(m_createAssetNameBuf));
                        m_showCreateAssetModal = true;
                    }
                    ImGui::EndPopup();
                }

                ImGui::SameLine();
                ImGui::Text("Dir: %s", m_currentDirectory.string().c_str());

                ImGui::SameLine();
                ImGui::SetNextItemWidth(180.0f);
                ImGui::InputTextWithHint("##AssetFilter", "Filter assets...", m_assetSearchBuf, sizeof(m_assetSearchBuf));

                if (!m_selectedAssetPath.empty() && std::filesystem::exists(m_selectedAssetPath)) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "| %s", m_selectedAssetPath.filename().string().c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("Rename")) {
                        m_assetToRename = m_selectedAssetPath;
                        strncpy(m_renameBuf, m_selectedAssetPath.filename().string().c_str(), sizeof(m_renameBuf));
                        m_showRenameAssetModal = true;
                    }
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    if (ImGui::Button("Delete")) {
                        m_assetToDelete = m_selectedAssetPath;
                        m_showDeleteAssetModal = true;
                    }
                    ImGui::PopStyleColor();
                }

                ImGui::Separator();

                // Folder & File Cards
                ImGui::BeginChild("FilesView", ImVec2(0, 0), true);
                if (std::filesystem::exists(m_currentDirectory)) {
                    std::vector<std::filesystem::directory_entry> entries;
                    for (const auto& entry : std::filesystem::directory_iterator(m_currentDirectory)) {
                        std::string fn = entry.path().filename().string();
                        if (!fn.empty() && fn[0] != '.') {
                            if (strlen(m_assetSearchBuf) > 0) {
                                std::string lowerFn = fn;
                                std::string lowerSearch = m_assetSearchBuf;
                                std::transform(lowerFn.begin(), lowerFn.end(), lowerFn.begin(), ::tolower);
                                std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
                                if (lowerFn.find(lowerSearch) == std::string::npos) continue;
                            }
                            entries.push_back(entry);
                        }
                    }
                    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                        if (a.is_directory() != b.is_directory()) return a.is_directory();
                        return a.path().filename().string() < b.path().filename().string();
                    });

                    for (const auto& entry : entries) {
                        const auto& p = entry.path();
                        std::string filename = p.filename().string();
                        bool isSel = (m_selectedAssetPath == p);

                        if (entry.is_directory()) {
                            std::string label = "[Folder]\n" + filename;
                            ImGui::PushStyleColor(ImGuiCol_Button, isSel ? ImVec4(0.35f, 0.55f, 0.85f, 1.0f) : ImVec4(0.2f, 0.3f, 0.4f, 0.9f));
                            if (ImGui::Button((label + "##dir_" + filename).c_str(), ImVec2(135, 48))) {
                                m_selectedAssetPath = p;
                            }
                            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                m_currentDirectory /= p.filename();
                                m_selectedAssetPath.clear();
                            }
                            ImGui::PopStyleColor();

                            // Right-click context menu for directory
                            if (ImGui::BeginPopupContextItem()) {
                                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Folder: %s", filename.c_str());
                                ImGui::Separator();
                                if (ImGui::MenuItem("Open Folder")) {
                                    m_currentDirectory /= p.filename();
                                    m_selectedAssetPath.clear();
                                }
                                if (ImGui::MenuItem("Rename Folder...")) {
                                    m_assetToRename = p;
                                    strncpy(m_renameBuf, filename.c_str(), sizeof(m_renameBuf));
                                    m_showRenameAssetModal = true;
                                }
                                ImGui::Separator();
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                                if (ImGui::MenuItem("Delete Folder (Permanent)...")) {
                                    m_assetToDelete = p;
                                    m_showDeleteAssetModal = true;
                                }
                                ImGui::PopStyleColor();
                                ImGui::EndPopup();
                            }
                            ImGui::SameLine();
                        } else {
                            std::string ext = p.extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                            if (filename.find(".mat.json") != std::string::npos || ext == ".mat") {
                                // PBR Material Card
                                std::string stem = p.stem().string();
                                if (stem.find(".mat") != std::string::npos) stem = std::filesystem::path(stem).stem().string();
                                std::string label = "[Material]\n" + stem;

                                Material tempMat;
                                loadMaterialFromFile(p.string(), tempMat);
                                ImVec4 swatch = ImVec4(tempMat.albedo.r * 0.7f + 0.1f, tempMat.albedo.g * 0.7f + 0.1f, tempMat.albedo.b * 0.7f + 0.1f, 1.0f);

                                ImGui::PushStyleColor(ImGuiCol_Button, isSel ? ImVec4(0.5f, 0.7f, 1.0f, 1.0f) : swatch);
                                if (ImGui::Button((label + "##mat_" + filename).c_str(), ImVec2(135, 48))) {
                                    m_selectedAssetPath = p;
                                    m_selectedMaterialFile = p.string();
                                    m_editingMaterial = tempMat;
                                    if (registry().valid(m_selectedEntity) && registry().has<MeshRenderer>(m_selectedEntity)) {
                                        applyMaterialToEntity(m_selectedEntity, p.string());
                                    }
                                }
                                ImGui::PopStyleColor();

                                // Drag & Drop Material Source
                                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                                    std::string pStr = p.string();
                                    ImGui::SetDragDropPayload("MATERIAL_PATH", pStr.c_str(), pStr.size() + 1);
                                    ImGui::SetDragDropPayload("ASSET_PATH", pStr.c_str(), pStr.size() + 1);
                                    ImGui::Text("Material: %s", stem.c_str());
                                    ImGui::EndDragDropSource();
                                }

                                if (ImGui::BeginPopupContextItem()) {
                                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Material: %s", stem.c_str());
                                    ImGui::Separator();
                                    if (ImGui::MenuItem("Apply to Selected Entity")) {
                                        if (registry().valid(m_selectedEntity) && registry().has<MeshRenderer>(m_selectedEntity)) {
                                            applyMaterialToEntity(m_selectedEntity, p.string());
                                        }
                                    }
                                    if (ImGui::MenuItem("Open in Material Palette")) {
                                        m_showMaterialPalette = true;
                                        m_selectedMaterialFile = p.string();
                                        m_editingMaterial = tempMat;
                                    }
                                    if (ImGui::MenuItem("Rename Material...")) {
                                        m_assetToRename = p;
                                        strncpy(m_renameBuf, filename.c_str(), sizeof(m_renameBuf));
                                        m_showRenameAssetModal = true;
                                    }
                                    ImGui::Separator();
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                                    if (ImGui::MenuItem("Delete Material...")) {
                                        m_assetToDelete = p;
                                        m_showDeleteAssetModal = true;
                                    }
                                    ImGui::PopStyleColor();
                                    ImGui::EndPopup();
                                }
                                ImGui::SameLine();
                            } else if (ext == ".obj" || ext == ".gltf" || ext == ".fbx") {
                                std::string label = "[3D Model]\n" + filename;
                                ImGui::PushStyleColor(ImGuiCol_Button, isSel ? ImVec4(0.4f, 0.65f, 0.4f, 1.0f) : ImVec4(0.25f, 0.4f, 0.25f, 0.9f));
                                if (ImGui::Button((label + "##model_" + filename).c_str(), ImVec2(140, 48))) {
                                    m_selectedAssetPath = p;
                                    std::string tex = "";
                                    std::string candTex = p.parent_path().string() + "/" + p.stem().string() + ".png";
                                    if (std::filesystem::exists(candTex)) tex = candTex;
                                    else if (std::filesystem::exists("assets/textures/colormap.png")) tex = "assets/textures/colormap.png";
                                    m_selectedEntity = spawnModel(p.stem().string(), p.string(), tex, spawnPos, 1.0f);
                                }
                                ImGui::PopStyleColor();

                                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                                    std::string pStr = p.string();
                                    ImGui::SetDragDropPayload("ASSET_PATH", pStr.c_str(), pStr.size() + 1);
                                    ImGui::Text("Model: %s", filename.c_str());
                                    ImGui::EndDragDropSource();
                                }

                                if (ImGui::BeginPopupContextItem()) {
                                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Model: %s", filename.c_str());
                                    ImGui::Separator();
                                    if (ImGui::MenuItem("Spawn 3D Model in Scene")) {
                                        std::string tex = "";
                                        std::string candTex = p.parent_path().string() + "/" + p.stem().string() + ".png";
                                        if (std::filesystem::exists(candTex)) tex = candTex;
                                        else if (std::filesystem::exists("assets/textures/colormap.png")) tex = "assets/textures/colormap.png";
                                        m_selectedEntity = spawnModel(p.stem().string(), p.string(), tex, spawnPos, 1.0f);
                                    }
                                    if (ImGui::MenuItem("Rename Model...")) {
                                        m_assetToRename = p;
                                        strncpy(m_renameBuf, filename.c_str(), sizeof(m_renameBuf));
                                        m_showRenameAssetModal = true;
                                    }
                                    ImGui::Separator();
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                                    if (ImGui::MenuItem("Delete Model File...")) {
                                        m_assetToDelete = p;
                                        m_showDeleteAssetModal = true;
                                    }
                                    ImGui::PopStyleColor();
                                    ImGui::EndPopup();
                                }
                                ImGui::SameLine();
                            } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") {
                                std::string label = "[Texture]\n" + filename;
                                ImGui::PushStyleColor(ImGuiCol_Button, isSel ? ImVec4(0.6f, 0.45f, 0.3f, 1.0f) : ImVec4(0.4f, 0.3f, 0.2f, 0.9f));
                                if (ImGui::Button((label + "##tex_" + filename).c_str(), ImVec2(130, 48))) {
                                    m_selectedAssetPath = p;
                                    if (registry().valid(m_selectedEntity) && registry().has<MeshRenderer>(m_selectedEntity)) {
                                        auto& mr = registry().get<MeshRenderer>(m_selectedEntity);
                                        mr.material.diffuseMapPath = p.string();
                                        mr.material.diffuseMap = Assets::Texture(p.string(), true);
                                        mr.material.useDiffuseMap = (mr.material.diffuseMap && mr.material.diffuseMap->valid());
                                    }
                                }
                                ImGui::PopStyleColor();

                                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                                    std::string pStr = p.string();
                                    ImGui::SetDragDropPayload("ASSET_PATH", pStr.c_str(), pStr.size() + 1);
                                    ImGui::Text("Texture: %s", filename.c_str());
                                    ImGui::EndDragDropSource();
                                }

                                if (ImGui::BeginPopupContextItem()) {
                                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Texture: %s", filename.c_str());
                                    ImGui::Separator();
                                    if (ImGui::MenuItem("Apply Texture to Selected Mesh")) {
                                        if (registry().valid(m_selectedEntity) && registry().has<MeshRenderer>(m_selectedEntity)) {
                                            auto& mr = registry().get<MeshRenderer>(m_selectedEntity);
                                            mr.material.diffuseMapPath = p.string();
                                            mr.material.diffuseMap = Assets::Texture(p.string(), true);
                                            mr.material.useDiffuseMap = (mr.material.diffuseMap && mr.material.diffuseMap->valid());
                                        }
                                    }
                                    if (ImGui::MenuItem("Create PBR Material from this Texture")) {
                                        Material m;
                                        m.diffuseMapPath = p.string();
                                        m.diffuseMap = Assets::Texture(p.string(), true);
                                        m.useDiffuseMap = true;
                                        createNewMaterialFile(p.stem().string(), m);
                                    }
                                    if (ImGui::MenuItem("Rename Texture...")) {
                                        m_assetToRename = p;
                                        strncpy(m_renameBuf, filename.c_str(), sizeof(m_renameBuf));
                                        m_showRenameAssetModal = true;
                                    }
                                    ImGui::Separator();
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                                    if (ImGui::MenuItem("Delete Texture File...")) {
                                        m_assetToDelete = p;
                                        m_showDeleteAssetModal = true;
                                    }
                                    ImGui::PopStyleColor();
                                    ImGui::EndPopup();
                                }
                                ImGui::SameLine();
                            } else if (ext == ".json") {
                                std::string label = "[Scene]\n" + filename;
                                ImGui::PushStyleColor(ImGuiCol_Button, isSel ? ImVec4(0.55f, 0.3f, 0.6f, 1.0f) : ImVec4(0.35f, 0.2f, 0.4f, 0.9f));
                                if (ImGui::Button((label + "##scene_" + filename).c_str(), ImVec2(130, 48))) {
                                    m_selectedAssetPath = p;
                                    loadSceneFromFile(p.string());
                                    strncpy(m_sceneFileBuf, p.string().c_str(), sizeof(m_sceneFileBuf));
                                }
                                ImGui::PopStyleColor();

                                if (ImGui::BeginPopupContextItem()) {
                                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Scene: %s", filename.c_str());
                                    ImGui::Separator();
                                    if (ImGui::MenuItem("Load Scene File")) {
                                        loadSceneFromFile(p.string());
                                        strncpy(m_sceneFileBuf, p.string().c_str(), sizeof(m_sceneFileBuf));
                                    }
                                    if (ImGui::MenuItem("Rename Scene...")) {
                                        m_assetToRename = p;
                                        strncpy(m_renameBuf, filename.c_str(), sizeof(m_renameBuf));
                                        m_showRenameAssetModal = true;
                                    }
                                    ImGui::Separator();
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                                    if (ImGui::MenuItem("Delete Scene File...")) {
                                        m_assetToDelete = p;
                                        m_showDeleteAssetModal = true;
                                    }
                                    ImGui::PopStyleColor();
                                    ImGui::EndPopup();
                                }
                                ImGui::SameLine();
                            } else {
                                std::string label = "[File]\n" + filename;
                                ImGui::PushStyleColor(ImGuiCol_Button, isSel ? ImVec4(0.45f, 0.45f, 0.5f, 1.0f) : ImVec4(0.25f, 0.25f, 0.3f, 0.9f));
                                if (ImGui::Button((label + "##file_" + filename).c_str(), ImVec2(130, 48))) {
                                    m_selectedAssetPath = p;
                                }
                                ImGui::PopStyleColor();

                                if (ImGui::BeginPopupContextItem()) {
                                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "File: %s", filename.c_str());
                                    ImGui::Separator();
                                    if (ImGui::MenuItem("Rename File...")) {
                                        m_assetToRename = p;
                                        strncpy(m_renameBuf, filename.c_str(), sizeof(m_renameBuf));
                                        m_showRenameAssetModal = true;
                                    }
                                    ImGui::Separator();
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                                    if (ImGui::MenuItem("Delete File...")) {
                                        m_assetToDelete = p;
                                        m_showDeleteAssetModal = true;
                                    }
                                    ImGui::PopStyleColor();
                                    ImGui::EndPopup();
                                }
                                ImGui::SameLine();
                            }
                        }
                    }
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Models & Vehicles")) {
                ImGui::TextDisabled("Click any asset to spawn into current camera view:");
                ImGui::Separator();

                if (ImGui::Button("Assemble Modular Vehicle\n(Chassis + 4 Wheels + Lights)", ImVec2(240, 50)))
                    m_selectedEntity = assembleModularVehicle(spawnPos);

                ImGui::Separator();

                if (ImGui::Button("Police Cruiser\n(police.obj)", ImVec2(140, 50)))
                    m_selectedEntity = spawnModel("PoliceCruiser", "assets/models/cars/police.obj", "assets/textures/colormap.png", spawnPos, 1.4f);
                ImGui::SameLine();
                if (ImGui::Button("Sports Sedan\n(sedan-sports.obj)", ImVec2(140, 50)))
                    m_selectedEntity = spawnModel("SportsSedan", "assets/models/cars/sedan-sports.obj", "assets/textures/colormap.png", spawnPos, 1.4f);
                ImGui::SameLine();
                if (ImGui::Button("City Taxi\n(taxi.obj)", ImVec2(140, 50)))
                    m_selectedEntity = spawnModel("Taxi", "assets/models/cars/taxi.obj", "assets/textures/colormap.png", spawnPos, 1.4f);
                ImGui::SameLine();
                if (ImGui::Button("SUV Truck\n(suv.obj)", ImVec2(140, 50)))
                    m_selectedEntity = spawnModel("SUV", "assets/models/cars/suv.obj", "assets/textures/colormap.png", spawnPos, 1.4f);
                ImGui::SameLine();
                if (ImGui::Button("Park Bench\n(bench.obj)", ImVec2(140, 50)))
                    m_selectedEntity = spawnModel("Bench", "assets/models/bench.obj", "", spawnPos, 1.3f);
                ImGui::SameLine();
                if (ImGui::Button("Metal Barrel\n(barrel.obj)", ImVec2(140, 50)))
                    m_selectedEntity = spawnModel("Barrel", "assets/models/barrel.obj", "assets/textures/barrel.png", spawnPos, 1.2f);
                ImGui::SameLine();
                if (ImGui::Button("Indoor Plant\n(plant.obj)", ImVec2(140, 50)))
                    m_selectedEntity = spawnModel("Plant", "assets/models/indoor_plant.obj", "assets/textures/indoor_plant_COL.jpg", spawnPos, 0.25f);
                
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Lights & FX")) {
                ImGui::TextDisabled("Instant PBR point lights and neon emitters:");
                ImGui::Separator();

                if (ImGui::Button("Warm Halogen (3200K)\n[+ Light]", ImVec2(160, 50)))
                    m_selectedEntity = spawnPointLight(spawnPos, {1.0f, 0.85f, 0.65f}, 6.0f, 18.0f);
                ImGui::SameLine();
                if (ImGui::Button("Cyberpunk Cyan\n[+ Light]", ImVec2(160, 50)))
                    m_selectedEntity = spawnPointLight(spawnPos, {0.1f, 0.75f, 1.0f}, 8.0f, 20.0f);
                ImGui::SameLine();
                if (ImGui::Button("Police Siren Red\n[+ Light]", ImVec2(160, 50)))
                    m_selectedEntity = spawnPointLight(spawnPos, {1.0f, 0.08f, 0.12f}, 8.0f, 20.0f);
                ImGui::SameLine();
                if (ImGui::Button("Emerald Neon\n[+ Light]", ImVec2(160, 50)))
                    m_selectedEntity = spawnPointLight(spawnPos, {0.1f, 1.0f, 0.35f}, 8.0f, 20.0f);
                ImGui::SameLine();
                if (ImGui::Button("Magenta Neon\n[+ Light]", ImVec2(160, 50)))
                    m_selectedEntity = spawnPointLight(spawnPos, {0.95f, 0.1f, 0.85f}, 8.0f, 20.0f);

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Primitives")) {
                ImGui::TextDisabled("Physics & collision test primitives:");
                ImGui::Separator();

                if (ImGui::Button("PBR Cube\n(1x1x1)", ImVec2(130, 50)))
                    m_selectedEntity = spawnPrimitive("Cube", spawnPos);
                ImGui::SameLine();
                if (ImGui::Button("PBR Sphere\n(Radius 0.5)", ImVec2(130, 50)))
                    m_selectedEntity = spawnPrimitive("Sphere", spawnPos);
                ImGui::SameLine();
                if (ImGui::Button("Mirror Floor Plane\n(10x10)", ImVec2(140, 50)))
                    m_selectedEntity = spawnPrimitive("Plane", spawnPos);

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("C++ Scripts")) {
                ImGui::Text("Native C++ Script Engine (C++20 Hot-Reload)");
                ImGui::Separator();

                // Create new script generator
                static char newScriptNameBuf[64] = "MyPlayerScript";
                ImGui::InputText("New Script Class", newScriptNameBuf, sizeof(newScriptNameBuf));
                ImGui::SameLine();
                if (ImGui::Button("+ Create C++ Script File", ImVec2(190, 0))) {
                    createNewScriptFile(newScriptNameBuf);
                }

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "[Active] Registered C++ Game Scripts:");
                for (const auto& [scriptName, factory] : ScriptRegistry::Get().allScripts()) {
                    ImGui::BulletText("%s", scriptName.c_str());
                    ImGui::SameLine(180.0f);
                    if (registry().valid(m_selectedEntity)) {
                        std::string btnLabel = "Attach to Selected##" + scriptName;
                        if (ImGui::Button(btnLabel.c_str())) {
                            auto& ns = registry().has<NativeScript>(m_selectedEntity) ? registry().get<NativeScript>(m_selectedEntity) : registry().emplace<NativeScript>(m_selectedEntity);
                            ns.scriptName = scriptName;
                            ns.instantiate = factory;
                            ns.instance = factory();
                            if (ns.instance) {
                                ns.instance->_init(m_selectedEntity, &registry());
                                ns.instance->onCreate();
                            }
                        }
                    }
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

Entity SceneEditor::duplicateEntity(Entity e) {
    if (!registry().valid(e)) return NullEntity;

    std::string newName = "Entity_Copy";
    if (registry().has<Name>(e)) newName = registry().get<Name>(e).value + "_Copy";

    Transform newTr;
    if (registry().has<Transform>(e)) {
        newTr = registry().get<Transform>(e);
        newTr.position += glm::vec3(1.5f, 0.0f, 1.5f);
    }

    auto ref = scene().create(newName, newTr);
    Entity newEnt = ref.id();

    if (registry().has<MeshRenderer>(e)) {
        ref.add<MeshRenderer>(registry().get<MeshRenderer>(e));
    }
    if (registry().has<PointLight>(e)) {
        ref.add<PointLight>(registry().get<PointLight>(e));
    }
    if (registry().has<NativeScript>(e)) {
        auto& oldNs = registry().get<NativeScript>(e);
        auto& newNs = ref.add<NativeScript>();
        newNs.scriptName = oldNs.scriptName;
        newNs.instantiate = oldNs.instantiate;
        if (newNs.instantiate) {
            newNs.instance = newNs.instantiate();
            if (newNs.instance) {
                newNs.instance->_init(newEnt, &registry());
                newNs.instance->onCreate();
            }
        }
    }
    m_selectedEntity = newEnt;
    std::cout << "[SceneEditor] Duplicated Entity: " << (uint32_t)e << " -> " << (uint32_t)newEnt << " (" << newName << ")\n";
    return newEnt;
}

Entity SceneEditor::assembleModularVehicle(const glm::vec3& pos) {
    // 1. Root vehicle chassis
    Entity root = spawnModel("CustomCar_Chassis", "assets/models/cars/police.obj", "assets/textures/colormap.png", pos, 1.4f);
    
    // Attach Vehicle driver script
    auto& ns = registry().has<NativeScript>(root) ? registry().get<NativeScript>(root) : registry().emplace<NativeScript>(root);
    ns.scriptName = "Vehicle Driver";
    auto factory = ScriptRegistry::Get().create("Vehicle Driver");
    if (factory) {
        ns.instantiate = []() { return ScriptRegistry::Get().create("Vehicle Driver"); };
        ns.instance = ns.instantiate();
        if (ns.instance) {
            ns.instance->_init(root, &registry());
            ns.instance->onCreate();
        }
    }

    // 2. Wheels
    auto wheelTex = Assets::Texture("assets/textures/wheel.png", true);
    Material wheelMat = Material::Textured(wheelTex, {0.15f, 0.15f, 0.15f}, 0.1f, 0.9f);

    auto spawnWheel = [&](const std::string& name, const glm::vec3& offset) {
        auto wRef = scene().create(name, Transform{pos + offset, {0, 0, 0}, {0.6f, 0.6f, 0.35f}});
        MeshRenderer wMr(Assets::Sphere(0.35f), wheelMat);
        wMr.assetPath = "primitive:sphere";
        wRef.add<MeshRenderer>(wMr);
    };

    spawnWheel("Wheel_FL", {-1.1f, -0.2f, 1.3f});
    spawnWheel("Wheel_FR", { 1.1f, -0.2f, 1.3f});
    spawnWheel("Wheel_RL", {-1.1f, -0.2f, -1.3f});
    spawnWheel("Wheel_RR", { 1.1f, -0.2f, -1.3f});

    // 3. Headlights (dual point lights)
    spawnPointLight(pos + glm::vec3(-0.6f, 0.3f, 2.0f), {1.0f, 0.95f, 0.8f}, 5.0f, 15.0f);
    spawnPointLight(pos + glm::vec3( 0.6f, 0.3f, 2.0f), {1.0f, 0.95f, 0.8f}, 5.0f, 15.0f);

    m_selectedEntity = root;
    std::cout << "[SceneEditor] Assembled complete modular vehicle at pos: " << pos.x << ", " << pos.y << ", " << pos.z << "\n";
    return root;
}

void SceneEditor::createNewScriptFile(const std::string& scriptName) {
    std::filesystem::create_directories("assets/scripts");
    std::string filename = "assets/scripts/" + scriptName + ".h";
    std::ofstream out(filename);
    if (!out.is_open()) return;

    out << "#pragma once\n"
        << "#include \"engine/Scripting/ScriptableEntity.h\"\n"
        << "#include <imgui.h>\n"
        << "#include <iostream>\n\n"
        << "class " << scriptName << " : public ScriptableEntity {\n"
        << "public:\n"
        << "    float speed = 10.0f;\n"
        << "    bool active = true;\n\n"
        << "    void onStart() override {\n"
        << "        std::cout << \"[" << scriptName << "] Started!\\n\";\n"
        << "    }\n\n"
        << "    void onUpdate(float dt) override {\n"
        << "        if (!active) return;\n"
        << "        // Custom gameplay logic here\n"
        << "    }\n\n"
        << "    void onInspectorGUI() override {\n"
        << "        ImGui::Text(\"" << scriptName << " Settings\");\n"
        << "        ImGui::Checkbox(\"Active\", &active);\n"
        << "        ImGui::SliderFloat(\"Speed\", &speed, 0.0f, 100.0f);\n"
        << "    }\n"
        << "};\n"
        << "REGISTER_SCRIPT(" << scriptName << ", \"" << scriptName << "\")\n";
    out.close();
    std::cout << "[SceneEditor] Created user script template: " << filename << "\n";
}

void SceneEditor::renderAtmosphereEditor() {
    ImGui::SetNextWindowPos(ImVec2(1600 - 410, 48), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(395, 380), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Atmosphere & Sky", &m_showAtmosphereEditor)) {
        if (ImGui::Button("Sunset")) {
            if (auto v = registry().view<Sky>(); v.begin() != v.end()) registry().get<Sky>(*v.begin()) = Sky::Sunset();
        }
        ImGui::SameLine();
        if (ImGui::Button("Night Sky")) {
            if (auto v = registry().view<Sky>(); v.begin() != v.end()) registry().get<Sky>(*v.begin()) = Sky::Night();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Day")) {
            if (auto v = registry().view<Sky>(); v.begin() != v.end()) registry().get<Sky>(*v.begin()) = Sky::ClearDay();
        }

        if (auto v = registry().view<Sky>(); v.begin() != v.end()) {
            auto& sky = registry().get<Sky>(*v.begin());
            ImGui::SeparatorText("Sky Gradients");
            ImGui::ColorEdit3("Zenith (Top)", &sky.top.r);
            ImGui::ColorEdit3("Horizon", &sky.horizon.r);
            ImGui::ColorEdit3("Ground (Bottom)", &sky.bottom.r);
            ImGui::SliderFloat("Sky Exposure", &sky.exposure, 0.1f, 3.0f);
        }

        if (auto v = registry().view<DirectionalLight>(); v.begin() != v.end()) {
            auto& sun = registry().get<DirectionalLight>(*v.begin());
            ImGui::SeparatorText("Sun Directional Light");
            ImGui::DragFloat3("Sun Direction", &sun.direction.x, 0.02f, -1.0f, 1.0f);
            if (ImGui::Button("Normalize Sun Dir")) sun.direction = glm::normalize(sun.direction);
            ImGui::ColorEdit3("Sun Color", &sun.color.r);
            ImGui::SliderFloat("Sun Lux Intensity", &sun.intensity, 0.0f, 30.0f, "%.2f Lux");
        }

        if (auto v = registry().view<Fog>(); v.begin() != v.end()) {
            auto& fog = registry().get<Fog>(*v.begin());
            ImGui::SeparatorText("Atmospheric Distance Fog");
            ImGui::ColorEdit3("Fog Color", &fog.color.r);
            ImGui::SliderFloat("Fog Density", &fog.density, 0.0001f, 0.05f, "%.4f");
        }
    }
    ImGui::End();
}

void SceneEditor::renderGraphicsSettings() {
    ImGui::SetNextWindowPos(ImVec2(1600 - 410, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(395, 320), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Graphics, RTX & Post-Process", &m_showGraphicsSettings)) {
        if (m_pipe) {
            RenderPipeline::Settings s = m_pipe->settings();

            if (ImGui::CollapsingHeader("Ray Tracing & Reflections (SSR)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Screen-Space Reflections (SSR)", &s.ssr);
                if (s.ssr) {
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "SSR Hi-Z Raymarching Active (Roughness-Aware)");
                }
            }

            if (ImGui::CollapsingHeader("Ambient Occlusion (GTAO)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Ground-Truth Ambient Occlusion (GTAO)", &s.gtao);
                if (s.gtao) {
                    ImGui::SliderFloat("AO Radius (m)", &s.gtaoRadius, 0.1f, 5.0f, "%.2f m");
                    ImGui::SliderFloat("AO Intensity", &s.gtaoIntensity, 0.1f, 3.0f, "%.2f");
                }
            }

            if (ImGui::CollapsingHeader("Volumetric Fog & God Rays (Light Shafts)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Volumetric Atmospheric Fog", &s.volumetricFog);
                if (s.volumetricFog) {
                    ImGui::SliderFloat("Fog Density", &s.fogDensity, 0.0001f, 0.05f, "%.5f");
                    ImGui::SliderFloat("Height Falloff", &s.fogHeightFalloff, 0.01f, 2.0f);
                    ImGui::SliderFloat("Base Height", &s.fogHeight, -10.0f, 50.0f);
                }
                ImGui::Checkbox("Screen-Space Light Shafts (God Rays)", &s.lightShafts);
                if (s.lightShafts) {
                    ImGui::SliderFloat("Shafts Density", &s.shaftDensity, 0.1f, 1.0f);
                    ImGui::SliderFloat("Shafts Weight", &s.shaftWeight, 0.1f, 1.0f);
                }
            }

            if (ImGui::CollapsingHeader("HDR Bloom & ACES Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("HDR Rendering Pipeline", &s.hdr);
                ImGui::Checkbox("HDR Bloom", &s.bloom);
                if (s.bloom) {
                    ImGui::SliderFloat("Bloom Threshold", &s.bloomThreshold, 0.1f, 3.0f);
                    ImGui::SliderFloat("Bloom Intensity", &s.bloomIntensity, 0.0f, 2.0f);
                    ImGui::SliderInt("Blur Passes", &s.bloomBlurPasses, 1, 5);
                }
                ImGui::SliderFloat("Exposure EV", &s.exposure, 0.1f, 4.0f, "%.3f");
                ImGui::SliderFloat("Gamma", &s.gamma, 1.0f, 3.0f, "%.2f");
                ImGui::SliderFloat("Vignette", &s.vignette, 0.0f, 1.0f);
            }

            if (ImGui::CollapsingHeader("Anti-Aliasing (TAA / FXAA)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Temporal Anti-Aliasing (TAA)", &s.taa);
                ImGui::Checkbox("Fast Approximate AA (FXAA)", &s.fxaa);
            }

            if (ImGui::CollapsingHeader("Atmosphere Sky & Time of Day", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Button("Sunset")) {
                    if (auto v = registry().view<Sky>(); v.begin() != v.end()) registry().get<Sky>(*v.begin()) = Sky::Sunset();
                }
                ImGui::SameLine();
                if (ImGui::Button("Night Sky")) {
                    if (auto v = registry().view<Sky>(); v.begin() != v.end()) registry().get<Sky>(*v.begin()) = Sky::Night();
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear Day")) {
                    if (auto v = registry().view<Sky>(); v.begin() != v.end()) registry().get<Sky>(*v.begin()) = Sky::ClearDay();
                }

                if (auto v = registry().view<Sky>(); v.begin() != v.end()) {
                    auto& sky = registry().get<Sky>(*v.begin());
                    ImGui::ColorEdit3("Zenith (Top)", &sky.top.r);
                    ImGui::ColorEdit3("Horizon", &sky.horizon.r);
                    ImGui::ColorEdit3("Ground (Bottom)", &sky.bottom.r);
                    ImGui::SliderFloat("Sky Exposure", &sky.exposure, 0.1f, 3.0f);
                }

                if (auto v = registry().view<DirectionalLight>(); v.begin() != v.end()) {
                    auto& sun = registry().get<DirectionalLight>(*v.begin());
                    ImGui::DragFloat3("Sun Direction", &sun.direction.x, 0.02f, -1.0f, 1.0f);
                    if (ImGui::Button("Normalize Sun Dir")) sun.direction = glm::normalize(sun.direction);
                    ImGui::ColorEdit3("Sun Color", &sun.color.r);
                    ImGui::SliderFloat("Sun Lux Intensity", &sun.intensity, 0.0f, 30.0f, "%.2f Lux");
                }
            }

            m_pipe->setSettings(s);
        }
    }
    ImGui::End();
}

void SceneEditor::renderClusterLODSettings() {
    ImGui::SetNextWindowPos(ImVec2(1600 - 410, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(395, 290), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("ClusterLOD / Nanite Virtual Geometry", &m_showClusterLODSettings)) {
        static bool globalClusterLOD = true;
        static float errorThresholdPx = 2.0f;
        static int debugMode = 0;

        ImGui::Checkbox("Enable Virtualized ClusterLOD", &globalClusterLOD);
        ImGui::SliderFloat("Error Threshold (px)", &errorThresholdPx, 0.5f, 10.0f, "%.1f px");

        const char* debugModes[] = { "0: Normal PBR Shading", "1: Color-Coded LODs (L0=Green, L1=Blue, L2=Yellow, L3=Red)", "2: Cluster Wireframe" };
        ImGui::Combo("LOD Debug Visualizer", &debugMode, debugModes, IM_ARRAYSIZE(debugModes));

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.7f, 1.0f), "Nanite Architecture Statistics:");
        
        size_t totalMeshes = 0;
        size_t clusterLODMeshes = 0;
        for (Entity e : registry().view<MeshRenderer>()) {
            totalMeshes++;
            if (registry().get<MeshRenderer>(e).clusterLOD) clusterLODMeshes++;
        }

        ImGui::BulletText("Total Scene Meshes: %zu", totalMeshes);
        ImGui::BulletText("ClusterLOD Virtualized: %zu", clusterLODMeshes);
        ImGui::BulletText("Cluster Bucket Size: 128 Triangles/Cluster");
        ImGui::BulletText("Hierarchical LOD Levels: 4 discrete DAG levels");
        ImGui::BulletText("Culling: Sub-mesh Frustum + Screen Error");
        ImGui::BulletText("Rendering: GPU MultiDraw Instancing");
    }
    ImGui::End();
}

void SceneEditor::buildStandaloneGame() {
    std::cout << "[SceneEditor] Building standalone game executable...\n";
    if (m_isPlaying) {
        m_isPlaying = false;
        loadSceneFromFile("assets/.play_mode_backup.json");
        window().setCursorMode(GLFW_CURSOR_NORMAL);
    }

    saveSceneToFile("assets/custom_scene.json");
    if (std::string(m_sceneFileBuf) != "assets/custom_scene.json") {
        saveSceneToFile(m_sceneFileBuf);
    }

    // Ensure assets symlink exists in build/ so launching from build/ directory works
    std::system("mkdir -p build && ln -sfn ../assets build/assets");

    // Force touch StandaloneMain.cpp to ensure ninja ALWAYS rebuilds if needed
    std::system("touch game/src/StandaloneMain.cpp");

    int result = std::system("ninja -C build cjoka_standalone");
    m_buildSuccess = (result == 0);
    if (m_buildSuccess) {
        m_buildLog = "Standalone Game Build Succeeded!\n\n"
                     "Saved current scene to:\n"
                     "assets/custom_scene.json\n\n"
                     "Executable ready at:\n"
                     "./build/cjoka_standalone\n\n"
                     "All changes, colliders, hierarchy, scripts and lighting are updated in the build.";
    } else {
        m_buildLog = "Standalone Game Build Failed.\nCheck console output for compile errors.";
    }
    m_showBuildModal = true;
}

void SceneEditor::renderBuildModal() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - 240.0f, io.DisplaySize.y * 0.5f - 140.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(480.0f, 260.0f), ImGuiCond_Always);

    if (ImGui::Begin("Standalone Game Build", &m_showBuildModal, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        if (m_buildSuccess) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[SUCCESS] Build Standalone Game Complete!");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[ERROR] Build Failed");
        }
        ImGui::Separator();
        ImGui::TextWrapped("%s", m_buildLog.c_str());
        ImGui::Separator();

        if (m_buildSuccess) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("Run Standalone Game Now", ImVec2(220, 40))) {
                std::system("./build/cjoka_standalone &");
                m_showBuildModal = false;
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }

        if (ImGui::Button("Close", ImVec2(120, 40))) {
            m_showBuildModal = false;
        }
    }
    ImGui::End();
}

void SceneEditor::renderDeleteAssetModal() {
    if (!m_showDeleteAssetModal) return;

    ImGui::OpenPopup("Delete Asset Confirmation");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 200), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Delete Asset Confirmation", &m_showDeleteAssetModal, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "[WARNING] Permanent Deletion");
        ImGui::Separator();

        bool isDir = std::filesystem::is_directory(m_assetToDelete);
        ImGui::TextWrapped("Are you sure you want to permanently delete this %s?", isDir ? "directory and ALL its contents" : "file");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Target: %s", m_assetToDelete.string().c_str());
        ImGui::Spacing();
        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Yes, Delete Permanently", ImVec2(190, 32))) {
            std::error_code ec;
            if (isDir) {
                std::filesystem::remove_all(m_assetToDelete, ec);
            } else {
                std::filesystem::remove(m_assetToDelete, ec);
            }
            if (ec) {
                std::cerr << "[SceneEditor] Failed to delete: " << ec.message() << "\n";
            } else {
                std::cout << "[SceneEditor] Successfully deleted: " << m_assetToDelete << "\n";
            }
            if (m_selectedAssetPath == m_assetToDelete) {
                m_selectedAssetPath.clear();
            }
            m_assetToDelete.clear();
            m_showDeleteAssetModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 32))) {
            m_assetToDelete.clear();
            m_showDeleteAssetModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void SceneEditor::renderStats() {
    ImGui::SetNextWindowPos(ImVec2(330, 36), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240, 110), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Engine Stats", &m_showStats, ImGuiWindowFlags_NoResize)) {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
        ImGui::Text("Entities: %zu", registry().storage<Entity>().size());
        int w, h; window().getFramebufferSize(w, h);
        ImGui::Text("Resolution: %dx%d", w, h);
        ImGui::Text("Gizmo: %s (%s)", (m_gizmoOperation == 0 ? "Translate" : (m_gizmoOperation == 1 ? "Rotate" : "Scale")), (m_gizmoMode == 0 ? "Local" : "World"));
    }
    ImGui::End();
}

void SceneEditor::saveSceneToFile(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "[SceneEditor] Error: Failed to open file for saving: " << path << "\n";
        return;
    }

    file << "{\n";

    // 1. Atmosphere
    file << "  \"atmosphere\": {\n";
    if (auto v = registry().view<Sky>(); v.begin() != v.end()) {
        auto& sky = registry().get<Sky>(*v.begin());
        file << "    \"sky\": {\n";
        file << "      \"top\": [" << sky.top.r << ", " << sky.top.g << ", " << sky.top.b << "],\n";
        file << "      \"horizon\": [" << sky.horizon.r << ", " << sky.horizon.g << ", " << sky.horizon.b << "],\n";
        file << "      \"bottom\": [" << sky.bottom.r << ", " << sky.bottom.g << ", " << sky.bottom.b << "],\n";
        file << "      \"exposure\": " << sky.exposure << "\n";
        file << "    },\n";
    }
    if (auto v = registry().view<Fog>(); v.begin() != v.end()) {
        auto& fog = registry().get<Fog>(*v.begin());
        file << "    \"fog\": {\n";
        file << "      \"color\": [" << fog.color.r << ", " << fog.color.g << ", " << fog.color.b << "],\n";
        file << "      \"density\": " << fog.density << "\n";
        file << "    },\n";
    }
    if (auto v = registry().view<DirectionalLight>(); v.begin() != v.end()) {
        auto& sun = registry().get<DirectionalLight>(*v.begin());
        file << "    \"sun\": {\n";
        file << "      \"dir\": [" << sun.direction.x << ", " << sun.direction.y << ", " << sun.direction.z << "],\n";
        file << "      \"color\": [" << sun.color.r << ", " << sun.color.g << ", " << sun.color.b << "],\n";
        file << "      \"intensity\": " << sun.intensity << "\n";
        file << "    },\n";
    }
    if (auto v = registry().view<AmbientLight>(); v.begin() != v.end()) {
        auto& amb = registry().get<AmbientLight>(*v.begin());
        file << "    \"ambient\": {\n";
        file << "      \"color\": [" << amb.color.r << ", " << amb.color.g << ", " << amb.color.b << "],\n";
        file << "      \"intensity\": " << amb.intensity << "\n";
        file << "    },\n";
    }
    if (auto v = registry().view<PostProcessSettings>(); v.begin() != v.end()) {
        auto& pp = registry().get<PostProcessSettings>(*v.begin());
        file << "    \"post\": {\n";
        file << "      \"bloomThreshold\": " << pp.bloomThreshold << ",\n";
        file << "      \"bloomIntensity\": " << pp.bloomIntensity << ",\n";
        file << "      \"exposure\": " << pp.exposure << ",\n";
        file << "      \"vignette\": " << pp.vignette << "\n";
        file << "    }\n";
    }
    file << "  },\n";

    // 2. Entities
    file << "  \"entities\": [\n";
    bool first = true;
    for (Entity e : registry().view<Transform>()) {
        if (registry().has<Sky>(e) || registry().has<Fog>(e) || registry().has<DirectionalLight>(e) ||
            registry().has<AmbientLight>(e) || registry().has<PostProcessSettings>(e)) continue;

        if (!first) file << ",\n";
        first = false;

        auto& tr = registry().get<Transform>(e);
        std::string name = registry().has<Name>(e) ? registry().get<Name>(e).value : "Entity";

        file << "    {\n";
        file << "      \"name\": \"" << name << "\",\n";
        file << "      \"pos\": [" << tr.position.x << ", " << tr.position.y << ", " << tr.position.z << "],\n";
        file << "      \"rot\": [" << tr.rotation.x << ", " << tr.rotation.y << ", " << tr.rotation.z << "],\n";
        file << "      \"scale\": [" << tr.scale.x << ", " << tr.scale.y << ", " << tr.scale.z << "]";

        if (registry().has<Camera>(e)) {
            auto& cam = registry().get<Camera>(e);
            file << ",\n      \"camera\": {\n";
            file << "        \"fov\": " << cam.fov << ",\n";
            file << "        \"near\": " << cam.nearPlane << ",\n";
            file << "        \"far\": " << cam.farPlane << ",\n";
            file << "        \"primary\": " << (cam.primary ? "true" : "false") << ",\n";
            file << "        \"perspective\": " << (cam.perspective ? "true" : "false") << "\n";
            file << "      }";
        }

        if (registry().has<MeshRenderer>(e)) {
            auto& mr = registry().get<MeshRenderer>(e);
            file << ",\n      \"mesh\": {\n";
            file << "        \"assetPath\": \"" << mr.assetPath << "\",\n";
            file << "        \"materialPath\": \"" << mr.material.materialPath << "\",\n";
            file << "        \"diffusePath\": \"" << mr.material.diffuseMapPath << "\",\n";
            file << "        \"normalPath\": \"" << mr.material.normalMapPath << "\",\n";
            file << "        \"specularPath\": \"" << mr.material.specularMapPath << "\",\n";
            file << "        \"texturePath\": \"" << mr.texturePath << "\",\n";
            file << "        \"albedo\": [" << mr.material.albedo.r << ", " << mr.material.albedo.g << ", " << mr.material.albedo.b << "],\n";
            file << "        \"metallic\": " << mr.material.metallic << ",\n";
            file << "        \"roughness\": " << mr.material.roughness << ",\n";
            file << "        \"ao\": " << mr.material.ao << ",\n";
            file << "        \"emissive\": [" << mr.material.emissive.r << ", " << mr.material.emissive.g << ", " << mr.material.emissive.b << "],\n";
            file << "        \"clusterLOD\": " << (mr.clusterLOD ? "true" : "false") << ",\n";
            file << "        \"castShadow\": " << (mr.castShadow ? "true" : "false") << "\n";
            file << "      }";
        }

        if (registry().has<PointLight>(e)) {
            auto& pl = registry().get<PointLight>(e);
            file << ",\n      \"light\": {\n";
            file << "        \"color\": [" << pl.color.r << ", " << pl.color.g << ", " << pl.color.b << "],\n";
            file << "        \"intensity\": " << pl.intensity << ",\n";
            file << "        \"range\": " << pl.range << "\n";
            file << "      }";
        }

        if (registry().has<CharacterController>(e)) {
            auto& cc = registry().get<CharacterController>(e);
            file << ",\n      \"characterController\": {\n";
            file << "        \"radius\": " << cc.radius << ",\n";
            file << "        \"height\": " << cc.height << ",\n";
            file << "        \"speed\": " << cc.speed << ",\n";
            file << "        \"jumpForce\": " << cc.jumpForce << "\n";
            file << "      }";
        }

        if (registry().has<Hierarchy>(e)) {
            auto& h = registry().get<Hierarchy>(e);
            if (registry().valid(h.parent) && registry().has<Name>(h.parent)) {
                file << ",\n      \"parent\": \"" << registry().get<Name>(h.parent).value << "\"";
            }
        }

        if (registry().has<cjoka_phys::Collider>(e)) {
            auto& col = registry().get<cjoka_phys::Collider>(e);
            file << ",\n      \"collider\": {\n";
            file << "        \"type\": " << (int)col.type << ",\n";
            file << "        \"halfExtents\": [" << col.halfExtents.x << ", " << col.halfExtents.y << ", " << col.halfExtents.z << "],\n";
            file << "        \"radius\": " << col.radius << ",\n";
            file << "        \"height\": " << col.height << ",\n";
            file << "        \"offset\": [" << col.centerOffset.x << ", " << col.centerOffset.y << ", " << col.centerOffset.z << "]\n";
            file << "      }";
        }

        if (registry().has<NativeScript>(e)) {
            auto& ns = registry().get<NativeScript>(e);
            file << ",\n      \"scriptName\": \"" << ns.scriptName << "\"";
        }

        file << "\n    }";
    }
    file << "\n  ]\n}\n";
    file.close();
    std::cout << "[SceneEditor] Successfully saved full scene to " << path << "\n";
}

void SceneEditor::loadSceneFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "[SceneEditor] Could not open " << path << ", loading default showcase\n";
        newScene();
        return;
    }

    scene().clear();
    m_selectedEntity = NullEntity;

    // Default atmosphere
    scene().create("Sky").add<Sky>(Sky::Sunset());
    scene().create("Fog").add<Fog>(Fog{{0.2f, 0.22f, 0.28f}, 0.0035f});
    scene().create("Sun").add<DirectionalLight>(DirectionalLight{glm::normalize(glm::vec3{-0.4f, -0.8f, -0.3f}), {1.0f, 0.92f, 0.82f}, 2.5f});
    scene().create("Ambient").add<AmbientLight>(AmbientLight{{0.12f, 0.14f, 0.18f}, 1.0f});
    scene().create("PostProcess").add<PostProcessSettings>(PostProcessSettings::Cinematic());

    std::string line;
    std::string currentName = "";
    glm::vec3 pos{0.0f}, rot{0.0f}, scale{1.0f};
    bool hasTransform = false;

    bool hasMesh = false;
    std::string assetPath = "";
    std::string materialPath = "";
    std::string diffusePath = "";
    std::string normalPath = "";
    std::string specularPath = "";
    std::string texturePath = "";
    glm::vec3 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    glm::vec3 emissive{0.0f};
    bool clusterLOD = false;
    bool castShadow = true;

    bool hasLight = false;
    glm::vec3 lightCol{1.0f};
    float lightInt = 5.0f;
    float lightRange = 15.0f;

    bool hasCamera = false;
    float camFovVal = 65.0f;
    float camNearVal = 0.1f;
    float camFarVal = 1000.0f;
    bool camPrimaryVal = true;
    bool camPerspVal = true;

    bool hasScript = false;
    std::string scriptName = "";

    bool hasCC = false;
    float ccRadius = 0.4f;
    float ccHeight = 1.8f;
    float ccSpeed = 8.0f;
    float ccJump = 5.0f;

    bool hasCol = false;
    int colType = 0;
    glm::vec3 colHalfExtents{0.5f};
    float colRadius = 0.5f;
    float colHeight = 1.0f;
    glm::vec3 colOffset{0.0f};
    std::string parentName = "";
    std::vector<std::pair<Entity, std::string>> pendingParents;

    auto instantiateEntity = [&]() {
        if (currentName.empty() && !hasTransform) return;
        std::string entName = currentName.empty() ? "Entity" : currentName;
        Transform tr{pos, rot, scale};
        auto ref = scene().create(entName, tr);
        Entity e = ref.id();

        // 1. Camera Component
        if (hasCamera) {
            ref.add<Camera>(Camera{camFovVal, camNearVal, camFarVal, camPrimaryVal, camPerspVal, 10.0f});
        }

        // 2. Mesh Renderer & PBR Material
        if (hasMesh || !assetPath.empty()) {
            Material mat;
            if (!materialPath.empty() && std::filesystem::exists(materialPath)) {
                loadMaterialFromFile(materialPath, mat);
            } else {
                mat.albedo = albedo;
                mat.metallic = metallic;
                mat.roughness = roughness;
                mat.ao = ao;
                mat.emissive = emissive;
            }
            if (!diffusePath.empty()) {
                mat.diffuseMapPath = diffusePath;
                mat.diffuseMap = Assets::Texture(diffusePath, true);
                mat.useDiffuseMap = (mat.diffuseMap && mat.diffuseMap->valid());
            } else if (!texturePath.empty()) {
                mat.diffuseMapPath = texturePath;
                mat.diffuseMap = Assets::Texture(texturePath, true);
                mat.useDiffuseMap = (mat.diffuseMap && mat.diffuseMap->valid());
            }
            if (!normalPath.empty()) {
                mat.normalMapPath = normalPath;
                mat.normalMap = Assets::Texture(normalPath, true);
                mat.useNormalMap = (mat.normalMap && mat.normalMap->valid());
            }
            if (!specularPath.empty()) {
                mat.specularMapPath = specularPath;
                mat.specularMap = Assets::Texture(specularPath, true);
                mat.useSpecularMap = (mat.specularMap && mat.specularMap->valid());
            }

            std::shared_ptr<Mesh3D> mesh;
            if (assetPath.find(".obj") != std::string::npos) {
                mesh = Assets::Mesh(assetPath);
            } else if (assetPath.find("sphere") != std::string::npos) {
                mesh = Assets::Sphere(0.5f);
            } else {
                mesh = Assets::Cube(1.0f);
            }

            MeshRenderer mr(mesh, mat);
            mr.assetPath = assetPath;
            mr.texturePath = mat.diffuseMapPath;
            mr.setClusterLOD(clusterLOD);
            mr.setCastShadow(castShadow);
            ref.add<MeshRenderer>(mr);
        } else if (hasLight) {
            // Pure light: add a small visible emissive indicator
            Material orbMat = Material::Emissive(lightCol, lightInt * 2.0f);
            MeshRenderer mr(Assets::Sphere(0.15f), orbMat);
            mr.assetPath = "primitive:sphere";
            mr.setClusterLOD(false);
            ref.add<MeshRenderer>(mr);
        }

        // 3. Point Light
        if (hasLight) {
            ref.add<PointLight>(PointLight{lightCol, lightInt, lightRange});
        }

        // 4. Native Script
        if (hasScript && !scriptName.empty()) {
            auto factory = ScriptRegistry::Get().create(scriptName);
            if (factory) {
                auto& ns = ref.add<NativeScript>();
                ns.scriptName = scriptName;
                ns.instantiate = [scriptName]() { return ScriptRegistry::Get().create(scriptName); };
                ns.instance = ns.instantiate();
                if (ns.instance) {
                    ns.instance->_init(e, &registry());
                    ns.instance->onCreate();
                }
            }
        }

        // 5. Character Controller
        if (hasCC) {
            auto& cc = ref.add<CharacterController>();
            cc.radius = ccRadius;
            cc.height = ccHeight;
            cc.speed = ccSpeed;
            cc.jumpForce = ccJump;
        }

        // 6. PhysX Collider
        if (hasCol) {
            auto& col = ref.add<cjoka_phys::Collider>();
            col.type = (cjoka_phys::ColliderType)colType;
            col.halfExtents = colHalfExtents;
            col.radius = colRadius;
            col.height = colHeight;
            col.centerOffset = colOffset;
        }

        // 7. Hierarchy / Parent
        if (!parentName.empty()) {
            pendingParents.push_back({e, parentName});
        }

        // Reset state
        currentName = "";
        hasTransform = false;
        hasMesh = false;
        assetPath = "";
        materialPath = "";
        diffusePath = "";
        normalPath = "";
        specularPath = "";
        texturePath = "";
        albedo = {1.0f, 1.0f, 1.0f};
        metallic = 0.0f;
        roughness = 0.5f;
        ao = 1.0f;
        emissive = {0.0f, 0.0f, 0.0f};
        hasLight = false;
        hasCamera = false;
        camFovVal = 65.0f;
        camNearVal = 0.1f;
        camFarVal = 1000.0f;
        camPrimaryVal = true;
        camPerspVal = true;
        hasScript = false;
        scriptName = "";
        hasCC = false;
        ccRadius = 0.4f;
        ccHeight = 1.8f;
        ccSpeed = 8.0f;
        ccJump = 5.0f;
        hasCol = false;
        colType = 0;
        colHalfExtents = glm::vec3(0.5f);
        colRadius = 0.5f;
        colHeight = 1.0f;
        colOffset = glm::vec3(0.0f);
        parentName = "";
    };

    bool inSky = false, inFog = false, inSun = false, inAmbient = false, inPost = false;

    while (std::getline(file, line)) {
        if (line.find("\"sky\":") != std::string::npos) { inSky = true; inFog = inSun = inAmbient = inPost = false; continue; }
        if (line.find("\"fog\":") != std::string::npos) { inFog = true; inSky = inSun = inAmbient = inPost = false; continue; }
        if (line.find("\"sun\":") != std::string::npos) { inSun = true; inSky = inFog = inAmbient = inPost = false; continue; }
        if (line.find("\"ambient\":") != std::string::npos) { inAmbient = true; inSky = inFog = inSun = inPost = false; continue; }
        if (line.find("\"post\":") != std::string::npos) { inPost = true; inSky = inFog = inSun = inAmbient = false; continue; }
        if (line.find("\"entities\":") != std::string::npos) { inSky = inFog = inSun = inAmbient = inPost = false; continue; }

        if (inSky) {
            if (auto v = registry().view<Sky>(); v.begin() != v.end()) {
                auto& sky = registry().get<Sky>(*v.begin());
                if (line.find("\"top\":") != std::string::npos) sscanf(line.c_str(), "%*[^[][%f, %f, %f", &sky.top.r, &sky.top.g, &sky.top.b);
                else if (line.find("\"horizon\":") != std::string::npos) sscanf(line.c_str(), "%*[^[][%f, %f, %f", &sky.horizon.r, &sky.horizon.g, &sky.horizon.b);
                else if (line.find("\"bottom\":") != std::string::npos) sscanf(line.c_str(), "%*[^[][%f, %f, %f", &sky.bottom.r, &sky.bottom.g, &sky.bottom.b);
                else if (line.find("\"exposure\":") != std::string::npos) sscanf(line.c_str(), "%*[^:]: %f", &sky.exposure);
            }
            continue;
        } else if (inFog) {
            if (auto v = registry().view<Fog>(); v.begin() != v.end()) {
                auto& fog = registry().get<Fog>(*v.begin());
                if (line.find("\"color\":") != std::string::npos) sscanf(line.c_str(), "%*[^[][%f, %f, %f", &fog.color.r, &fog.color.g, &fog.color.b);
                else if (line.find("\"density\":") != std::string::npos) sscanf(line.c_str(), "%*[^:]: %f", &fog.density);
            }
            continue;
        } else if (inSun) {
            if (auto v = registry().view<DirectionalLight>(); v.begin() != v.end()) {
                auto& sun = registry().get<DirectionalLight>(*v.begin());
                if (line.find("\"dir\":") != std::string::npos) sscanf(line.c_str(), "%*[^[][%f, %f, %f", &sun.direction.x, &sun.direction.y, &sun.direction.z);
                else if (line.find("\"color\":") != std::string::npos) sscanf(line.c_str(), "%*[^[][%f, %f, %f", &sun.color.r, &sun.color.g, &sun.color.b);
                else if (line.find("\"intensity\":") != std::string::npos) sscanf(line.c_str(), "%*[^:]: %f", &sun.intensity);
            }
            continue;
        } else if (inAmbient) {
            if (auto v = registry().view<AmbientLight>(); v.begin() != v.end()) {
                auto& amb = registry().get<AmbientLight>(*v.begin());
                if (line.find("\"color\":") != std::string::npos) sscanf(line.c_str(), "%*[^[][%f, %f, %f", &amb.color.r, &amb.color.g, &amb.color.b);
                else if (line.find("\"intensity\":") != std::string::npos) sscanf(line.c_str(), "%*[^:]: %f", &amb.intensity);
            }
            continue;
        } else if (inPost) {
            if (auto v = registry().view<PostProcessSettings>(); v.begin() != v.end()) {
                auto& pp = registry().get<PostProcessSettings>(*v.begin());
                if (line.find("\"bloomThreshold\":") != std::string::npos) sscanf(line.c_str(), "%*[^:]: %f", &pp.bloomThreshold);
                else if (line.find("\"bloomIntensity\":") != std::string::npos) sscanf(line.c_str(), "%*[^:]: %f", &pp.bloomIntensity);
                else if (line.find("\"exposure\":") != std::string::npos) sscanf(line.c_str(), "%*[^:]: %f", &pp.exposure);
                else if (line.find("\"vignette\":") != std::string::npos) sscanf(line.c_str(), "%*[^:]: %f", &pp.vignette);
            }
            continue;
        }

        if (line.find("\"name\":") != std::string::npos && line.find("\"script\":") == std::string::npos && line.find("\"scriptName\":") == std::string::npos) {
            instantiateEntity();
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            currentName = line.substr(start, end - start);
            hasTransform = true;
        } else if (line.find("\"pos\":") != std::string::npos) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &pos.x, &pos.y, &pos.z);
        } else if (line.find("\"rot\":") != std::string::npos) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &rot.x, &rot.y, &rot.z);
        } else if (line.find("\"scale\":") != std::string::npos) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &scale.x, &scale.y, &scale.z);
        } else if (line.find("\"camera\":") != std::string::npos) {
            hasCamera = true;
        } else if (line.find("\"fov\":") != std::string::npos && hasCamera) {
            sscanf(line.c_str(), "%*[^:]: %f", &camFovVal);
        } else if (line.find("\"near\":") != std::string::npos && hasCamera) {
            sscanf(line.c_str(), "%*[^:]: %f", &camNearVal);
        } else if (line.find("\"far\":") != std::string::npos && hasCamera) {
            sscanf(line.c_str(), "%*[^:]: %f", &camFarVal);
        } else if (line.find("\"primary\":") != std::string::npos && hasCamera) {
            camPrimaryVal = (line.find("true") != std::string::npos);
        } else if (line.find("\"perspective\":") != std::string::npos && hasCamera) {
            camPerspVal = (line.find("true") != std::string::npos);
        } else if (line.find("\"mesh\":") != std::string::npos) {
            hasMesh = true;
        } else if (line.find("\"assetPath\":") != std::string::npos) {
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            assetPath = line.substr(start, end - start);
            hasMesh = true;
        } else if (line.find("\"materialPath\":") != std::string::npos) {
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            materialPath = line.substr(start, end - start);
        } else if (line.find("\"diffusePath\":") != std::string::npos) {
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            diffusePath = line.substr(start, end - start);
        } else if (line.find("\"normalPath\":") != std::string::npos) {
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            normalPath = line.substr(start, end - start);
        } else if (line.find("\"specularPath\":") != std::string::npos) {
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            specularPath = line.substr(start, end - start);
        } else if (line.find("\"texturePath\":") != std::string::npos) {
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            texturePath = line.substr(start, end - start);
        } else if (line.find("\"albedo\":") != std::string::npos) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &albedo.r, &albedo.g, &albedo.b);
        } else if (line.find("\"metallic\":") != std::string::npos) {
            sscanf(line.c_str(), "%*[^:]: %f", &metallic);
        } else if (line.find("\"roughness\":") != std::string::npos) {
            sscanf(line.c_str(), "%*[^:]: %f", &roughness);
        } else if (line.find("\"ao\":") != std::string::npos) {
            sscanf(line.c_str(), "%*[^:]: %f", &ao);
        } else if (line.find("\"emissive\":") != std::string::npos) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &emissive.r, &emissive.g, &emissive.b);
        } else if (line.find("\"clusterLOD\":") != std::string::npos) {
            clusterLOD = (line.find("true") != std::string::npos);
        } else if (line.find("\"castShadow\":") != std::string::npos) {
            castShadow = (line.find("true") != std::string::npos);
        } else if (line.find("\"light\":") != std::string::npos) {
            hasLight = true;
        } else if (line.find("\"color\":") != std::string::npos && hasLight) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &lightCol.r, &lightCol.g, &lightCol.b);
        } else if (line.find("\"intensity\":") != std::string::npos && hasLight) {
            sscanf(line.c_str(), "%*[^:]: %f", &lightInt);
        } else if (line.find("\"range\":") != std::string::npos && hasLight) {
            sscanf(line.c_str(), "%*[^:]: %f", &lightRange);
        } else if (line.find("\"characterController\":") != std::string::npos) {
            hasCC = true;
        } else if (line.find("\"radius\":") != std::string::npos && hasCC) {
            sscanf(line.c_str(), "%*[^:]: %f", &ccRadius);
        } else if (line.find("\"height\":") != std::string::npos && hasCC) {
            sscanf(line.c_str(), "%*[^:]: %f", &ccHeight);
        } else if (line.find("\"speed\":") != std::string::npos && hasCC) {
            sscanf(line.c_str(), "%*[^:]: %f", &ccSpeed);
        } else if (line.find("\"jumpForce\":") != std::string::npos && hasCC) {
            sscanf(line.c_str(), "%*[^:]: %f", &ccJump);
        } else if (line.find("\"collider\":") != std::string::npos) {
            hasCol = true;
        } else if (line.find("\"type\":") != std::string::npos && hasCol) {
            sscanf(line.c_str(), "%*[^:]: %d", &colType);
        } else if (line.find("\"halfExtents\":") != std::string::npos && hasCol) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &colHalfExtents.x, &colHalfExtents.y, &colHalfExtents.z);
        } else if (line.find("\"radius\":") != std::string::npos && hasCol) {
            sscanf(line.c_str(), "%*[^:]: %f", &colRadius);
        } else if (line.find("\"height\":") != std::string::npos && hasCol) {
            sscanf(line.c_str(), "%*[^:]: %f", &colHeight);
        } else if (line.find("\"offset\":") != std::string::npos && hasCol) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &colOffset.x, &colOffset.y, &colOffset.z);
        } else if (line.find("\"parent\":") != std::string::npos) {
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            parentName = line.substr(start, end - start);
        } else if (line.find("\"scriptName\":") != std::string::npos) {
            hasScript = true;
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            scriptName = line.substr(start, end - start);
        } else if (line.find("\"script\":") != std::string::npos) {
            hasScript = true;
        }
    }
    instantiateEntity();

    // Resolve parent-child relationships
    for (auto& [child, pName] : pendingParents) {
        for (Entity p : registry().view<Name>()) {
            if (registry().get<Name>(p).value == pName && p != child) {
                auto& h = registry().has<Hierarchy>(child) ? registry().get<Hierarchy>(child) : registry().emplace<Hierarchy>(child);
                h.parent = p;
                break;
            }
        }
    }

    // Ensure at least one game camera exists
    bool foundCam = false;
    for (Entity e : registry().view<Camera>()) {
        (void)e;
        foundCam = true;
        break;
    }
    if (!foundCam) {
        auto camRef = scene().create("MainCamera", Transform{{0.0f, 3.5f, -12.0f}, {-10.0f, 0.0f, 0.0f}, glm::vec3(1.0f)});
        camRef.add<Camera>(Camera{65.0f, 0.1f, 1000.0f, true, true, 10.0f});
    }

    std::cout << "[SceneEditor] Successfully loaded scene from " << path << "\n";
}

void SceneEditor::applyLayoutPreset(int presetId) {
    if (presetId == 0) {
        // Studio Default
        m_showHierarchy = true;
        m_showInspector = true;
        m_showAssetBrowser = true;
        m_showMaterialPalette = false;
        m_showAtmosphereEditor = false;
        m_showGraphicsSettings = false;
        m_showClusterLODSettings = false;
        m_showStats = false;
        m_showCameraPreview = true;
        m_showColliders = true;
    } else if (presetId == 1) {
        // Level Design & World Building
        m_showHierarchy = true;
        m_showInspector = true;
        m_showAssetBrowser = true;
        m_showAtmosphereEditor = true;
        m_showMaterialPalette = true;
        m_showGraphicsSettings = false;
        m_showClusterLODSettings = false;
        m_showStats = false;
        m_showCameraPreview = true;
        m_showColliders = true;
    } else if (presetId == 2) {
        // Material & Shading Artist
        m_showHierarchy = false;
        m_showInspector = true;
        m_showAssetBrowser = true;
        m_showMaterialPalette = true;
        m_showAtmosphereEditor = true;
        m_showGraphicsSettings = true;
        m_showClusterLODSettings = false;
        m_showStats = false;
        m_showCameraPreview = true;
        m_showColliders = false;
    } else if (presetId == 3) {
        // Game Testing / Minimal HUD
        m_showHierarchy = false;
        m_showInspector = false;
        m_showAssetBrowser = false;
        m_showMaterialPalette = false;
        m_showAtmosphereEditor = false;
        m_showGraphicsSettings = false;
        m_showClusterLODSettings = false;
        m_showStats = true;
        m_showCameraPreview = false;
        m_showColliders = false;
    }
}

void SceneEditor::loadMaterialFromFile(const std::string& path, Material& outMat) {
    std::ifstream file(path);
    if (!file.is_open()) return;
    outMat.materialPath = path;
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("\"albedo\":") != std::string::npos) sscanf(line.c_str(), "%*[^[][%f, %f, %f", &outMat.albedo.r, &outMat.albedo.g, &outMat.albedo.b);
        else if (line.find("\"metallic\":") != std::string::npos) sscanf(line.c_str(), "%*[^:]: %f", &outMat.metallic);
        else if (line.find("\"roughness\":") != std::string::npos) sscanf(line.c_str(), "%*[^:]: %f", &outMat.roughness);
        else if (line.find("\"ao\":") != std::string::npos) sscanf(line.c_str(), "%*[^:]: %f", &outMat.ao);
        else if (line.find("\"emissive\":") != std::string::npos) sscanf(line.c_str(), "%*[^[][%f, %f, %f", &outMat.emissive.r, &outMat.emissive.g, &outMat.emissive.b);
        else if (line.find("\"diffuseMap\":") != std::string::npos || line.find("\"diffusePath\":") != std::string::npos) {
            size_t s = line.find("\"", line.find(":") + 1) + 1;
            size_t e = line.find("\"", s);
            outMat.diffuseMapPath = line.substr(s, e - s);
            if (!outMat.diffuseMapPath.empty() && std::filesystem::exists(outMat.diffuseMapPath)) {
                outMat.diffuseMap = Assets::Texture(outMat.diffuseMapPath, true);
                outMat.useDiffuseMap = (outMat.diffuseMap && outMat.diffuseMap->valid());
            } else {
                outMat.diffuseMap = nullptr;
                outMat.useDiffuseMap = false;
            }
        }
        else if (line.find("\"normalMap\":") != std::string::npos || line.find("\"normalPath\":") != std::string::npos) {
            size_t s = line.find("\"", line.find(":") + 1) + 1;
            size_t e = line.find("\"", s);
            outMat.normalMapPath = line.substr(s, e - s);
            if (!outMat.normalMapPath.empty() && std::filesystem::exists(outMat.normalMapPath)) {
                outMat.normalMap = Assets::Texture(outMat.normalMapPath, true);
                outMat.useNormalMap = (outMat.normalMap && outMat.normalMap->valid());
            } else {
                outMat.normalMap = nullptr;
                outMat.useNormalMap = false;
            }
        }
        else if (line.find("\"specularMap\":") != std::string::npos || line.find("\"specularPath\":") != std::string::npos) {
            size_t s = line.find("\"", line.find(":") + 1) + 1;
            size_t e = line.find("\"", s);
            outMat.specularMapPath = line.substr(s, e - s);
            if (!outMat.specularMapPath.empty() && std::filesystem::exists(outMat.specularMapPath)) {
                outMat.specularMap = Assets::Texture(outMat.specularMapPath, true);
                outMat.useSpecularMap = (outMat.specularMap && outMat.specularMap->valid());
            } else {
                outMat.specularMap = nullptr;
                outMat.useSpecularMap = false;
            }
        }
    }
}

void SceneEditor::saveMaterialToFile(const std::string& path, const Material& mat) {
    std::ofstream file(path);
    if (!file.is_open()) return;
    file << "{\n";
    file << "  \"name\": \"" << std::filesystem::path(path).stem().string() << "\",\n";
    file << "  \"albedo\": [" << mat.albedo.r << ", " << mat.albedo.g << ", " << mat.albedo.b << "],\n";
    file << "  \"metallic\": " << mat.metallic << ",\n";
    file << "  \"roughness\": " << mat.roughness << ",\n";
    file << "  \"ao\": " << mat.ao << ",\n";
    file << "  \"emissive\": [" << mat.emissive.r << ", " << mat.emissive.g << ", " << mat.emissive.b << "],\n";
    file << "  \"diffuseMap\": \"" << mat.diffuseMapPath << "\",\n";
    file << "  \"normalMap\": \"" << mat.normalMapPath << "\",\n";
    file << "  \"specularMap\": \"" << mat.specularMapPath << "\"\n";
    file << "}\n";
    file.close();
}

void SceneEditor::applyMaterialToEntity(Entity e, const std::string& matPath) {
    if (!registry().valid(e) || !registry().has<MeshRenderer>(e)) return;
    auto& mr = registry().get<MeshRenderer>(e);
    loadMaterialFromFile(matPath, mr.material);
}

void SceneEditor::createNewMaterialFile(const std::string& matName, const Material& templateMat) {
    std::string safeName = matName.empty() ? "NewMaterial" : matName;
    std::string path = (m_currentDirectory / (safeName + ".mat.json")).string();
    saveMaterialToFile(path, templateMat);
    m_selectedAssetPath = path;
    m_selectedMaterialFile = path;
    m_editingMaterial = templateMat;
}

void SceneEditor::createNewFolder(const std::string& folderName) {
    std::string safeName = folderName.empty() ? "NewFolder" : folderName;
    auto p = m_currentDirectory / safeName;
    std::filesystem::create_directories(p);
}

void SceneEditor::createNewSceneFile(const std::string& sceneName) {
    std::string safeName = sceneName.empty() ? "NewScene" : sceneName;
    auto p = m_currentDirectory / (safeName + ".json");
    saveSceneToFile(p.string());
    m_selectedAssetPath = p;
}

void SceneEditor::renderMaterialPalette() {
    ImGui::SetNextWindowPos(ImVec2(16, 520), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 350), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Material Palette / PBR Library", &m_showMaterialPalette)) {
        ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "PBR Material Library");
        ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);
        if (ImGui::Button("[+] New Material", ImVec2(140, 24))) {
            m_createAssetType = 0;
            strncpy(m_createAssetNameBuf, "CustomMaterial", sizeof(m_createAssetNameBuf));
            m_showCreateAssetModal = true;
        }

        ImGui::Separator();

        // Scan materials folder
        std::vector<std::string> matFiles;
        if (std::filesystem::exists("assets/materials")) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator("assets/materials")) {
                if (entry.is_regular_file() && (entry.path().extension() == ".json" || entry.path().string().find(".mat") != std::string::npos)) {
                    matFiles.push_back(entry.path().string());
                }
            }
        }
        std::sort(matFiles.begin(), matFiles.end());

        ImGui::BeginChild("MaterialGrid", ImVec2(0, 160), true);
        float cardWidth = 115.0f;
        float windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

        for (size_t i = 0; i < matFiles.size(); ++i) {
            const auto& p = matFiles[i];
            std::string stem = std::filesystem::path(p).stem().string();
            if (stem.find(".mat") != std::string::npos) stem = std::filesystem::path(stem).stem().string();

            Material tempMat;
            loadMaterialFromFile(p, tempMat);

            ImVec4 swatchCol = ImVec4(tempMat.albedo.r * 0.7f + 0.1f, tempMat.albedo.g * 0.7f + 0.1f, tempMat.albedo.b * 0.7f + 0.1f, 1.0f);

            ImGui::PushID((int)i);
            ImGui::BeginGroup();
            // Color Swatch Button
            ImGui::PushStyleColor(ImGuiCol_Button, swatchCol);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(swatchCol.x * 1.2f, swatchCol.y * 1.2f, swatchCol.z * 1.2f, 1.0f));
            if (ImGui::Button(("##swatch_" + stem).c_str(), ImVec2(cardWidth, 36))) {
                m_selectedMaterialFile = p;
                m_editingMaterial = tempMat;
                if (registry().valid(m_selectedEntity) && registry().has<MeshRenderer>(m_selectedEntity)) {
                    applyMaterialToEntity(m_selectedEntity, p);
                }
            }
            ImGui::PopStyleColor(2);

            // DragDrop Source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("MATERIAL_PATH", p.c_str(), p.size() + 1);
                ImGui::Text("Material: %s", stem.c_str());
                ImGui::EndDragDropSource();
            }

            ImGui::TextWrapped("%s", stem.c_str());
            ImGui::TextDisabled("R:%.2f M:%.2f", tempMat.roughness, tempMat.metallic);
            if (tempMat.useNormalMap) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "[NRM]");
            }
            ImGui::EndGroup();

            // Right-click context menu on material card
            if (ImGui::BeginPopupContextItem()) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Material: %s", stem.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem("Apply to Selected Object")) {
                    if (registry().valid(m_selectedEntity) && registry().has<MeshRenderer>(m_selectedEntity)) {
                        applyMaterialToEntity(m_selectedEntity, p);
                    }
                }
                if (ImGui::MenuItem("Edit Properties")) {
                    m_selectedMaterialFile = p;
                    m_editingMaterial = tempMat;
                }
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                if (ImGui::MenuItem("Delete Material File...")) {
                    m_assetToDelete = p;
                    m_showDeleteAssetModal = true;
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            float lastButtonX2 = ImGui::GetItemRectMax().x;
            float nextButtonX2 = lastButtonX2 + ImGui::GetStyle().ItemSpacing.x + cardWidth;
            if (i + 1 < matFiles.size() && nextButtonX2 < windowVisibleX2) {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        // Selected Material Live Editor
        if (!m_selectedMaterialFile.empty()) {
            ImGui::SeparatorText("Material Editor");
            std::string editStem = std::filesystem::path(m_selectedMaterialFile).stem().string();
            ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "Editing: %s", editStem.c_str());

            bool changed = false;
            if (ImGui::ColorEdit3("Albedo", &m_editingMaterial.albedo.x, ImGuiColorEditFlags_Float)) changed = true;
            if (ImGui::SliderFloat("Roughness", &m_editingMaterial.roughness, 0.04f, 1.0f)) changed = true;
            if (ImGui::SliderFloat("Metallic", &m_editingMaterial.metallic, 0.0f, 1.0f)) changed = true;
            if (ImGui::SliderFloat("AO", &m_editingMaterial.ao, 0.0f, 1.0f)) changed = true;
            if (ImGui::ColorEdit3("Emissive", &m_editingMaterial.emissive.x, ImGuiColorEditFlags_Float)) changed = true;

            ImGui::Text("Maps: %s | %s | %s",
                m_editingMaterial.useDiffuseMap ? "[Albedo]" : "[No Alb]",
                m_editingMaterial.useNormalMap ? "[Normal]" : "[No Nrm]",
                m_editingMaterial.useSpecularMap ? "[Spec]" : "[No Spec]");

            if (ImGui::Button("Save Material Changes", ImVec2(180, 28))) {
                saveMaterialToFile(m_selectedMaterialFile, m_editingMaterial);
                if (registry().valid(m_selectedEntity) && registry().has<MeshRenderer>(m_selectedEntity)) {
                    applyMaterialToEntity(m_selectedEntity, m_selectedMaterialFile);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Apply to Selected", ImVec2(140, 28))) {
                if (registry().valid(m_selectedEntity) && registry().has<MeshRenderer>(m_selectedEntity)) {
                    registry().get<MeshRenderer>(m_selectedEntity).material = m_editingMaterial;
                }
            }
        }
    }
    ImGui::End();
}

void SceneEditor::renderCreateAssetModal() {
    if (!m_showCreateAssetModal) return;

    ImGui::OpenPopup("Create Asset");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440, 220), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Create Asset", &m_showCreateAssetModal, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        const char* typeNames[] = { "PBR Material (.mat.json)", "New Directory / Folder", "Scene (.json)", "C++ Script Template (.h)" };
        ImGui::Combo("Asset Type", &m_createAssetType, typeNames, IM_ARRAYSIZE(typeNames));

        ImGui::InputText("Asset Name", m_createAssetNameBuf, sizeof(m_createAssetNameBuf));
        ImGui::TextDisabled("Location: %s", m_currentDirectory.string().c_str());
        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.35f, 1.0f));
        if (ImGui::Button("Create Asset", ImVec2(140, 32))) {
            std::string name = m_createAssetNameBuf;
            if (m_createAssetType == 0) {
                createNewMaterialFile(name);
            } else if (m_createAssetType == 1) {
                createNewFolder(name);
            } else if (m_createAssetType == 2) {
                createNewSceneFile(name);
            } else if (m_createAssetType == 3) {
                createNewScriptFile(name);
            }
            m_showCreateAssetModal = false;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 32))) {
            m_showCreateAssetModal = false;
        }
        ImGui::EndPopup();
    }
}

void SceneEditor::renderRenameAssetModal() {
    if (!m_showRenameAssetModal) return;

    ImGui::OpenPopup("Rename Asset");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440, 180), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Rename Asset", &m_showRenameAssetModal, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Current: %s", m_assetToRename.filename().string().c_str());
        ImGui::InputText("New Name", m_renameBuf, sizeof(m_renameBuf));
        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
        if (ImGui::Button("Rename", ImVec2(120, 32))) {
            std::string newName = m_renameBuf;
            if (!newName.empty()) {
                auto newPath = m_assetToRename.parent_path() / newName;
                std::error_code ec;
                std::filesystem::rename(m_assetToRename, newPath, ec);
                if (!ec) {
                    m_selectedAssetPath = newPath;
                }
            }
            m_showRenameAssetModal = false;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 32))) {
            m_showRenameAssetModal = false;
        }
        ImGui::EndPopup();
    }
}

void SceneEditor::onShutdown() {
    m_phys.reset();
    cjoka_phys::Global::Shutdown();
    std::cout << "[SceneEditor] Shutdown complete\n";
}
