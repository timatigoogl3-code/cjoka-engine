#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include "engine/Scene/Scene.h"

class SceneManager {
public:
    using SceneBuilder = std::function<void(Scene&)>;

    static void RegisterScene(const std::string& name, SceneBuilder builder);
    static bool LoadScene(const std::string& name, Scene& targetScene);
    static const std::string& GetActiveSceneName();
    static std::vector<std::string> GetRegisteredScenes();

private:
    static std::unordered_map<std::string, SceneBuilder> s_scenes;
    static std::string s_activeSceneName;
};
