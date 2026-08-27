#include "engine/GUI/ImGuiLayer.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <GLFW/glfw3.h>
#include <iostream>

namespace ImGuiLayer {

static bool s_initialized = false;

void ApplyDarkTheme() {
    auto& style = ImGui::GetStyle();
    auto& colors = style.Colors;

    style.WindowRounding = 6.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    // Modern Deep Dark theme (Graphite & Neon Cyan accent)
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.52f, 0.56f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.10f, 0.11f, 0.13f, 0.94f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.08f, 0.09f, 0.10f, 0.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.12f, 0.13f, 0.15f, 0.96f);
    colors[ImGuiCol_Border]                = ImVec4(0.22f, 0.24f, 0.28f, 0.65f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.16f, 0.17f, 0.20f, 0.85f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.28f, 0.33f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.06f, 0.06f, 0.08f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.09f, 0.10f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.24f, 0.26f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.32f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.40f, 0.44f, 0.50f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.00f, 0.80f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.00f, 0.75f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.25f, 0.28f, 0.34f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.00f, 0.65f, 0.78f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.18f, 0.20f, 0.24f, 0.80f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.00f, 0.65f, 0.78f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.22f, 0.24f, 0.28f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.00f, 0.75f, 0.85f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.20f, 0.22f, 0.26f, 0.40f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.00f, 0.75f, 0.85f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.00f, 0.90f, 1.00f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.12f, 0.13f, 0.16f, 0.86f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.22f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.10f, 0.11f, 0.13f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.00f, 0.80f, 0.90f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.15f, 0.16f, 0.19f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.22f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.18f, 0.19f, 0.22f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.00f, 0.65f, 0.78f, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.00f, 0.85f, 0.95f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.00f, 0.80f, 0.90f, 1.00f);
}

void Init(GLFWwindow* window) {
    if (s_initialized) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ApplyDarkTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    s_initialized = true;
    std::cout << "[ImGuiLayer] Dear ImGui initialized successfully\n";
}

void Shutdown() {
    if (!s_initialized) return;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    s_initialized = false;
    std::cout << "[ImGuiLayer] Dear ImGui shutdown\n";
}

void Begin() {
    if (!s_initialized) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void End() {
    if (!s_initialized) return;

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace ImGuiLayer
