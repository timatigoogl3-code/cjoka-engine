#include "SceneEditor.h"
#include "engine/Core/Input.h"
#include "engine/ECS/Systems.h"
#include "engine/Renderer/DefaultShaders.h"
#include "engine/Assets/AssetManager.h"
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

    m_litShader = std::make_unique<Shader>(DefaultShaders::kLitVS, DefaultShaders::kLitFS);

    // Initial camera
    auto camRef = scene().create("EditorCamera", Transform{m_camPos, {m_camPitch, m_camYaw, 0.0f}, glm::vec3(1.0f)});
    camRef.add<Camera>(Camera{m_camFov, 0.1f, 1000.0f, true, true, 10.0f});
    m_camera = camRef.id();

    // Check if custom_scene.json exists
    std::ifstream check("assets/custom_scene.json");
    if (check.good()) {
        check.close();
        loadSceneFromFile("assets/custom_scene.json");
    } else {
        loadDefaultShowcase();
    }
}

void SceneEditor::loadDefaultShowcase() {
    scene().clear();
    m_selectedEntity = NullEntity;

    // Recreate camera
    auto camRef = scene().create("EditorCamera", Transform{m_camPos, {m_camPitch, m_camYaw, 0.0f}, glm::vec3(1.0f)});
    camRef.add<Camera>(Camera{m_camFov, 0.1f, 1000.0f, true, true, 10.0f});
    m_camera = camRef.id();

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
    auto camRef = scene().create("EditorCamera", Transform{m_camPos, {m_camPitch, m_camYaw, 0.0f}, glm::vec3(1.0f)});
    camRef.add<Camera>(Camera{m_camFov, 0.1f, 1000.0f, true, true, 10.0f});
    m_camera = camRef.id();
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
    if (!registry().valid(m_camera)) return;

    double mouseX, mouseY;
    window().getCursorPos(mouseX, mouseY);
    int w, h; window().getFramebufferSize(w, h);
    if (w <= 0 || h <= 0) return;

    float ndcX = (2.0f * (float)mouseX) / (float)w - 1.0f;
    float ndcY = 1.0f - (2.0f * (float)mouseY) / (float)h;

    auto& camTr = registry().get<Transform>(m_camera);
    auto& camComp = registry().get<Camera>(m_camera);
    float aspect = float(w) / float(h ? h : 1);

    glm::mat4 view = Camera::viewFromTransform(camTr);
    glm::mat4 proj = camComp.projection(aspect);
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
        if (e == m_camera) continue;

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
        } else {
            continue;
        }

        float t = 0.0f;
        if (RayIntersectAABB(localRayOrigin, localRayDir, minB, maxB, t)) {
            // Calculate world space distance
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
    // Toggle UI visibility with Tab
    if (Input::IsKeyJustPressed(GLFW_KEY_TAB)) {
        m_showUI = !m_showUI;
    }

    // Toggle Flycam with Right Mouse Button
    bool rmb = Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    if (rmb && !m_flycamActive) {
        m_flycamActive = true;
        Input::SetCursorLocked(true);
    } else if (!rmb && m_flycamActive) {
        m_flycamActive = false;
        Input::SetCursorLocked(false);
    }

    if (m_flycamActive) {
        glm::vec2 delta = Input::GetMouseDelta();
        m_camYaw   -= delta.x * 0.12f;
        m_camPitch -= delta.y * 0.12f;
        m_camPitch = glm::clamp(m_camPitch, -89.0f, 89.0f);

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

        float speed = m_camSpeed;
        if (Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT)) speed *= 2.5f;
        if (Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL)) speed *= 0.35f;

        if (Input::IsKeyPressed(GLFW_KEY_W)) m_camPos += front * speed * dt;
        if (Input::IsKeyPressed(GLFW_KEY_S)) m_camPos -= front * speed * dt;
        if (Input::IsKeyPressed(GLFW_KEY_D)) m_camPos += right * speed * dt;
        if (Input::IsKeyPressed(GLFW_KEY_A)) m_camPos -= right * speed * dt;
        if (Input::IsKeyPressed(GLFW_KEY_E)) m_camPos += up * speed * dt;
        if (Input::IsKeyPressed(GLFW_KEY_Q)) m_camPos -= up * speed * dt;
    }

    // Object picking via Left Mouse Click
    if (Input::IsMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (!m_flycamActive && !ImGui::GetIO().WantCaptureMouse && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
            pickObjectUnderMouse();
        }
    }

    // Update Camera entity
    if (registry().valid(m_camera)) {
        auto& tr = registry().get<Transform>(m_camera);
        tr.position = m_camPos;
        tr.rotation = {m_camPitch, m_camYaw, 0.0f};
        auto& cam = registry().get<Camera>(m_camera);
        cam.fov = m_camFov;
    }

    // Keyboard Shortcuts
    if (!m_flycamActive && !ImGui::GetIO().WantCaptureKeyboard) {
        if (Input::IsKeyJustPressed(GLFW_KEY_W)) m_gizmoOperation = 0; // Translate
        if (Input::IsKeyJustPressed(GLFW_KEY_E)) m_gizmoOperation = 1; // Rotate
        if (Input::IsKeyJustPressed(GLFW_KEY_R)) m_gizmoOperation = 2; // Scale
        if (Input::IsKeyJustPressed(GLFW_KEY_DELETE) && registry().valid(m_selectedEntity)) {
            scene().destroy(m_selectedEntity);
            m_selectedEntity = NullEntity;
        }
        if (Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL) && Input::IsKeyJustPressed(GLFW_KEY_D) && registry().valid(m_selectedEntity)) {
            // Duplicate
            auto& tr = registry().get<Transform>(m_selectedEntity);
            std::string name = "Copy";
            if (registry().has<Name>(m_selectedEntity)) name = registry().get<Name>(m_selectedEntity).value + "_Copy";
            auto newRef = scene().create(name, Transform{tr.position + glm::vec3(1.0f, 0.0f, 0.0f), tr.rotation, tr.scale});
            if (registry().has<MeshRenderer>(m_selectedEntity)) {
                newRef.add<MeshRenderer>(registry().get<MeshRenderer>(m_selectedEntity));
            }
            if (registry().has<PointLight>(m_selectedEntity)) {
                newRef.add<PointLight>(registry().get<PointLight>(m_selectedEntity));
            }
            m_selectedEntity = newRef.id();
        }
    }
}

