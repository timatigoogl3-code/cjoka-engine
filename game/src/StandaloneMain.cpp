#include "engine/Core/Application.h"
#include "engine/Core/Input.h"
#include "engine/Renderer/RenderPipeline.h"
#include "engine/Renderer/DefaultShaders.h"
#include "engine/Renderer/MeshClusters.h"
#include "engine/ECS/Systems.h"
#include "engine/Scene/Scene.h"
#include "engine/Physics/Physics.h"
#include "engine/Audio/AudioEngine.h"
#include "GameScripts.h"
#include <iostream>
#include <fstream>
#include <GLFW/glfw3.h>

class StandaloneGame : public Application {
public:
    StandaloneGame()
        : Application(1920, 1080, "cjoka | Standalone Game") {
    }

    ~StandaloneGame() override = default;

    void onInit() override {
        std::cout << "========================================\n";
        std::cout << "  cjoka Standalone Game Launching...\n";
        std::cout << "========================================\n";

        cjoka_phys::Global::Init();
        m_phys = std::make_unique<cjoka_phys::World>(glm::vec3{0.0f, -9.81f, 0.0f});

        m_litShader = std::make_unique<Shader>(DefaultShaders::kLitVS, DefaultShaders::kLitFS);

        int w, h;
        window().getFramebufferSize(w, h);
        m_pipe = std::make_unique<RenderPipeline>(w, h);

        loadScene("assets/custom_scene.json");
        Input::SetCursorLocked(true);
        m_cursorLocked = true;
    }

    void loadScene(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cout << "[Standalone] Warning: Could not open " << path << ", creating default camera\n";
            auto cam = scene().create("MainCamera", Transform{{0.0f, 2.0f, -8.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
            cam.add<Camera>(Camera{65.0f, 0.1f, 1000.0f, true, true, 10.0f});
            cam.add<CharacterController>();
            scene().create("Sky").add<Sky>(Sky::Sunset());
            scene().create("Sun").add<DirectionalLight>(DirectionalLight{glm::normalize(glm::vec3{-0.4f, -0.8f, -0.3f}), {1.0f, 0.92f, 0.82f}, 2.5f});
            scene().create("Ambient").add<AmbientLight>(AmbientLight{{0.12f, 0.14f, 0.18f}, 1.0f});
            scene().create("PostProcess").add<PostProcessSettings>(PostProcessSettings::Cinematic());
            return;
        }

        scene().clear();
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
        std::string texturePath = "";
        glm::vec3 albedo{1.0f};
        float metallic = 0.0f, roughness = 0.5f, ao = 1.0f;
        glm::vec3 emissive{0.0f};
        bool clusterLOD = false, castShadow = true;
        bool hasLight = false;
        glm::vec3 lightCol{1.0f};
        float lightInt = 5.0f, lightRange = 15.0f;
        bool hasCamera = false;
        float camFovVal = 65.0f, camNearVal = 0.1f, camFarVal = 1000.0f;
        bool camPrimaryVal = true, camPerspVal = true;
        bool hasScript = false;
        std::string scriptName = "";
        bool hasCC = false;
        float ccRadius = 0.4f, ccHeight = 1.8f, ccSpeed = 8.0f, ccJump = 5.0f;

        auto instantiateEntity = [&]() {
            if (currentName.empty() && !hasTransform) return;
            std::string entName = currentName.empty() ? "Entity" : currentName;
            Transform tr{pos, rot, scale};
            auto ref = scene().create(entName, tr);
            Entity e = ref.id();

            if (hasCamera) {
                ref.add<Camera>(Camera{camFovVal, camNearVal, camFarVal, camPrimaryVal, camPerspVal, 10.0f});
            }

            if (hasMesh || !assetPath.empty()) {
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
                ref.add<MeshRenderer>(mr);
            } else if (hasLight) {
                Material orbMat = Material::Emissive(lightCol, lightInt * 2.0f);
                MeshRenderer mr(Assets::Sphere(0.15f), orbMat);
                mr.assetPath = "primitive:sphere";
                mr.setClusterLOD(false);
                ref.add<MeshRenderer>(mr);
            }

            if (hasLight) {
                ref.add<PointLight>(PointLight{lightCol, lightInt, lightRange});
            }

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
                        ns.instance->onStart();
                    }
                }
            }

            if (hasCC) {
                auto& cc = ref.add<CharacterController>();
                cc.radius = ccRadius;
                cc.height = ccHeight;
                cc.speed = ccSpeed;
                cc.jumpForce = ccJump;
            }

            currentName = "";
            hasTransform = false;
            hasMesh = false;
            assetPath = "";
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
        };

        while (std::getline(file, line)) {
            if (line.find("\"name\":") != std::string::npos && line.find("\"script\":") == std::string::npos) {
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
            } else if (line.find("\"script\":") != std::string::npos) {
                hasScript = true;
            } else if (line.find("\"name\":") != std::string::npos && hasScript) {
                size_t start = line.find("\"", line.find(":") + 1) + 1;
                size_t end = line.find("\"", start);
                scriptName = line.substr(start, end - start);
            }
        }
        instantiateEntity();

        bool foundCam = false;
        for (Entity e : registry().view<Camera>()) {
            (void)e;
            foundCam = true;
            break;
        }
        if (!foundCam) {
            auto camRef = scene().create("MainCamera", Transform{{0.0f, 3.5f, -12.0f}, {-10.0f, 0.0f, 0.0f}, glm::vec3(1.0f)});
            camRef.add<Camera>(Camera{65.0f, 0.1f, 1000.0f, true, true, 10.0f});
            camRef.add<CharacterController>();
        }

