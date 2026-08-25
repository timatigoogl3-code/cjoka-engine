#include "Demo.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// ============================================================
//  cjoka DEMO — рендер GL4.6 + NVIDIA PhysX 5.5
//   • Гравитация и падение объектов (Rigidbody Dynamic)
//   • Статика: пол/платформы
//   • Character Controller: WASD ходьба, Space прыжок, ступеньки
//   • F — кинуть ящик вперёд (impulse)
//   • Рендер: HDR bloom, FXAA, sky, fog, batching, kGUI
// ============================================================

static Entity MakeBox(Scene& sc, glm::vec3 pos, glm::vec3 scale, const Material& m, const std::string& name) {
    return sc.createCube({pos, {}, scale}, m, name);
}

Demo::Demo() : Application(1280,720,"cjoka demo — PhysX 5.5 + HDR pipeline") {}
Demo::~Demo() = default;

void Demo::setupWorld() {
    scene().createBeautifulAtmosphere(
        {.top{0.16f,0.40f,0.86f}, .horizon{0.60f,0.74f,0.94f}, .bottom{0.88f,0.90f,0.95f}, .exposure=1.08f},
        {.color{0.10f,0.12f,0.16f}, .density=0.010f});
    scene().createPost(PostProcessSettings::Cinematic());
    registry().emplace<AmbientLight>(scene().create("Ambient"), AmbientLight{{0.20f,0.20f,0.25f},1});
    scene().createDirectionalLight({-0.55f,-1,-0.35f},{1.0f,0.96f,0.88f},1.35f,"Sun");
    scene().createPointLight({{2.4f,2.2f,1.4f}}, {.color{1.0f,0.75f,0.42f}, .intensity=1.6f, .range=15}, "Warm");
}

void Demo::setupPlayer() {
    // CCT капсула: radius 0.35 height 1.2 (глаза ~1.7)
    m_cct = m_phys->CreateCharacter(m_playerPos, 0.35f, 1.2f);
}

void Demo::setupGUI() {
    registry().emplace<Panel2D>(scene().create("HUDPanel"),
        Panel2D{{12,12},{430,86},{0.06f,0.07f,0.10f,0.80f},10});
    registry().emplace<Text2D>(scene().create("Title"),
        Text2D{"cjoka + PhysX 5.5", {24,18}, 1.05f, {1,0.96f,0.84f,1}});
    registry().emplace<Text2D>(scene().create("Sub"),
        Text2D{"CCT ходьба+прыжок • динамика ящиков • HDR bloom", {24,46}, 0.58f, {0.82f,0.87f,0.95f,1}});
    registry().emplace<Text2D>(scene().create("Hint"),
        Text2D{"WASD • Space прыжок • F кинуть ящик • ПКМ обзор", {24,68}, 0.52f, {0.66f,0.72f,0.80f,1}});
}

void Demo::onInit() {
    std::cout << "[Demo] init\n";
    cjoka_phys::Global::Init();
    m_phys = std::make_unique<cjoka_phys::World>(glm::vec3{0,-9.81f,0});

    m_litShader = std::make_unique<Shader>(DefaultShaders::kLitVS, DefaultShaders::kLitFS);
    setupWorld();

    // Пол — prototype-сетка 1METER
    auto texFloor = Assets::Texture("assets/textures/prototype_floor.png");
    scene().createQuad({{0,0,0},{-90,0,0},{14,14,1}}, texturedMaterial(texFloor,glm::vec3(1),24), 1.75f, "Ground");
    // физический пол = КОНЕЧНЫЙ бокс ровно по визуалу (21x21), за краем — пустота
    m_phys->CreateBoxActor({0,-0.05f,0}, {10.5f,0.05f,10.5f}, false, 0);

    // Платформа-ступеньки (статика)
    Material plat; plat.albedo={0.75f,0.72f,0.66f}; plat.shininess=8;
    for (int i=0;i<3;++i)
        MakeBox(scene(), {float(i)*-1.6f - 1.6f, 0.25f + i*0.5f, -2.f}, {1.5f,0.25f+i*0.5f>0?0.5f+i*0.5f:0.5f, 1.5f}, plat, ("Step"+std::to_string(i)).c_str());
    // физика для платформ — статические боксы (визуал выше совпадает по габаритам приблизительно)
    for (int i=0;i<3;++i)
        m_phys->CreateBoxActor({float(i)*-1.6f - 1.6f, 0.25f + i*0.5f, -2.f}, {1.5f, 0.5f*(i+1), 1.5f}, false, 0);

    // Башня из ящиков — GridBox прототип-текстура
    auto texGridBox = Assets::Texture("assets/textures/GridBox_Default.png");
    for (int y=0;y<4;++y) for(int x=0;x<2;++x){
        float px = 2.0f + x*0.62f, py = 0.31f + y*0.62f;
        auto e = MakeBox(scene(), {px,py,-1.5f}, {0.6f,0.6f,0.6f},
                         texturedMaterial(texGridBox,glm::vec3(1),48), "Crate");
        registry().emplace<cjoka_phys::Rigidbody>(e, cjoka_phys::Rigidbody{});
        m_debris.push_back(e);
    }

    // Горшок — тяжёлая моделька как декор (без физики, статика сцены)
    Assets::QuickSpawn(scene(), "assets/models/indoor_plant.obj",
                       {{-3.2f,0,-1.0f},{0,-30,0},{0.18f,0.18f,0.18f}});

    m_camera = scene().createCamera({{0,1.7f,4.5f},{0,-90,0}}, {55.0f,0.1f,120.0f,true}, "MainCamera");

    setupPlayer();
    setupGUI();
    std::cout << "[Demo] entities=" << scene().alive() << "\n";
}

