# game/ — для БОЛЬШИХ игр: удобно и приятно

```
game/src/
├── Game.cpp/.h      → главный уровень (только тут логика уровня)
├── Levels/          → LevelCity.cpp, LevelForest.cpp (каждый уровень — отдельный .cpp)
├── Prefabs/         → LevelPrefabs.h (повторяемые штуки: улица, комната, враг)
├── Systems/         → MyCombatSystem.h, MyQuestSystem.h (ECS системы)
├── Components/      → MyHealth.h, MyInventory.h (ECS компоненты)
└── main.cpp         → не трогай
```

**Правило:** `engine/` не пересобирается при правке `game/` → `sjoka.sh compile game` — только линковка (1с).

**Большая игра:**
1. Новый уровень: `game/src/Levels/LevelCity.cpp` → `void LoadCity(Scene& scene){ Prefabs::IndoorPlantRow(scene,{0,0,0},20); ... }`
2. В `Game::onInit()` → `LoadCity(scene());`
3. Префабы: кидай в `Prefabs/LevelPrefabs.h` — `IndoorPlantRow`, `StreetLight`, `Ground`.
4. Контент без кода: `assets/scenes/city.json` + `SceneLoader::load(scene(),"assets/scenes/city.json")`
5. Быстрый тест: `assets/models/huge.obj` 100k tris → проверит batching/FPS.

**Удобства:**
- `scene.createCube/TexturedModel/IndoorPlant/QuickSpawn` — 1 строка
- `Assets::QuickSpawn(scene,"my.obj")` — авто-текстура
- `TAB` браузер + `Drag&Drop` в окно + `F5` reload json
- `scene.createPost(Cinematic/Vibrant/Soft/Night)` + `1-4 B F [ ]` в рантайме

**Оптимизации для большой игры:** держи `scene.json` на уровень, юзай `Batcher` (одинаковые меши батчатся), текстуры 2048 + aniso, `PostProcessSettings` exposure/bloom tuned.