void SceneEditor::onUpdate(float dt) {
    updateCamera(dt);

    int w, h;
    window().getFramebufferSize(w, h);
    if (!m_pipe) m_pipe = std::make_unique<RenderPipeline>(w, h);
    m_pipe->resize(w, h);
    m_pipe->syncFromRegistry(registry());

    if (registry().valid(m_camera)) {
        auto& camTr = registry().get<Transform>(m_camera);
        auto& camComp = registry().get<Camera>(m_camera);
        float aspect = float(w) / float(h ? h : 1);
        m_pipe->setCameraMatrices(Camera::viewFromTransform(camTr), camComp.projection(aspect));
    }

    m_pipe->beginFrame();
    Systems::Render(registry(), *m_litShader, window());
    m_pipe->endFrame();
}

void SceneEditor::onImGuiRender() {
    if (!m_showUI) return;

    renderMenuBar();
    renderGizmo();
    renderHierarchy();
    renderInspector();
    renderSpawner();
    renderAtmosphereEditor();
    renderStats();
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
            if (ImGui::MenuItem("Exit", "Alt+F4")) close();
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
            ImGui::MenuItem("Grid Snapping", nullptr, &m_useSnap);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Toggle Editor UI", "Tab", &m_showUI);
            ImGui::EndMenu();
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 340.0f);
        ImGui::TextDisabled("Click Mesh to Select | [RMB] Fly | [Tab] UI");
        ImGui::EndMainMenuBar();
    }
}

void SceneEditor::renderGizmo() {
    if (!registry().valid(m_selectedEntity) || !registry().has<Transform>(m_selectedEntity)) return;
    if (!registry().valid(m_camera) || m_flycamActive) return;

    auto& camTr = registry().get<Transform>(m_camera);
    auto& camComp = registry().get<Camera>(m_camera);
    int w, h; window().getFramebufferSize(w, h);
    float aspect = float(w) / float(h ? h : 1);

    glm::mat4 view = Camera::viewFromTransform(camTr);
    glm::mat4 proj = camComp.projection(aspect);

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
        tr.position = glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]);
        tr.rotation = glm::vec3(matrixRotation[0], matrixRotation[1], matrixRotation[2]);
        tr.scale    = glm::vec3(matrixScale[0], matrixScale[1], matrixScale[2]);
    }
}