void Demo::handlePlayerInput(float dt) {
    if (!m_cct || !registry().valid(m_camera)) return;
    auto& camTr = registry().get<Transform>(m_camera);

    // --- Обзор: ТОЛЬКО ПКМ вращает взгляд, камера не двигается ---
    static bool grabbing=false; static double lastX=0,lastY=0;
    bool mb = window().isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    if (mb) {
        if (!grabbing) { grabbing=true; window().setCursorMode(GLFW_CURSOR_DISABLED); lastX=0; lastY=0; }
        double x,y; window().getCursorPos(x,y);
        if (lastX==0 && lastY==0) { lastX=x; lastY=y; }
        float dx=static_cast<float>(x-lastX), dy=static_cast<float>(y-lastY);
        lastX=x; lastY=y;
        camTr.rotation.y += dx*0.15f;
        camTr.rotation.x -= dy*0.15f;
        camTr.rotation.x = glm::clamp(camTr.rotation.x, -89.0f, 89.0f);
    } else if (grabbing) { grabbing=false; window().setCursorMode(GLFW_CURSOR_NORMAL); }

    // --- Движение: ТОЧНО та же формула что Camera::viewFromTransform ---
    float yaw = glm::radians(camTr.rotation.y);
    glm::vec3 fwd{ std::cos(yaw), 0, std::sin(yaw) }; // yaw=-90 → (0,0,-1) = взгляд
    fwd = glm::normalize(fwd);
    glm::vec3 right{ -fwd.z, 0, fwd.x };              // cross(fwd, up) → при взгляде -Z право = +X

    glm::vec3 move{};
    if (window().isKeyPressed(GLFW_KEY_W)) move += fwd;
    if (window().isKeyPressed(GLFW_KEY_S)) move -= fwd;
    if (window().isKeyPressed(GLFW_KEY_A)) move -= right;
    if (window().isKeyPressed(GLFW_KEY_D)) move += right;
    if (glm::length(move)>0.001f) move = glm::normalize(move)*4.0f;

    float vy = m_playerVel.y;
    if (m_onGround && window().isKeyPressed(GLFW_KEY_SPACE)) vy = 5.0f;
    vy -= 9.81f*dt*1.6f;

    glm::vec3 disp = (move + glm::vec3{0,vy,0}) * dt;
    m_onGround = m_phys->MoveCharacter(m_cct, disp, dt, m_playerPos);
    m_playerVel.y = m_onGround ? 0 : vy;

    // толкаем ящики плечом при ходьбе
    if (glm::length(move) > 0.5f) {
        glm::vec3 pushDir = glm::normalize(move);
        m_phys->PushAt(m_playerPos + glm::vec3{0,0.35f,0} + pushDir*0.55f, pushDir, 0.6f, 2.2f*dt*60.0f*0.016f + 1.5f);
    }

    // камера следует за игроком (глаза)
    camTr.position = m_playerPos + glm::vec3{0,0.65f,0};
}

