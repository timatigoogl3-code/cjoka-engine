#pragma once
#include <imgui.h>
#include <ImGuizmo.h>

struct GLFWwindow;

namespace ImGuiLayer {
    void Init(GLFWwindow* window);
    void Shutdown();
    void Begin();
    void End();
    void ApplyDarkTheme();
}
