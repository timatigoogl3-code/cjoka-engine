#pragma once
#include <string>
#include <memory>
#include <glm/glm.hpp>
#include "engine/ECS/Registry.h"

// miniaudio forward declaration
struct ma_engine;
struct ma_sound;

namespace Audio {

// ---------- Audio Components for ECS ----------
struct AudioListenerComponent {
    bool active = true;
};

struct AudioSourceComponent {
    std::string filepath;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
    bool spatial = true;      // 3D Spatial Audio
    float minDistance = 1.0f; // Attenuation starts
    float maxDistance = 30.0f;
    bool playOnStart = false;

    // Runtime handle
    std::shared_ptr<ma_sound> soundHandle;
    bool isPlaying = false;

    void play();
    void stop();
    void pause();
    void setVolume(float v);
    void setPitch(float p);
};

// ---------- AudioEngine Core ----------
class Engine {
public:
    static bool Init();
    static void Shutdown();
    static bool IsInitialized();

    // Глобальная громкость (0.0 .. 1.0)
    static void SetMasterVolume(float volume);
    static float GetMasterVolume();

    // Быстрое воспроизведение звука (2D / 3D OneShot)
    static void PlayOneShot(const std::string& filepath, float volume = 1.0f, float pitch = 1.0f);
    static void PlayOneShot3D(const std::string& filepath, const glm::vec3& position, float volume = 1.0f);

    // Установка позиции слушателя (камеры / игрока)
    static void SetListenerTransform(const glm::vec3& position, const glm::vec3& forward = glm::vec3(0,0,-1), const glm::vec3& up = glm::vec3(0,1,0));

    // Системный апдейт ECS (синхронизация позиций источников и слушателя)
    static void UpdateECS(Registry& reg);

    static ma_engine* GetNativeEngine();
};

} // namespace Audio
