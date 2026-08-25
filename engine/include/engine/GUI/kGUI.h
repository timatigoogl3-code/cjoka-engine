#pragma once
#include <string>
#include <glm/glm.hpp>
#include "engine/ECS/Registry.h"

class Window;
class Font;

// kGUI — простая retained+immediate GUI система для текста/панелей
namespace kGUI {

bool Init(int fbW, int fbH, const char* fontTTF = nullptr, float fontSize = 28.0f);
void Shutdown();
void SetWindowSize(int fbW, int fbH);
void BeginFrame();
void EndFrame(); // flush draw

// Immediate API — рисуй прямо в Game::onUpdate после 3D
void Text(float x, float y, float scale, const std::string& text, glm::vec4 color = {1,1,1,1});
void Panel(float x, float y, float w, float h, glm::vec4 color = {0.1f,0.1f,0.12f,0.85f}, float radius = 8.0f);

// ECS — отрисовка Text2D/Panel2D компонентов
void Draw(Registry& reg, const Window& win);

// Утилиты
Font* GetFont();
glm::vec2 Measure(const std::string& text, float scale = 1.0f);

} // namespace kGUI