void SceneEditor::renderHierarchy() {
    ImGui::SetNextWindowPos(ImVec2(16, 36), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 480), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Scene Hierarchy", nullptr)) {
        ImGui::InputTextWithHint("##Search", "Search entities...", m_searchBuf, sizeof(m_searchBuf));
        ImGui::Separator();

        if (ImGui::Button("+ Entity", ImVec2(90, 0))) {
            auto ref = scene().create("NewEntity", Transform{m_camPos + glm::vec3(0, 0, 5), {}, {1,1,1}});
            m_selectedEntity = ref.id();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(80, 0)) && registry().valid(m_selectedEntity)) {
            scene().destroy(m_selectedEntity);
            m_selectedEntity = NullEntity;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All", ImVec2(80, 0))) {
            newScene();
        }

        ImGui::Separator();
        ImGui::BeginChild("EntityList");

        std::string filter = m_searchBuf;
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

        for (Entity e : registry().view<Transform>()) {
            if (e == m_camera) continue;

            std::string name = "Entity_" + std::to_string((uint32_t)e);
            if (registry().has<Name>(e)) name = registry().get<Name>(e).value;

            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (!filter.empty() && lowerName.find(filter) == std::string::npos) continue;

            std::string tag = "[Obj] ";
            if (registry().has<PointLight>(e)) tag = "[Light] ";
            else if (registry().has<DirectionalLight>(e)) tag = "[Sun] ";
            else if (registry().has<Sky>(e)) tag = "[Sky] ";

            bool isSelected = (m_selectedEntity == e);
            if (ImGui::Selectable((tag + name + "##" + std::to_string((uint32_t)e)).c_str(), isSelected)) {
                m_selectedEntity = e;
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void SceneEditor::renderInspector() {
    ImGui::SetNextWindowPos(ImVec2(1600 - 360, 36), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 680), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Inspector", nullptr)) {
        if (!registry().valid(m_selectedEntity)) {
            ImGui::TextDisabled("Click any object in 3D or list to select.");
            ImGui::End();
            return;
        }

        Entity e = m_selectedEntity;

        // Name
        if (registry().has<Name>(e)) {
            auto& n = registry().get<Name>(e);
            char nameBuf[128];
            strncpy(nameBuf, n.value.c_str(), sizeof(nameBuf));
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                n.value = nameBuf;
            }
        }

        ImGui::Separator();

        // Transform
        if (registry().has<Transform>(e) && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& tr = registry().get<Transform>(e);
            ImGui::DragFloat3("Position", &tr.position.x, 0.1f);
            ImGui::DragFloat3("Rotation", &tr.rotation.x, 0.5f);
            ImGui::DragFloat3("Scale", &tr.scale.x, 0.05f, 0.001f, 100.0f);
            if (ImGui::Button("Reset Transform")) {
                tr.position = {0,0,0}; tr.rotation = {0,0,0}; tr.scale = {1,1,1};
            }
        }

        // MeshRenderer & Material
        if (registry().has<MeshRenderer>(e) && ImGui::CollapsingHeader("Mesh & Material", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& mr = registry().get<MeshRenderer>(e);
            ImGui::Checkbox("Visible", &mr.visible);
            ImGui::Checkbox("ClusterLOD Active", &mr.clusterLOD);

            ImGui::SeparatorText("PBR Material");
            ImGui::ColorEdit3("Albedo", &mr.material.albedo.x);
            ImGui::SliderFloat("Metallic", &mr.material.metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &mr.material.roughness, 0.04f, 1.0f);
            ImGui::SliderFloat("AO", &mr.material.ao, 0.0f, 1.0f);
            ImGui::ColorEdit3("Emissive Color", &mr.material.emissive.x);

            ImGui::SeparatorText("Presets");
            if (ImGui::Button("Gold")) { mr.material.albedo = {1.0f, 0.76f, 0.33f}; mr.material.metallic = 1.0f; mr.material.roughness = 0.25f; }
            ImGui::SameLine();
            if (ImGui::Button("Chrome")) { mr.material.albedo = {0.95f, 0.95f, 0.95f}; mr.material.metallic = 1.0f; mr.material.roughness = 0.08f; }
            ImGui::SameLine();
            if (ImGui::Button("Rubber")) { mr.material.albedo = {0.1f, 0.1f, 0.1f}; mr.material.metallic = 0.0f; mr.material.roughness = 0.9f; }
            ImGui::SameLine();
            if (ImGui::Button("Neon Cyan")) { mr.material.emissive = {0.0f, 8.5f, 10.0f}; }
        }

        // Point Light
        if (registry().has<PointLight>(e) && ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& pl = registry().get<PointLight>(e);
            ImGui::ColorEdit3("Light Color", &pl.color.x);
            ImGui::SliderFloat("Intensity", &pl.intensity, 0.0f, 50.0f, "%.1f");
            ImGui::SliderFloat("Range (Radius)", &pl.range, 0.5f, 100.0f, "%.1f");
        }

        // Directional Light
        if (registry().has<DirectionalLight>(e) && ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& dl = registry().get<DirectionalLight>(e);
            ImGui::DragFloat3("Direction", &dl.direction.x, 0.02f, -1.0f, 1.0f);
            if (ImGui::Button("Normalize Dir")) dl.direction = glm::normalize(dl.direction);
            ImGui::ColorEdit3("Sun Color", &dl.color.x);
            ImGui::SliderFloat("Sun Intensity", &dl.intensity, 0.0f, 10.0f);
        }

        // Add Component menu
        ImGui::Separator();
        if (ImGui::Button("+ Add Point Light")) {
            if (!registry().has<PointLight>(e)) registry().emplace<PointLight>(e, PointLight{{1.0f, 0.9f, 0.8f}, 5.0f, 15.0f});
        }
    }
    ImGui::End();
}

void SceneEditor::renderSpawner() {
    ImGui::SetNextWindowPos(ImVec2(16, 530), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 330), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Asset Spawner", nullptr)) {
        glm::vec3 spawnPos = m_camPos + glm::vec3(0, 0, 4);

        ImGui::SeparatorText("Primitives");
        if (ImGui::Button("+ Cube", ImVec2(80, 0))) m_selectedEntity = spawnPrimitive("Cube", spawnPos);
        ImGui::SameLine();
        if (ImGui::Button("+ Sphere", ImVec2(80, 0))) m_selectedEntity = spawnPrimitive("Sphere", spawnPos);
        ImGui::SameLine();
        if (ImGui::Button("+ Plane", ImVec2(80, 0))) m_selectedEntity = spawnPrimitive("Plane", spawnPos);

        ImGui::SeparatorText("Lighting");
        if (ImGui::Button("+ Warm Light", ImVec2(120, 0))) m_selectedEntity = spawnPointLight(spawnPos, {1.0f, 0.85f, 0.65f}, 5.0f, 15.0f);
        ImGui::SameLine();
        if (ImGui::Button("+ Blue Neon", ImVec2(120, 0))) m_selectedEntity = spawnPointLight(spawnPos, {0.1f, 0.7f, 1.0f}, 7.0f, 18.0f);

        if (ImGui::Button("+ Crimson Light", ImVec2(120, 0))) m_selectedEntity = spawnPointLight(spawnPos, {1.0f, 0.15f, 0.25f}, 7.0f, 18.0f);
        ImGui::SameLine();
        if (ImGui::Button("+ Emerald Light", ImVec2(120, 0))) m_selectedEntity = spawnPointLight(spawnPos, {0.1f, 1.0f, 0.4f}, 7.0f, 18.0f);

        ImGui::SeparatorText("Vehicles & Props");
        if (ImGui::Button("+ Sports Sedan", ImVec2(120, 0)))
            m_selectedEntity = spawnModel("SportsSedan", "assets/models/cars/sedan-sports.obj", "assets/textures/colormap.png", spawnPos, 1.4f);
        ImGui::SameLine();
        if (ImGui::Button("+ Police Car", ImVec2(120, 0)))
            m_selectedEntity = spawnModel("PoliceCruiser", "assets/models/cars/police.obj", "assets/textures/colormap.png", spawnPos, 1.4f);

        if (ImGui::Button("+ Taxi", ImVec2(120, 0)))
            m_selectedEntity = spawnModel("Taxi", "assets/models/cars/taxi.obj", "assets/textures/colormap.png", spawnPos, 1.4f);
        ImGui::SameLine();
        if (ImGui::Button("+ SUV", ImVec2(120, 0)))
            m_selectedEntity = spawnModel("SUV", "assets/models/cars/suv.obj", "assets/textures/colormap.png", spawnPos, 1.4f);

        if (ImGui::Button("+ Bench", ImVec2(80, 0)))
            m_selectedEntity = spawnModel("Bench", "assets/models/bench.obj", "", spawnPos, 1.3f);
        ImGui::SameLine();
        if (ImGui::Button("+ Barrel", ImVec2(80, 0)))
            m_selectedEntity = spawnModel("Barrel", "assets/models/barrel.obj", "assets/textures/barrel.png", spawnPos, 1.2f);
        ImGui::SameLine();
        if (ImGui::Button("+ Plant", ImVec2(80, 0)))
            m_selectedEntity = spawnModel("Plant", "assets/models/indoor_plant.obj", "assets/textures/indoor_plant_COL.jpg", spawnPos, 0.25f);
    }
    ImGui::End();
}

