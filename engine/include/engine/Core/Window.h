#pragma once
#include <string>
#include <vector>
struct GLFWwindow;

class Window {
public:
    Window(int width, int height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void setShouldClose(bool v) const;
    void pollEvents() const;
    void swapBuffers() const;

    void getFramebufferSize(int& w, int& h) const;
    void getSize(int& w, int& h) const;

    bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;
    void getCursorPos(double& x, double& y) const;
    void setCursorMode(int mode) const;
    float getTime() const;

    GLFWwindow* handle() const { return m_window; }
    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    GLFWwindow* m_window = nullptr;
    int m_width = 0, m_height = 0;
};
