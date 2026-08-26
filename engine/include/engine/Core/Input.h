#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

struct GLFWwindow;

class Input {
public:
    static void Init(GLFWwindow* window);
    static void Update();

    // Raw Key State
    static bool IsKeyPressed(int keyCode);
    static bool IsKeyJustPressed(int keyCode);
    static bool IsKeyJustReleased(int keyCode);

    // Raw Mouse State
    static bool IsMouseButtonPressed(int button);
    static bool IsMouseButtonJustPressed(int button);
    static glm::dvec2 GetMousePosition();
    static glm::vec2 GetMouseDelta();
    static void SetCursorLocked(bool locked);
    static bool IsCursorLocked();

    // Action & Axis Mapping
    static void MapAction(const std::string& actionName, int keyCode);
    static void MapAxis(const std::string& axisName, int positiveKey, int negativeKey);

    static bool IsActionPressed(const std::string& actionName);
    static bool IsActionJustPressed(const std::string& actionName);
    static float GetAxis(const std::string& axisName);

private:
    struct AxisMapping {
        int positiveKey;
        int negativeKey;
    };

    static GLFWwindow* s_window;
    static constexpr int MAX_KEYS = 512;
    static constexpr int MAX_BUTTONS = 16;

    static bool s_keys[MAX_KEYS];
    static bool s_prevKeys[MAX_KEYS];
    static bool s_mouseButtons[MAX_BUTTONS];
    static bool s_prevMouseButtons[MAX_BUTTONS];

    static glm::dvec2 s_mousePos;
    static glm::dvec2 s_prevMousePos;
    static glm::vec2 s_mouseDelta;
    static bool s_firstMouse;
    static bool s_cursorLocked;

    static std::unordered_map<std::string, std::vector<int>> s_actionMap;
    static std::unordered_map<std::string, AxisMapping> s_axisMap;
};
