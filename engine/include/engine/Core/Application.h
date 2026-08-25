#pragma once
#include "Window.h"
#include "engine/Scene/Scene.h"

class Application {
public:
    Application(int width, int height, const char* title);
    virtual ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

    // Игровая логика — переопределяет game-слой
    virtual void onInit() = 0;
    virtual void onUpdate(float dt) = 0;
    virtual void onShutdown() {}

protected:
    Registry& registry() { return m_scene.registry(); }
    Scene& scene() { return m_scene; }
    Window& window() { return m_window; }

private:
    Window m_window;
    Scene m_scene;
};
