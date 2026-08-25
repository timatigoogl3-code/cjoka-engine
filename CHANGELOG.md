# CHANGELOG — cjoka engine

Формат: SemVer-подобные версии движка. Даты — 2026.

## [0.30] — 2026-08-25 — Тени + PhysX

### Engine
- **PhysX 5.5** интегрирован (статически, `third_party/physx5`):
  - `cjoka_phys::World` — сцена с гравитацией, фиксированный шаг 60 Гц
  - `Rigidbody` (Dynamic/Static/Kinematic) + коллайдеры Box/Sphere/Plane
  - **Character Controller**: капсула, ходьба, ступеньки 40см, склоны ≤45°, прыжок
  - Рейкасты (`Raycast`) и толчки (`PushAt`, импульсы)
  - ECS-синк: `BuildFromECS` / `SyncToECS` (Transform ↔ PhysX)
- **Тени от солнца** (`ShadowMap` 2048²):
  - Shadow pass с ортокамеры направленного света, автоматом при наличии `DirectionalLight`
  - PCF 3×3 (мягкие края), slope-scaled bias против acne
  - Работают и в обычном, и в instanced пути рендера
- Фикс: ленивый Init + сохранение FBO/viewport вокруг shadow pass (чёрный экран/«рыбий глаз»)
- `CameraControllers.h`: FlyCamera, Orbit, Follow, FirstPerson — или свой контроллер
- `WorldGen.h`: процедурная генерация — Grid, Ring, Terrain (высотный меш), Scatter
- AssetBrowser: `QuickSpawn` (авто-поиск текстуры по имени модели), ListModels/ListTextures

### Demo
- Ходьба WASD + прыжок Space (CCT), ПКМ обзор, F кинуть ящик, ЛКМ толкнуть объект

## [0.20] — 2026-08-25 — HDR пайплайн + контент (базовый гит)

### Engine
- OpenGL 4.6 Core (GLAD 2.0.8), GLFW 3.4, C++20/Clang/Ninja
- ECS Registry + компоненты (Transform, Material, MeshRenderer, Camera, 4 типа света, Fog, Sky, Text2D/Panel2D)
- Меши: OBJ через tinyobjloader (до сотен тысяч вершин), примитивы Cube/Quad/Sphere
- Текстуры: stb_image, mipmaps + anisotropy 8x, sRGB
- Материалы Blinn-Phong: albedo/specular/emissive, diffuse+specular maps
- Свет: Ambient + Directional + до 8 Point (затухание, range fade)
- Небо: градиент+солнце+звёзды; туман exponential; hemisphere fake-GI
- **HDR pipeline**: RGBA16F → Bloom (half-res ping-pong) → ACES tonemap → vignette → FXAA
- `PostProcessSettings` в ECS: пресеты Cinematic/Vibrant/Soft/Night, live-правка из игры
- Автобатчинг: одинаковые меши → один draw call (instancing, layout 4..9)
- kGUI: кириллица (NotoSans, атлас ASCII+Cyrillic+symbols), панели, HUD
- Scene API «1 строка = 1 объект»: createCube/Sphere/Quad/Model/TexturedModel/Grid/Duplicate
- `SceneLoader`: сцена из JSON без перекомпиляции, hot-reload F5
- AssetManager: кэш weak_ptr, Stats, hot-reload флаг
- sjoka.sh CLI: compile engine|game|all, play game, precompile shaders, clean; LSAN-фильтры

## [0.10] — инициализация проекта
- Каркас: root/engine/game CMake, presets clang-debug/release, окно+GLAD, первый треугольник→куб