        std::cout << "[Standalone] Successfully loaded scene: " << path << "\n";
    }

    void onUpdate(float dt) override {
        // Toggle cursor lock with Escape
        if (Input::IsKeyJustPressed(GLFW_KEY_ESCAPE)) {
            m_cursorLocked = !m_cursorLocked;
            Input::SetCursorLocked(m_cursorLocked);
        }

        // Run Physics
        if (m_phys) {
            m_phys->Step(dt);
            m_phys->SyncToECS(registry());
        }

        // Run Native Scripts
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

        // Find primary camera
        Entity primaryCam = NullEntity;
        for (Entity e : registry().view<Camera, Transform>()) {
            if (registry().get<Camera>(e).primary) {
                primaryCam = e;
                break;
            }
            if (primaryCam == NullEntity) primaryCam = e;
        }

        // Default Character Controller / First person look on primary camera
        if (m_cursorLocked && registry().valid(primaryCam) && registry().has<Transform>(primaryCam)) {
            auto& tr = registry().get<Transform>(primaryCam);
            glm::vec2 delta = Input::GetMouseDelta();
            tr.rotation.y += delta.x * 0.12f;
            tr.rotation.x += delta.y * 0.12f;
            tr.rotation.x = glm::clamp(tr.rotation.x, -89.0f, 89.0f);

            glm::vec3 moveDir{0.0f};
            float yawRad = glm::radians(tr.rotation.y);
            float pitchRad = glm::radians(tr.rotation.x);
            glm::vec3 front{
                std::cos(yawRad) * std::cos(pitchRad),
                std::sin(pitchRad),
                std::sin(yawRad) * std::cos(pitchRad)
            };
            front = glm::normalize(front);
            glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));

            if (Input::IsKeyPressed(GLFW_KEY_W)) moveDir += front;
            if (Input::IsKeyPressed(GLFW_KEY_S)) moveDir -= front;
            if (Input::IsKeyPressed(GLFW_KEY_D)) moveDir += right;
            if (Input::IsKeyPressed(GLFW_KEY_A)) moveDir -= right;

            moveDir.y = 0.0f;
            if (glm::length(moveDir) > 0.0f) moveDir = glm::normalize(moveDir);

            float speed = 8.0f;
            if (Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT)) speed *= 2.0f;

            if (registry().has<CharacterController>(primaryCam)) {
                auto& cc = registry().get<CharacterController>(primaryCam);
                if (!cc.pxController && m_phys) {
                    cc.pxController = m_phys->CreateCharacter(tr.position, cc.radius, cc.height);
                    cc.velocity = glm::vec3(0.0f);
                }
                glm::vec3 disp = moveDir * cc.speed * dt;
                cc.velocity.y -= 9.81f * dt;
                if (cc.onGround && Input::IsKeyJustPressed(GLFW_KEY_SPACE)) {
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
                if (Input::IsKeyPressed(GLFW_KEY_SPACE)) tr.position.y += speed * dt;
                if (Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL)) tr.position.y -= speed * dt;
            }
        }

        // Render pipeline
        int w, h;
        window().getFramebufferSize(w, h);
        if (!m_pipe) m_pipe = std::make_unique<RenderPipeline>(w, h);
        m_pipe->resize(w, h);
        m_pipe->syncFromRegistry(registry());

        float aspect = float(w) / float(h ? h : 1);
        glm::mat4 viewMatrix(1.0f);
        glm::mat4 projMatrix = glm::perspective(glm::radians(65.0f), aspect, 0.1f, 1000.0f);
        glm::vec3 viewPos{0.0f, 2.0f, -8.0f};

        if (registry().valid(primaryCam) && registry().has<Transform>(primaryCam)) {
            auto& tr = registry().get<Transform>(primaryCam);
            auto& cam = registry().get<Camera>(primaryCam);
            viewMatrix = Camera::viewFromTransform(tr);
            projMatrix = cam.projection(aspect);
            viewPos = tr.position;
            m_pipe->setCameraMatrices(viewMatrix, projMatrix);
        }

        m_pipe->beginFrame();
        Systems::RenderWithCamera(registry(), *m_litShader, window(), viewMatrix, projMatrix, viewPos, m_pipe->prevViewProj());
        m_pipe->endFrame();
    }

    void onImGuiRender() override {
        // Minimal HUD in standalone game
        if (!m_cursorLocked) {
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(320, 140), ImGuiCond_Always);
            if (ImGui::Begin("Game Paused", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "cjoka Standalone Game");
                ImGui::Separator();
                if (ImGui::Button("Resume Game (ESC)", ImVec2(280, 35))) {
                    m_cursorLocked = true;
                    Input::SetCursorLocked(true);
                }
                if (ImGui::Button("Exit to Desktop", ImVec2(280, 35))) {
                    close();
                }
            }
            ImGui::End();
        }
    }

    void onShutdown() override {
        m_phys.reset();
        cjoka_phys::Global::Shutdown();
        std::cout << "[Standalone] Game shutdown complete\n";
    }

private:
    std::unique_ptr<Shader> m_litShader;
    std::unique_ptr<RenderPipeline> m_pipe;
    std::unique_ptr<cjoka_phys::World> m_phys;
    bool m_cursorLocked = false;
};

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
extern "C" const char* __asan_default_options() {
    return "detect_leaks=0";
}
#endif
#endif

int main() {
    StandaloneGame app;
    app.run();
    return 0;
}