void SceneEditor::renderAtmosphereEditor() {
    ImGui::SetNextWindowPos(ImVec2(1600 - 360, 725), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 150), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Atmosphere & Sky", nullptr)) {
        if (ImGui::Button("Sunset")) {
            if (auto v = registry().view<Sky>(); v.begin() != v.end()) registry().get<Sky>(*v.begin()) = Sky::Sunset();
        }
        ImGui::SameLine();
        if (ImGui::Button("Night")) {
            if (auto v = registry().view<Sky>(); v.begin() != v.end()) registry().get<Sky>(*v.begin()) = Sky::Night();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Day")) {
            if (auto v = registry().view<Sky>(); v.begin() != v.end()) registry().get<Sky>(*v.begin()) = Sky::ClearDay();
        }

        if (auto v = registry().view<Fog>(); v.begin() != v.end()) {
            auto& f = registry().get<Fog>(*v.begin());
            ImGui::SliderFloat("Fog Density", &f.density, 0.0f, 0.02f, "%.4f");
            ImGui::ColorEdit3("Fog Color", &f.color.x);
        }
        if (auto v = registry().view<PostProcessSettings>(); v.begin() != v.end()) {
            auto& post = registry().get<PostProcessSettings>(*v.begin());
            ImGui::SliderFloat("Exposure", &post.exposure, 0.1f, 3.0f);
        }
    }
    ImGui::End();
}

