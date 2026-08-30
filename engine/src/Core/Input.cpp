#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "engine/Core/Input.h"
#include <cstring>
#include <iostream>

GLFWwindow* Input::s_window = nullptr;
bool Input::s_keys[Input::MAX_KEYS] = {false};
bool Input::s_prevKeys[Input::MAX_KEYS] = {false};
bool Input::s_mouseButtons[Input::MAX_BUTTONS] = {false};
bool Input::s_prevMouseButtons[Input::MAX_BUTTONS] = {false};

glm::dvec2 Input::s_mousePos = {0.0, 0.0};
glm::dvec2 Input::s_prevMousePos = {0.0, 0.0};
glm::vec2 Input::s_mouseDelta = {0.0f, 0.0f};
bool Input::s_firstMouse = true;
bool Input::s_cursorLocked = false;

std::unordered_map<std::string, std::vector<int>> Input::s_actionMap;
std::unordered_map<std::string, Input::AxisMapping> Input::s_axisMap;

void Input::Init(GLFWwindow* window) {
    s_window = window;
    std::memset(s_keys, 0, sizeof(s_keys));
    std::memset(s_prevKeys, 0, sizeof(s_prevKeys));
    std::memset(s_mouseButtons, 0, sizeof(s_mouseButtons));
    std::memset(s_prevMouseButtons, 0, sizeof(s_prevMouseButtons));
    s_firstMouse = true;
    s_cursorLocked = false;

    // Default Action Mappings
    MapAction("Forward", GLFW_KEY_W);
    MapAction("Backward", GLFW_KEY_S);
    MapAction("Left", GLFW_KEY_A);
    MapAction("Right", GLFW_KEY_D);
    MapAction("Jump", GLFW_KEY_SPACE);
    MapAction("Sprint", GLFW_KEY_LEFT_SHIFT);
    MapAction("Interact", GLFW_KEY_E);
    MapAction("Throw", GLFW_KEY_F);
    MapAction("ToggleView", GLFW_KEY_V);

    // Default Axis Mappings
    MapAxis("Horizontal", GLFW_KEY_D, GLFW_KEY_A);
    MapAxis("Vertical", GLFW_KEY_W, GLFW_KEY_S);

    std::cout << "[Input] Action & Axis Mapping system initialized\n";
}

void Input::Update() {
    if (!s_window) return;

    // Save previous state
    std::memcpy(s_prevKeys, s_keys, sizeof(s_keys));
    std::memcpy(s_prevMouseButtons, s_mouseButtons, sizeof(s_mouseButtons));

    // Update current keys (GLFW keys range from GLFW_KEY_SPACE=32 to GLFW_KEY_LAST=348)
    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST && key < MAX_KEYS; ++key) {
        s_keys[key] = (glfwGetKey(s_window, key) == GLFW_PRESS);
    }

    // Update mouse buttons
    for (int btn = 0; btn < MAX_BUTTONS; ++btn) {
        s_mouseButtons[btn] = (glfwGetMouseButton(s_window, btn) == GLFW_PRESS);
    }

    // Update mouse position and delta
    double x, y;
    glfwGetCursorPos(s_window, &x, &y);
    s_mousePos = {x, y};

    if (s_firstMouse) {
        s_prevMousePos = s_mousePos;
        s_firstMouse = false;
    }

    s_mouseDelta = glm::vec2(static_cast<float>(s_mousePos.x - s_prevMousePos.x),
                             static_cast<float>(s_prevMousePos.y - s_mousePos.y)); // standard OpenGL Y-up delta
    s_prevMousePos = s_mousePos;
}

bool Input::IsKeyPressed(int keyCode) {
    if (keyCode < 0 || keyCode >= MAX_KEYS) return false;
    return s_keys[keyCode];
}

bool Input::IsKeyJustPressed(int keyCode) {
    if (keyCode < 0 || keyCode >= MAX_KEYS) return false;
    return s_keys[keyCode] && !s_prevKeys[keyCode];
}

bool Input::IsKeyJustReleased(int keyCode) {
    if (keyCode < 0 || keyCode >= MAX_KEYS) return false;
    return !s_keys[keyCode] && s_prevKeys[keyCode];
}

bool Input::IsMouseButtonPressed(int button) {
    if (button < 0 || button >= MAX_BUTTONS) return false;
    return s_mouseButtons[button];
}

bool Input::IsMouseButtonJustPressed(int button) {
    if (button < 0 || button >= MAX_BUTTONS) return false;
    return s_mouseButtons[button] && !s_prevMouseButtons[button];
}

glm::dvec2 Input::GetMousePosition() {
    return s_mousePos;
}

glm::vec2 Input::GetMouseDelta() {
    return s_mouseDelta;
}

void Input::SetCursorLocked(bool locked) {
    s_cursorLocked = locked;
    if (s_window) {
        glfwSetInputMode(s_window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        s_firstMouse = true;
    }
}

bool Input::IsCursorLocked() {
    return s_cursorLocked;
}

void Input::MapAction(const std::string& actionName, int keyCode) {
    s_actionMap[actionName].push_back(keyCode);
}

void Input::MapAxis(const std::string& axisName, int positiveKey, int negativeKey) {
    s_axisMap[axisName] = AxisMapping{positiveKey, negativeKey};
}

bool Input::IsActionPressed(const std::string& actionName) {
    auto it = s_actionMap.find(actionName);
    if (it == s_actionMap.end()) return false;
    for (int key : it->second) {
        if (IsKeyPressed(key)) return true;
    }
    return false;
}

bool Input::IsActionJustPressed(const std::string& actionName) {
    auto it = s_actionMap.find(actionName);
    if (it == s_actionMap.end()) return false;
    for (int key : it->second) {
        if (IsKeyJustPressed(key)) return true;
    }
    return false;
}

float Input::GetAxis(const std::string& axisName) {
    auto it = s_axisMap.find(axisName);
    if (it == s_axisMap.end()) return 0.0f;
    float value = 0.0f;
    if (IsKeyPressed(it->second.positiveKey)) value += 1.0f;
    if (IsKeyPressed(it->second.negativeKey)) value -= 1.0f;
    return value;
}
