# CHANGELOG — cjoka engine

Формат: SemVer-подобные версии движка. Даты — 2026.

## [0.41] — 2026-08-25 — FBX Skeletal Animation, Rigging & ClusterLOD

### Skeletal Animation & FBX
- **Поддержка формата FBX**:
  - Быстрый импорт FBX моделей, полисеток, UV и нормалей через `ufbx`
  - Загрузка ригов, иерархии костей и матриц обратной bind-pose (`inverse bind pose`)
  - 4-костный скининг вершин с нормализацией весов (`SkinnedVertex`)
  - Чтение и сэмплирование анимационных клипов / треков с `slerp` (вращение) и `lerp` (позиция, масштаб)
- **Контроллер анимаций (`Animator`)**:
  - `play(clipName, loop, blendTime)` — запуск клипа с плавным переходом (cross-fade)
  - `pause()`, `resume()`, `stop()`, `reset()` — полный контроль проигрывания
  - `setSpeed(speed)` — регулировка скорости и реверс анимации
  - `setTime(time)` — произвольная перемотка
- **Шейдеры скининга**:
  - `kSkinnedLitVS` + `kSkinnedShadowVS` — поддержка до 128 костей за 1 вызов `setMat4Array`
  - Полная поддержка PBR материалов, теней и карты освещения для персонажей
- **Интеграция в Scene и Demo**:
  - `scene().createSkinnedModel(fbxPath, texPath, transform, material)`
  - Анимированный персонаж Натан (`rp_nathan_animated_003_walking.fbx`) в демке с управлением с клавиатуры: `1` (Play), `2` (Pause), `3` (Reset), `-`/`+` (Скорость)

### ClusterLOD (ex-Nanite)
- Все классы, пространства имён и логи переименованы в **`ClusterLOD`** / `ClusteredLOD` (во избежание проблем с товарными знаками)
- Сохранены псевдонимы `namespace nanite = cluster_lod` для обратной совместимости

## [0.40] — 2026-08-25 — Nanite-lite + GPU Instancing (5x-10x Boost) & DX 2.0

### Developer Experience & API (DX)
- **`EntityRef`** — высокоуровневый объектный handle над сущностями (`entity.transform()`, `entity.add<T>()`, `entity.destroy()`)
- **PBR Пресеты материалов**: `Material::Default()`, `Material::Metal()`, `Material::Chrome()`, `Material::Gold()`, `Material::Dielectric()`, `Material::Plastic()`, `Material::Emissive()`, `Material::Textured()` + fluent builder (`withRoughness()`, `withMetallic()`, `withTexture()`)
- **1-строчный спавн в Scene**: `createDynamicBox()`, `createStaticBox()`, `createDynamicSphere()`, `createNaniteModel()`, `createSun()`
- **Transform хелперы**: `forward()`, `right()`, `up()`, `translate()`, `rotate()`
- **Очистка кода**: устранены предупреждения компилятора (0 warnings в коде движка), убраны оверхеды и дублирование
- **Физика**: полноценный `World::AddImpulse`, корректные размеры коллайдеров акторов, CCD для быстрых динамических тел

### Engine (Performance)
- **GPU Instancing для ClusteredMesh**:
  - `DrawInstanced()` и `DrawInstancedShadow()` — группировка одинаковых мешей по LOD
  - Вместо 100+ draw calls на сцену теперь всего ~4 вызова (`glDrawElementsInstanced`)
  - Динамический `m_instanceVBO` с матрицами трансформаций (location 4..7)
  - `kClusterInstancedVS` и `kShadowInstancedVS` для аппаратного инстансинга
  - Рост производительности: **с 7–10 FPS до ~95-108 FPS в Release!**
- **Оптимизация шейдеров и теней**:
  - Early-out для сэмплинга теней на обратных гранях (`NoL <= 0`)
  - 8-точечный Poisson disk для мягких теней (на 33% быстрее выборка)
  - Distance culling объектов (>75м)
- **Nanite-lite** — облегчённый аналог UE5 Nanite:
  - Кластеризация по Morton-порядку (~128 трис/кластер)
  - LOD-цепочка через vertex clustering (4 уровня), базовая ячейка = diag/512
  - Ленточное правило выбора LOD по screen-space error: ровно один уровень на область
  - Frustum culling объектов и кластеров (6-plane sphere test)
  - Сварка с учётом октанта нормали — нет чёрных пятен на двусторонних поверхностях
  - Блокировка граничных вершин между группами — нет щелей между LOD
  - Debug-визуализация: TAB окрашивает по LOD (радуга), N вкл/выкл, `[ ]` live порог
  - HUD: drawn/total кластеров, текущий порог
- **Тени кэшируются** (`shadowEveryNFrames = 4`) — shadow map рисуется раз в N кадров
- Автоматическая нанитизация `MeshRenderer` (ленивый `ClusteredFrom()`)
- Кэш кластеров по .obj и по Mesh3D ptr

### Demo
- Стресс-тест: сетка 10×10 кустов (100 инстансов одного ClusteredMesh) за пределами платформы
- Горшок `indoor_plant.obj` — ~46K трис L0, 4 уровня LOD

## [0.35] — 2026-08-25 — PBR + мягкие тени + CCD

### Engine
- **PBR освещение** (Cook-Torrance GGX): metallic/roughness/ao в `Material`,
  энергосбережение, Френель (Schlick), GGX-распределение, Smith-геометрия
  - Работает в обоих путях рендера (Lit и Instanced/Batcher)
  - `InstanceData` упаковывает roughness (albedo.a) и metallic (emissive.a)
- **Мягкие тени**: Poisson disk 12 сэмплов, радиус растёт с дистанцией,
  fade к 60м (нет жёсткой границы shadow map)
- **Тонемаппинг перенесён в composite**: exposure+ACES+gamma+saturation
  применяется ко всему кадру единообразно (раньше пёклось в объектных шейдерах)
- **Физика**: CCD для динамических тел >20см (без туннелирования),
  сон тел (sleepThreshold 0.05), solver 4/1, ThrowFrom будит+CCD
- `PushAt` игнорирует kinematic (не пинает CCT), рейкаст с userdata per-actor

### Demo
- PBR-витрина: хром/медь/шероховатый пластик, ряд roughness 0.15→0.9
- Пол — prototype-сетка 1METER (конечный физический бокс, за краем пустота),
  коробки — GridBox_Default; уборка тел, упавших ниже y<-30
- CCT толкает ящики плечом при ходьбе; F-бросок с начальной скоростью

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