void SceneEditor::renderStats() {
    ImGui::SetNextWindowPos(ImVec2(330, 36), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240, 110), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Engine Stats", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("FPS: %.1f (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / std::max(ImGui::GetIO().Framerate, 1.0f));
        ImGui::Text("Entities: %zu", registry().aliveCount());
        ImGui::Text("Lights: %zu", registry().count<PointLight>());
        ImGui::SliderFloat("Fly Speed", &m_camSpeed, 2.0f, 50.0f);
    }
    ImGui::End();
}

void SceneEditor::saveSceneToFile(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return;

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
        if (e == m_camera) continue;
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

        if (registry().has<MeshRenderer>(e)) {
            auto& mr = registry().get<MeshRenderer>(e);
            file << ",\n      \"mesh\": {\n";
            file << "        \"assetPath\": \"" << mr.assetPath << "\",\n";
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
        file << "\n    }";
    }
    file << "\n  ]\n}\n";
    std::cout << "[SceneEditor] Saved full scene to " << path << "\n";
}

void SceneEditor::loadSceneFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "[SceneEditor] Could not open " << path << ", loading default showcase\n";
        loadDefaultShowcase();
        return;
    }

    scene().clear();
    m_selectedEntity = NullEntity;

    auto camRef = scene().create("EditorCamera", Transform{m_camPos, {m_camPitch, m_camYaw, 0.0f}, glm::vec3(1.0f)});
    camRef.add<Camera>(Camera{m_camFov, 0.1f, 1000.0f, true, true, 10.0f});
    m_camera = camRef.id();

    // Default atmosphere
    auto skyRef = scene().create("Sky"); skyRef.add<Sky>(Sky::Sunset());
    auto fogRef = scene().create("Fog"); fogRef.add<Fog>(Fog{{0.2f, 0.22f, 0.28f}, 0.0035f});
    auto sunRef = scene().create("Sun"); sunRef.add<DirectionalLight>(DirectionalLight{glm::normalize(glm::vec3{-0.4f, -0.8f, -0.3f}), {1.0f, 0.92f, 0.82f}, 2.5f});
    auto ambRef = scene().create("Ambient"); ambRef.add<AmbientLight>(AmbientLight{{0.12f, 0.14f, 0.18f}, 1.0f});
    auto ppRef = scene().create("PostProcess"); ppRef.add<PostProcessSettings>(PostProcessSettings::Cinematic());

    std::string line;
    std::string currentName = "";
    glm::vec3 pos{0.0f}, rot{0.0f}, scale{1.0f};
    bool inMesh = false;
    std::string assetPath = "";
    std::string texturePath = "";
    glm::vec3 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    glm::vec3 emissive{0.0f};
    bool clusterLOD = false;
    bool castShadow = true;
    bool hasMesh = false;

    bool inLight = false;
    glm::vec3 lightCol{1.0f};
    float lightInt = 5.0f;
    float lightRange = 15.0f;
    bool hasLight = false;

    auto instantiateEntity = [&]() {
        if (currentName.empty()) return;
        Transform tr{pos, rot, scale};
        if (hasLight) {
            Material orbMat = Material::Emissive(lightCol, lightInt * 2.0f);
            auto ref = scene().create(currentName, tr);
            MeshRenderer mr(Assets::Sphere(0.15f), orbMat);
            mr.assetPath = "primitive:sphere";
            mr.setClusterLOD(false);
            ref.add<MeshRenderer>(mr);
            ref.add<PointLight>(PointLight{lightCol, lightInt, lightRange});
        } else if (hasMesh) {
            Material mat;
            mat.albedo = albedo;
            mat.metallic = metallic;
            mat.roughness = roughness;
            mat.ao = ao;
            mat.emissive = emissive;
            if (!texturePath.empty()) {
                mat.diffuseMap = Assets::Texture(texturePath, true);
                mat.useDiffuseMap = (mat.diffuseMap && mat.diffuseMap->valid());
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
            mr.texturePath = texturePath;
            mr.setClusterLOD(clusterLOD);
            mr.setCastShadow(castShadow);
            scene().create(currentName, tr).add<MeshRenderer>(mr);
        } else {
            // Fallback inferred from name
            if (currentName.find("SportsSedan") != std::string::npos) spawnModel(currentName, "assets/models/cars/sedan-sports.obj", "assets/textures/colormap.png", pos, scale.x);
            else if (currentName.find("Police") != std::string::npos) spawnModel(currentName, "assets/models/cars/police.obj", "assets/textures/colormap.png", pos, scale.x);
            else if (currentName.find("Taxi") != std::string::npos) spawnModel(currentName, "assets/models/cars/taxi.obj", "assets/textures/colormap.png", pos, scale.x);
            else if (currentName.find("SUV") != std::string::npos) spawnModel(currentName, "assets/models/cars/suv.obj", "assets/textures/colormap.png", pos, scale.x);
            else if (currentName.find("Bench") != std::string::npos) spawnModel(currentName, "assets/models/bench.obj", "", pos, scale.x);
            else if (currentName.find("Barrel") != std::string::npos) spawnModel(currentName, "assets/models/barrel.obj", "assets/textures/barrel.png", pos, scale.x);
            else if (currentName.find("Plant") != std::string::npos) spawnModel(currentName, "assets/models/indoor_plant.obj", "assets/textures/indoor_plant_COL.jpg", pos, scale.x);
            else if (currentName.find("Plane") != std::string::npos || currentName.find("Ground") != std::string::npos) {
                auto floorTex = Assets::Texture("assets/textures/prototype_floor.png", true);
                Material floorMat = Material::Textured(floorTex, {0.9f, 0.9f, 0.9f}, 0.1f, 0.8f);
                scene().create(currentName, tr).add<MeshRenderer>(MeshRenderer(Assets::Cube(1.0f), floorMat).setClusterLOD(false));
            } else if (currentName.find("Sphere") != std::string::npos) {
                Material m; m.albedo = {0.8f, 0.8f, 0.85f}; m.metallic = 0.5f; m.roughness = 0.2f;
                scene().createSphere(tr, m, 0.5f, currentName);
            } else {
                Material m; m.albedo = {0.2f, 0.22f, 0.25f}; m.metallic = 0.8f; m.roughness = 0.2f;
                scene().createCube(tr, m, currentName);
            }
        }
        currentName = "";
        hasLight = false;
        hasMesh = false;
        assetPath = "";
        texturePath = "";
        albedo = {1.0f, 1.0f, 1.0f};
        metallic = 0.0f;
        roughness = 0.5f;
        ao = 1.0f;
        emissive = {0.0f, 0.0f, 0.0f};
    };

    while (std::getline(file, line)) {
        if (line.find("\"atmosphere\":") != std::string::npos) {
            // Atmosphere section
        } else if (line.find("\"name\":") != std::string::npos) {
            instantiateEntity();
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            currentName = line.substr(start, end - start);
        } else if (line.find("\"pos\":") != std::string::npos) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &pos.x, &pos.y, &pos.z);
        } else if (line.find("\"rot\":") != std::string::npos) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &rot.x, &rot.y, &rot.z);
        } else if (line.find("\"scale\":") != std::string::npos) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &scale.x, &scale.y, &scale.z);
        } else if (line.find("\"mesh\":") != std::string::npos) {
            hasMesh = true;
        } else if (line.find("\"assetPath\":") != std::string::npos && hasMesh) {
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            assetPath = line.substr(start, end - start);
        } else if (line.find("\"texturePath\":") != std::string::npos && hasMesh) {
            size_t start = line.find("\"", line.find(":") + 1) + 1;
            size_t end = line.find("\"", start);
            texturePath = line.substr(start, end - start);
        } else if (line.find("\"albedo\":") != std::string::npos && hasMesh) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &albedo.r, &albedo.g, &albedo.b);
        } else if (line.find("\"metallic\":") != std::string::npos && hasMesh) {
            sscanf(line.c_str(), "%*[^:]: %f", &metallic);
        } else if (line.find("\"roughness\":") != std::string::npos && hasMesh) {
            sscanf(line.c_str(), "%*[^:]: %f", &roughness);
        } else if (line.find("\"ao\":") != std::string::npos && hasMesh) {
            sscanf(line.c_str(), "%*[^:]: %f", &ao);
        } else if (line.find("\"emissive\":") != std::string::npos && hasMesh) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &emissive.r, &emissive.g, &emissive.b);
        } else if (line.find("\"clusterLOD\":") != std::string::npos && hasMesh) {
            clusterLOD = (line.find("true") != std::string::npos);
        } else if (line.find("\"castShadow\":") != std::string::npos && hasMesh) {
            castShadow = (line.find("true") != std::string::npos);
        } else if (line.find("\"light\":") != std::string::npos) {
            hasLight = true;
        } else if (line.find("\"color\":") != std::string::npos && hasLight) {
            sscanf(line.c_str(), "%*[^[][%f, %f, %f", &lightCol.r, &lightCol.g, &lightCol.b);
        } else if (line.find("\"intensity\":") != std::string::npos && hasLight) {
            sscanf(line.c_str(), "%*[^:]: %f", &lightInt);
        } else if (line.find("\"range\":") != std::string::npos && hasLight) {
            sscanf(line.c_str(), "%*[^:]: %f", &lightRange);
        }
    }
    instantiateEntity();

    std::cout << "[SceneEditor] Successfully loaded scene from " << path << "\n";
}

void SceneEditor::onShutdown() {
    std::cout << "[SceneEditor] Shutdown complete\n";
}
