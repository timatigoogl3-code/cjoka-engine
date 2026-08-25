# assets/ — для БОЛЬШИХ игр

```
assets/
├── models/      → .obj/.fbx закинул → TAB → 1..9 → спавн, или Drag&Drop в окно, или QuickSpawn
├── textures/    → .png/.jpg закинул → авто-подхват (indoor_plant.obj → indoor_plant_COL.jpg)
├── scene.json   → без кода: добавь запись и F5 hot-reload
├── prefabs/     → json префабы (опционально)
├── scenes/      → уровни (scene_lvl1.json, scene_city.json)
├── shaders/     → .vert/.frag → sjoka.sh precompile shaders
└── fonts/       → .ttf
```

**Быстро добавить модель:**
1. Закинь `my.obj` (+ `my.mtl` + текстуры) в `assets/models/`
2. Текстуру в `assets/textures/my_COL.jpg`
3. Вариант A (код — 1 строка): `scene.createTexturedModel("assets/models/my.obj","assets/textures/my_COL.jpg",{{0,0,0}});`
4. Вариант B (без кода): добавь в `assets/scene.json`:
   ```json
   { "model":"assets/models/my.obj","texture":"assets/textures/my_COL.jpg","pos":[0,-0.35,-3],"scale":[1,1,1],"name":"MyObj" }
   ```
   F5 — hot-reload.
5. Вариант C (рантайм): перетащи файл в окно → авто-спавн перед камерой, TAB → браузер → 1..9

**Оптимизации уже включены:** `anisotropy 8x`, `mipmaps`, `batching` (одинаковые меши → 1 draw), `HDR Bloom+FXAA+vignette`, `GL4.6`.
**Красота:** `scene.createPost(PostProcessSettings::Cinematic())` → пресеты 1-4, `B` bloom `F` FXAA `[/]` exposure.
