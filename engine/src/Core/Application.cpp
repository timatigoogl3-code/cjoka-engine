#include "engine/Core/Application.h"
#include "engine/GUI/kGUI.h"
#include <GLFW/glfw3.h>
#include <iostream>

Application::Application(int w, int h, const char* title)
    : m_window(w, h, title) {
    int fbW, fbH; m_window.getFramebufferSize(fbW, fbH);
    kGUI::Init(fbW, fbH, nullptr, 26.0f);
    std::cout << "[Engine] Application created " << w << "x" << h << "\n";
}

Application::~Application() {
    onShutdown();
    kGUI::Shutdown();
    glfwTerminate();
    std::cout << "[Engine] Application shutdown\n";
}

void Application::run() {
    onInit();
    float last = m_window.getTime();
    while (!m_window.shouldClose()) {
        if (m_window.isKeyPressed(GLFW_KEY_ESCAPE)) m_window.setShouldClose(true);

        float now = m_window.getTime();
        float dt = now - last; last = now;

        onUpdate(dt);

        m_window.swapBuffers();
        m_window.pollEvents();
    }
}
