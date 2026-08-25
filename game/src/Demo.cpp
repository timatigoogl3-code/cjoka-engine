#include "Demo.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// ============================================================
//  ДЕМКА ВОЗМОЖНОСТЕЙ ДВИЖКА cjoka
//  Что показываем (всё движком, без читов):
//   1. Загрузка тяжёлого .obj — горшок 137k вершин / 45k треугольников
//   2. Текстуры 2048x2048 + mipmaps + anisotropy 8x
//   3. Blinn-Phong свет: ambient + directional + 4 point light
//   4. Материалы: albedo/shininess/emissive/specular
//   5. Небо: градиент + солнце + звёзды; туман exponential
//   6. HDR pipeline: bloom (half-res ping-pong) + ACES tonemap + vignette + FXAA
//   7. Автобатчинг: ряд одинаковых объектов → 1 draw call
//   8. kGUI: кириллица, панели, HUD с fps
//  Управление: WASD+QE полёт, ПКМ мышь, Shift ускорение.
//  Клавиши демо: 1-4 пресеты PP, B bloom, F fxaa, [ ] exposure, P спавн горшка.
// ============================================================

Demo::Demo() : Application(1280, 720, "cjoka engine demo — OBJ • HDR • Bloom • FXAA • Batching • kGUI") {}
Demo::~Demo() = default;

void Demo::setupAtmosphere() {
    scene().createBeautifulAtmosphere(
        {.top{0.16f,0.40f,0.86f}, .horizon{0.60f,0.74f,0.94f}, .bottom{0.88f,0.90f,0.95f}, .exposure=1.08f},
        {.color{0.10f,0.12f,0.16f}, .density=0.012f});
    scene().createPost(PostProcessSettings::Cinematic());
}

void Demo::setupLights() {
    registry().emplace<AmbientLight>(scene().create("Ambient"), AmbientLight{{0.20f,0.20f,0.25f}, 1.0f});
    scene().createDirectionalLight({-0.55f,-1.0f,-0.35f}, {1.0f,0.96f,0.88f}, 1.35f, "Sun");
    scene().createPointLight({{ 2.4f,2.2f,1.4f}}, {.color{1.00f,0.75f,0.42f}, .intensity=1.7f, .range=15.0f}, "Warm");
    scene().createPointLight({{-2.2f,1.4f,-1.8f}}, {.color{0.42f,0.70f,1.00f}, .intensity=1.4f, .range=13.0f}, "Cold");
    scene().createPointLight({{ 0.0f,3.6f,0.0f}}, {.color{1.00f,1.00f,1.00f}, .intensity=0.9f, .range=24.0f}, "Rim");
}

void Demo::setupShowcase() {
    // Пол
    auto texChecker = Assets::Texture("assets/textures/checker.png");
    scene().createQuad({{0,-0.9f,0},{-90,0,0},{9,9,1}},
                       texturedMaterial(texChecker, glm::vec3(1), 24), 1.5f, "Ground");

    // 1) Тяжёлая модель: горшок 137k verts, текстура 2048 COL
    m_plant = Assets::QuickSpawn(scene(), "assets/models/indoor_plant.obj",
                                 {{0,-0.35f,-1.6f},{0,-20,0},{0.20f,0.20f,0.20f}});

    // 2) Металлическая сфера — specular/shininess
    m_metalSphere = scene().createSphere({{-2.3f,-0.45f,-0.6f}},
        {.albedo{0.92f,0.93f,0.97f}, .metallic=0.85f, .shininess=160}, 0.45f, "MetalSphere");

    // 3) Emissive шар — питает bloom
    m_emissiveOrb = scene().createSphere({{2.3f,1.1f,-0.8f}},
        {.albedo{1,1,1}, .emissive{2.4f,1.6f,0.7f}, .shininess=32}, 0.22f, "EmissiveOrb");

    // 4) Батчинг: 8 одинаковых кубиков разного цвета → 1 draw call
    for (int i = 0; i < 8; ++i) {
        float x = -1.75f + i * 0.5f;
        Material m;
        m.albedo = {0.85f + 0.02f*i, 0.45f + 0.06f*i, 0.30f + 0.09f*i};
        m.shininess = 90;
        auto e = scene().createCube({{x,-0.68f,1.6f},{0,float(i)*22,0},{0.28f,0.28f,0.28f}}, m, "Batch"+std::to_string(i));
        m_batchRow.push_back(e);
    }

    // 5) Текстурированные кубы по бокам
    auto texGradient = Assets::Texture("assets/textures/gradient.png");
    scene().createCube({{-3.4f,0.1f,0.4f}}, texturedMaterial(texGradient, glm::vec3(1), 48), "TexCubeL");
    scene().createCube({{ 3.4f,0.1f,0.4f}}, texturedMaterial(texGradient, glm::vec3(1), 48), "TexCubeR");

    m_camera = scene().createCamera({{0,1.7f,5.2f},{-11,-90,0}}, {55.0f,0.1f,120.0f,true}, "MainCamera");
}

void Demo::setupGUI() {
    registry().emplace<Panel2D>(scene().create("HUDPanel"),
        Panel2D{{12,12},{430,86}, {0.06f,0.07f,0.10f,0.80f}, 10});
    registry().emplace<Text2D>(scene().create("Title"),
        Text2D{"cjoka — демка движка", {24,18}, 1.05f, {1,0.96f,0.84f,1}});
    registry().emplace<Text2D>(scene().create("Sub"),
        Text2D{"GL 4.6 • OBJ 137k • HDR Bloom • FXAA • Batching", {24,46}, 0.58f, {0.82f,0.87f,0.95f,1}});
    registry().emplace<Text2D>(scene().create("Hint"),
        Text2D{"WASD+QE • ПКМ обзор • 1-4 PP • B bloom • F fxaa • [ ] экспозиция", {24,68}, 0.52f, {0.66f,0.72f,0.80f,1}});
}