void Demo::onUpdate(float dt) {
    handlePlayerInput(dt); // мышь=обзор, WASD=ходьба, Space=прыжок (FlyCameraSystem отключён — он дублировал управление)

    static bool fDown=false;
    bool f = window().isKeyPressed(GLFW_KEY_F);
    if (f && !fDown && registry().valid(m_camera)) {
        // спавн ящика перед камерой + полёт вперёд (динамике задаём скорость через transform sync)
        auto& camTr = registry().get<Transform>(m_camera);
        float yaw=glm::radians(camTr.rotation.y), pitch=glm::radians(camTr.rotation.x);
        glm::vec3 dir{std::cos(yaw)*std::cos(pitch), std::sin(pitch), std::sin(yaw)*std::cos(pitch)};
        dir.z = -dir.z;
        dir = glm::normalize(dir);
        auto texG = Assets::Texture("assets/textures/GridBox_Default.png");
        auto e = MakeBox(scene(), camTr.position + dir*1.2f, glm::vec3{0.4f,0.4f,0.4f},
                         texturedMaterial(texG,{1,1,1},48), "Thrown"+std::to_string(m_spawned++));
        registry().emplace<cjoka_phys::Rigidbody>(e, cjoka_phys::Rigidbody{});
        m_debris.push_back(e);
        // сразу актор + начальная скорость (иначе падает под ногами)
        m_phys->BuildFromECS(registry());
        m_phys->ThrowFrom(e, dir*9.0f + glm::vec3{0,2.5f,0});
        std::cout << "[Demo] thrown box #" << m_spawned << "\n";
    }
    fDown=f;

    // ЛКМ — толкнуть объект под прицелом (как рука)
    static bool lmb=false;
    bool mb = window().isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    if (mb && !lmb && registry().valid(m_camera)) {
        auto& camTr = registry().get<Transform>(m_camera);
        float yaw=glm::radians(camTr.rotation.y), pitch=glm::radians(camTr.rotation.x);
        glm::vec3 dir{std::cos(yaw)*std::cos(pitch), std::sin(pitch), std::sin(yaw)*std::cos(pitch)};
        dir.z=-dir.z; dir=glm::normalize(dir);
        glm::vec3 origin = camTr.position + dir*0.8f; // вне капсулы CCT
        m_phys->PushAt(origin, dir, 6.0f, 14.0f);
    }
    lmb=mb;

    // Физика шаг + синк
    m_phys->Step(dt);

    // упали в пустоту — удаляем
    m_debris.erase(std::remove_if(m_debris.begin(), m_debris.end(), [&](Entity e){
        if (!registry().valid(e)) return true;
        auto* rb = registry().try_get<cjoka_phys::Rigidbody>(e);
        auto* tr = registry().try_get<Transform>(e);
        if (tr && tr->position.y < -30.0f) {
            if (rb && rb->pxActor) m_phys->RemoveActor(rb->pxActor);
            scene().destroy(e);
            return true;
        }
        return false;
    }), m_debris.end());

    // новые Rigidbody -> акторы, потом синк позиций
    m_phys->BuildFromECS(registry());
    m_phys->SyncToECS(registry());

    int w,h; window().getFramebufferSize(w,h);
    if (!m_pipe) m_pipe = std::make_unique<RenderPipeline>(w,h);
    m_pipe->resize(w,h);
    m_pipe->syncFromRegistry(registry());
    m_pipe->beginFrame();
    Systems::Render(registry(), *m_litShader, window());
    m_pipe->endFrame();
    updateHUD(dt,w,h);
}

void Demo::updateHUD(float dt, int w, int h) {
    kGUI::BeginFrame();
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << (dt>0?1/dt:0.f) << " fps • ents " << scene().alive()
       << " • bodies " << m_debris.size();
    kGUI::Text(float(w)-340.f, 18.f, 0.62f, ss.str(), {0.92f,0.94f,1,1});
    kGUI::Text(float(w)-340.f, 40.f, 0.44f,
        m_onGround ? "ground: yes" : "ground: no", {0.72f,0.78f,0.86f,1});
    kGUI::Panel(float(w)-350.f, float(h)-34.f, 338,26,{0,0,0,0.45f});
    kGUI::Text(float(w)-342.f, float(h)-27.f, 0.5f, "PhysX 5.5 работает ✓", {1,1,1,0.92f});
    kGUI::EndFrame();
}

void Demo::onShutdown() {
    if (m_cct) { m_phys->RemoveCharacter(m_cct); m_cct=nullptr; }
    m_phys.reset();
    cjoka_phys::Global::Shutdown();
    std::cout << "[Demo] shutdown clean\n";
}
