#include "engine/Scene/SceneManager.h"
#include <iostream>

std::unordered_map<std::string, SceneManager::SceneBuilder> SceneManager::s_scenes;
std::string SceneManager::s_activeSceneName = "Default";

void SceneManager::RegisterScene(const std::string& name, SceneBuilder builder) {
    s_scenes[name] = std::move(builder);
}

bool SceneManager::LoadScene(const std::string& name, Scene& targetScene) {
    auto it = s_scenes.find(name);
    if (it == s_scenes.end()) {
        std::cerr << "[SceneManager] Scene not found: " << name << "\n";
        return false;
    }

    targetScene.clear();
    it->second(targetScene);
    s_activeSceneName = name;
    std::cout << "[SceneManager] Loaded scene: " << name << " (" << targetScene.registry().aliveCount() << " entities)\n";
    return true;
}

const std::string& SceneManager::GetActiveSceneName() {
    return s_activeSceneName;
}

std::vector<std::string> SceneManager::GetRegisteredScenes() {
    std::vector<std::string> list;
    list.reserve(s_scenes.size());
    for (const auto& kv : s_scenes) {
        list.push_back(kv.first);
    }
    return list;
}