void Demo::onInit() {
    std::cout << "[Demo] init\n";
    m_litShader = std::make_unique<Shader>(DefaultShaders::kLitVS, DefaultShaders::kLitFS);

    setupAtmosphere();
    setupLights();
    setupShowcase();
    setupGUI();

    std::cout << "[Demo] entities=" << scene().alive() << "\n";
    Assets::Stats();
}

void Demo::onUpdate(float dt) {
    Systems::FlyCameraSystem(registry(), window(), dt);
    m_time += dt;

    // Анимации витрины
    if (registry().valid(m_emissiveOrb)) {
        auto& tr = registry().get<Transform>(m_emissiveOrb);
        tr.position.y = 1.1f + std::sin(m_time*1.4f)*0.35f;
        tr.position.x = 2.3f + std::cos(m_time*0.9f)*0.45f;
    }
    if (!m_batchRow.empty()) {
        float base = -1.75f;
        for (size_t i = 0; i < m_batchRow.size(); ++i)
            if (registry().valid(m_batchRow[i]))
                registry().get<Transform>(m_batchRow[i]).position.y =
                    -0.68f + std::sin(m_time*2.0f + float(i)*0.7f)*0.12f;
        (void)base;
    }

    // Тёплый свет плавает
    for (Entity e : registry().view<PointLight, Transform>()) {
        auto& pl = registry().get<PointLight>(e);
        if (pl.color.r > 0.9f && pl.color.g > 0.5f && pl.color.b < 0.6f) {
            auto& tr = registry().get<Transform>(e);
            tr.position.x =  2.4f + std::cos(m_time*0.7f)*0.5f;
            tr.position.y =  2.2f + std::sin(m_time*1.1f)*0.3f;
        }
    }

    // PP хоткеи
    static bool k1=false,k2=false,k3=false,k4=false,kB=false,kF=false,kL=false,kR=false,kP=false;
    bool n1=window().isKeyPressed(GLFW_KEY_1), n2=window().isKeyPressed(GLFW_KEY_2),
         n3=window().isKeyPressed(GLFW_KEY_3), n4=window().isKeyPressed(GLFW_KEY_4),
         nb=window().isKeyPressed(GLFW_KEY_B), nf=window().isKeyPressed(GLFW_KEY_F),
         nl=window().isKeyPressed(GLFW_KEY_LEFT_BRACKET), nr=window().isKeyPressed(GLFW_KEY_RIGHT_BRACKET),
         np=window().isKeyPressed(GLFW_KEY_P);
    if (auto v = registry().view<PostProcessSettings>(); !v.empty()) {
        auto& pp = registry().get<PostProcessSettings>(v[0]);
        if (n1 && !k1) pp = PostProcessSettings::Cinematic();
        if (n2 && !k2) pp = PostProcessSettings::Vibrant();
        if (n3 && !k3) pp = PostProcessSettings::Soft();
        if (n4 && !k4) pp = PostProcessSettings::Night();
        if (nb && !kB) pp.bloom = !pp.bloom;
        if (nf && !kF) pp.fxaa  = !pp.fxaa;
        if (nl && !kL) pp.exposure = std::max(0.5f, pp.exposure-0.05f);
        if (nr && !kR) pp.exposure = std::min(2.0f, pp.exposure+0.05f);
    }
    if (np && !kP) {
        float x = float(rand()%160 - 80)/40.0f, z = float(rand()%160 - 80)/40.0f;
        Assets::QuickSpawn(scene(), "assets/models/indoor_plant.obj",
                           {{x,-0.35f,z},{0,float(rand()%360),0},{0.14f,0.14f,0.14f}});
    }
    k1=n1;k2=n2;k3=n3;k4=n4;kB=nb;kF=nf;kL=nl;kR=nr;kP=np;

    // Кадр
    int w,h; window().getFramebufferSize(w,h);
    static std::unique_ptr<RenderPipeline> pipe;
    if (!pipe) pipe = std::make_unique<RenderPipeline>(w,h);
    pipe->resize(w,h);
    pipe->syncFromRegistry(registry());
    pipe->beginFrame();
    Systems::Render(registry(), *m_litShader, window());
    pipe->endFrame();

    updateHUD(dt,w,h);
}

void Demo::updateHUD(float dt, int w, int h) {
    kGUI::BeginFrame();
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << (dt>0?1.0f/dt:0.0f) << " fps • "
       << w << "x" << h << " • ents " << scene().alive();

    const PostProcessSettings* pp = nullptr;
    if (auto v = registry().view<PostProcessSettings>(); !v.empty())
        pp = &registry().get<PostProcessSettings>(v[0]);
    if (pp) ss << " • exp " << std::setprecision(2) << pp->exposure
               << (pp->bloom?" bloom":"") << (pp->fxaa?" fxaa":"");

    kGUI::Text(float(w)-360.f, 18.f, 0.62f, ss.str(), {0.92f,0.94f,1.0f,1});
    kGUI::Text(float(w)-360.f, 40.f, 0.44f,
        "batching ON • aniso 8x • mipmaps • GL 4.6", {0.72f,0.78f,0.86f,1});
    kGUI::Panel(float(w)-370.f, float(h)-34.f, 358, 26, {0,0,0,0.45f});
    kGUI::Text(float(w)-362.f, float(h)-27.f, 0.50f,
        "cjoka demo — всё движком ✓", {1,1,1,0.92f});
    kGUI::EndFrame();
}

void Demo::onShutdown() { std::cout << "[Demo] shutdown clean\n"; }
