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
    void close() { m_running = false; }
    bool isRunning() const { return m_running; }

    // Жизненный цикл приложения (переопределяется игрой)
    virtual void onInit() = 0;
    virtual void onFixedUpdate(float fixedDt) { (void)fixedDt; } // Физика и детерминированная симуляция (по умолчанию 60Hz)
    virtual void onUpdate(float dt) = 0;                        // Геймплей, анимации, ввод
    virtual void onRender(float interpolation) { (void)interpolation; } // Дополнительные проходы рендера
    virtual void onImGuiRender() {}                             // Отрисовка Dear ImGui UI / инспекторов
    virtual void onShutdown() {}

    // Настройка фиксированного шага симуляции (например, 1/60 или 1/120)
    void setFixedTimestep(float fixedDt) { m_fixedDt = fixedDt; }
    float fixedTimestep() const { return m_fixedDt; }

    void setMaxFrameTime(float maxFt) { m_maxFrameTime = maxFt; }
    float maxFrameTime() const { return m_maxFrameTime; }

protected:
    Registry& registry() { return m_scene.registry(); }
    Scene& scene() { return m_scene; }
    Window& window() { return m_window; }

private:
    Window m_window;
    Scene m_scene;
    float m_fixedDt = 1.0f / 60.0f;
    float m_maxFrameTime = 0.25f; // Защита от «спирали смерти» при резких просадках
    bool m_running = true;
};
