# cjoka — модульный игровой движок (C++20 / Clang / OpenGL 3.3)

```
cjoka/
├── CMakeLists.txt          # корень: C++20, clang, add_subdirectory(engine, game)
├── CMakePresets.json       # clang-debug / clang-release (Ninja)
├── engine/                 # СТАТИЧЕСКАЯ БИБЛИОТЕКА — не пересобирается при правке игры
│   ├── CMakeLists.txt      # libcjoka_engine.a + libglad.a
│   ├── third_party/glad/   # glad 2.0.8 gl:core=3.3 + loader (glad --api gl:core=3.3 --out-path third_party/glad c --loader)
│   └── include/engine/
│       ├── Engine.h        # umbrella: #include <engine/Engine.h>
│       ├── Core/Window.h, Application.h
│       ├── Renderer/Shader.h, Mesh3D.h, Renderer.h
│       └── ECS/Registry.h, Components.h, Systems.h
├── game/                   # ИГРОВАЯ ЛОГИКА — правишь только тут, линковка без пересборки движка
│   ├── CMakeLists.txt      # exe cjoka -> PRIVATE cjoka_engine
│   └── src/Game.h, Game.cpp, main.cpp
└── build/                  # Ninja out (gitignore)
```

## Требования
`clang 21`, `cmake 4.2+`, `ninja`, `libglfw3-dev`, `libglm-dev`, `libopengl-dev`

```bash
sudo apt install libglfw3-dev libglm-dev libopengl-dev
```

## Сборка (через sjoka.sh)

```bash
./sjoka.sh compile engine          # только движок  -> build/engine/libcjoka_engine.a
./sjoka.sh compile engine --release
./sjoka.sh compile game            # игра + движок  -> build/cjoka
./sjoka.sh compile all --release   # всё
./sjoka.sh play game               # собрать и запустить (требует DISPLAY)
./sjoka.sh precompile shaders      # шейдеры → binary (.spv + .h + bundle)
./sjoka.sh clean
```

Или напрямую cmake:
```bash
cmake --preset clang-debug && cmake --build --preset debug    # -> build/cjoka
cmake --preset clang-release && cmake --build --preset release
./build/cjoka
```

Инкрементальная сборка: `touch game/src/Game.cpp && ./sjoka.sh compile game` — пересоберется только `Game.cpp.o` + линковка, `engine/libcjoka_engine.a` не трогается.

## Шейдеры → binary

```bash
./sjoka.sh precompile shaders          # ищет assets/shaders/*.vert/*.frag
# → build/shaders/*.spv (если есть glslc/glslang) + build/shaders/shaders.h (xxd) + shaders.bundle
# → дубль в assets/shaders/binary/ (для коммита)
# Подключи в движке: #include "build/shaders/shaders.h"
```

## Как писать игру

```cpp
// game/src/Game.cpp
void Game::onInit() {
    Entity e = registry().create();
    registry().emplace<Transform>(e, Transform{{0,0,0}});
    registry().emplace<MeshRenderer>(e, MeshRenderer{m_cube});
}
void Game::onUpdate(float dt) {
    registry().get<Transform>(e).rotation.y += 40*dt;
    Systems::Render(registry(), *m_shader, view, proj);
}
```

Новый файл → добавь в `game/CMakeLists.txt: add_executable(cjoka src/MySystem.cpp ...)`.

## Стек
`C++20`, `Clang 21`, `OpenGL 3.3 Core`, `GLFW 3.4`, `GLAD 2`, `GLM`, `Ninja`, `CMake 3.20+`
