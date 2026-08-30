#include "engine/Core/Window.h"
#include <stdexcept>
#include <iostream>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

Window::Window(int width, int height, const char* title)
    : m_width(width), m_height(height) {
    if (!glfwInit()) throw std::runtime_error("glfwInit failed");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE); // 4.6 core
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
    if (!gladLoadGL(glfwGetProcAddress)) {
        throw std::runtime_error("gladLoadGL failed");
    }
    std::cout << "[Engine] GL " << (const char*)glGetString(GL_VERSION) << " GLSL " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
    glEnable(GL_DEPTH_TEST);
    // cull пока выкл — кубы/горшки имели смешанный winding, включаем только где нужно
    // glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // sRGB framebuffer если hdr pipeline не активен — оставляем linear для HDR
    // glEnable(GL_FRAMEBUFFER_SRGB); // HDR сам делает гамму

    glfwSetWindowCloseCallback(m_window, [](GLFWwindow*) {
        std::cout << "[Window] GLFW window close requested (close button or WM signal)!\n";
    });
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        // glfwTerminate вызывается в Application чтобы не убить раньше времени при нескольких окнах
    }
}

bool Window::shouldClose() const { return glfwWindowShouldClose(m_window); }
void Window::setShouldClose(bool v) const {
    std::cout << "[Window] setShouldClose(" << v << ") called!\n";
    glfwSetWindowShouldClose(m_window, v);
}
void Window::pollEvents() const { glfwPollEvents(); }
void Window::swapBuffers() const { glfwSwapBuffers(m_window); }
void Window::getFramebufferSize(int& w, int& h) const { glfwGetFramebufferSize(m_window, &w, &h); }
void Window::getSize(int& w, int& h) const { glfwGetWindowSize(m_window, &w, &h); }
bool Window::isKeyPressed(int key) const { return glfwGetKey(m_window, key) == GLFW_PRESS; }
bool Window::isMouseButtonPressed(int button) const { return glfwGetMouseButton(m_window, button) == GLFW_PRESS; }
void Window::getCursorPos(double& x, double& y) const { glfwGetCursorPos(m_window, &x, &y); }
void Window::setCursorMode(int mode) const { glfwSetInputMode(m_window, GLFW_CURSOR, mode); }
float Window::getTime() const { return (float)glfwGetTime(); }

static Window::DropCallback s_dropCallback;

void Window::setDropCallback(DropCallback cb) {
    s_dropCallback = std::move(cb);
    glfwSetDropCallback(m_window, [](GLFWwindow*, int count, const char** paths) {
        if (!s_dropCallback || count <= 0) return;
        std::vector<std::string> fileList;
        fileList.reserve(count);
        for (int i = 0; i < count; ++i) {
            if (paths[i]) fileList.emplace_back(paths[i]);
        }
        s_dropCallback(fileList);
    });
}
