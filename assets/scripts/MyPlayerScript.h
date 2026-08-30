#pragma once
#include "engine/Scripting/ScriptableEntity.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <iostream>

class MyPlayerScript : public ScriptableEntity {
public:
    float speed = 5.0f;
    bool active = true;

    void onStart() override {
        std::cout << "[MyPlayerScript] Started!\n";
    }

    void onUpdate(float dt) override {
        (void)dt;
        if (!active) return;
        // Custom gameplay logic here
    }

    void onInspectorGUI() override {
        ImGui::Text("MyPlayerScript Parameters");
        ImGui::Checkbox("Active", &active);
        ImGui::SliderFloat("Speed", &speed, 0.0f, 50.0f);
    }
};
REGISTER_SCRIPT(MyPlayerScript, "MyPlayerScript")
