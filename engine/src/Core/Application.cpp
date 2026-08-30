#include "engine/Core/Application.h"
#include "engine/Core/Input.h"
#include "engine/Audio/AudioEngine.h"
#include "engine/GUI/kGUI.h"
#include "engine/GUI/ImGuiLayer.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>

Application::Application(int w, int h, const char* title)
    : m_window(w, h, title) {
    int fbW, fbH; m_window.getFramebufferSize(fbW, fbH);
    kGUI::Init(fbW, fbH, nullptr, 26.0f);
    ImGuiLayer::Init(m_window.handle());
    Input::Init(m_window.handle());
    Audio::Engine::Init();
    std::cout << "[Engine] Application created " << w << "x" << h << "\n";
}

Application::~Application() {
    onShutdown();
    Audio::Engine::Shutdown();
    ImGuiLayer::Shutdown();
    kGUI::Shutdown();
    glfwTerminate();
    std::cout << "[Engine] Application shutdown\n";
}

void Application::run() {
    onInit();
    float lastTime = m_window.getTime();
    float accumulator = 0.0f;

    while (m_running && !m_window.shouldClose()) {
        float currentTime = m_window.getTime();
        float frameTime = currentTime - lastTime;
        lastTime = currentTime;

        // Защита от резких спайков / отладки (Spiral of Death prevention)
        if (frameTime > m_maxFrameTime) {
            frameTime = m_maxFrameTime;
        }

        accumulator += frameTime;

        // 1. Опрос событий окна и обновление ввода
        m_window.pollEvents();
        Input::Update();

        // 2. Фиксированный шаг симуляции (физика, детерминированная логика)
        while (accumulator >= m_fixedDt) {
            onFixedUpdate(m_fixedDt);
            accumulator -= m_fixedDt;
        }

        // 3. Переменный шаг (логика, ввод, анимация)
        onUpdate(frameTime);

        // 4. Обновление пространственного 3D аудио в ECS
        Audio::Engine::UpdateECS(m_scene.registry());

        // 5. Проход отрисовки (с фактором интерполяции для сглаживания)
        float interpolation = (m_fixedDt > 0.0f) ? (accumulator / m_fixedDt) : 1.0f;
        onRender(interpolation);

        // 6. Отрисовка Dear ImGui слоя поверх сцены
        ImGuiLayer::Begin();
        onImGuiRender();
        ImGuiLayer::End();

        // 7. Своп буферов
        m_window.swapBuffers();
    }

    std::cout << "[Engine] Application run loop terminated! m_running=" << m_running 
              << ", shouldClose=" << m_window.shouldClose() << "\n";
}
