#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "engine/Audio/AudioEngine.h"
#include "engine/ECS/Components.h"
#include <iostream>

namespace Audio {

static ma_engine s_maEngine;
static bool s_initialized = false;

bool Engine::Init() {
    if (s_initialized) return true;

    ma_engine_config config = ma_engine_config_init();
    ma_result result = ma_engine_init(&config, &s_maEngine);
    if (result != MA_SUCCESS) {
        std::cerr << "[AudioEngine] Failed to initialize miniaudio engine: " << result << "\n";
        return false;
    }

    s_initialized = true;
    std::cout << "[AudioEngine] Spatial 3D Audio Engine initialized successfully\n";
    return true;
}

void Engine::Shutdown() {
    if (!s_initialized) return;
    ma_engine_uninit(&s_maEngine);
    s_initialized = false;
    std::cout << "[AudioEngine] Audio Engine shutdown\n";
}

bool Engine::IsInitialized() {
    return s_initialized;
}

void Engine::SetMasterVolume(float volume) {
    if (!s_initialized) return;
    ma_engine_set_volume(&s_maEngine, volume);
}

float Engine::GetMasterVolume() {
    if (!s_initialized) return 0.0f;
    return ma_engine_get_volume(&s_maEngine);
}

void Engine::PlayOneShot(const std::string& filepath, float volume, float pitch) {
    if (!s_initialized) return;
    ma_sound sound;
    ma_result result = ma_sound_init_from_file(&s_maEngine, filepath.c_str(), 0, NULL, NULL, &sound);
    if (result == MA_SUCCESS) {
        ma_sound_set_volume(&sound, volume);
        ma_sound_set_pitch(&sound, pitch);
        ma_sound_start(&sound);
    }
}

void Engine::PlayOneShot3D(const std::string& filepath, const glm::vec3& position, float volume) {
    if (!s_initialized) return;
    ma_sound sound;
    ma_result result = ma_sound_init_from_file(&s_maEngine, filepath.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL, &sound);
    if (result == MA_SUCCESS) {
        ma_sound_set_spatialization_enabled(&sound, MA_TRUE);
        ma_sound_set_position(&sound, position.x, position.y, position.z);
        ma_sound_set_volume(&sound, volume);
        ma_sound_start(&sound);
    }
}

void Engine::SetListenerTransform(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) {
    if (!s_initialized) return;
    ma_engine_listener_set_position(&s_maEngine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&s_maEngine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&s_maEngine, 0, up.x, up.y, up.z);
}

void Engine::UpdateECS(Registry& reg) {
    if (!s_initialized) return;

    // 1. Позиционирование слушателя
    for (Entity e : reg.view<AudioListenerComponent, Transform>()) {
        auto& tr = reg.get<Transform>(e);
        auto& listener = reg.get<AudioListenerComponent>(e);
        if (listener.active) {
            SetListenerTransform(tr.position, tr.forward(), tr.up());
            break; // Первый активный слушатель
        }
    }

    // 2. Позиционирование и управление источниками звука
    for (Entity e : reg.view<AudioSourceComponent, Transform>()) {
        auto& src = reg.get<AudioSourceComponent>(e);
        auto& tr = reg.get<Transform>(e);

        if (!src.soundHandle && !src.filepath.empty()) {
            auto soundPtr = std::shared_ptr<ma_sound>(new ma_sound(), [](ma_sound* s) {
                if (s) {
                    ma_sound_uninit(s);
                    delete s;
                }
            });

            ma_result res = ma_sound_init_from_file(&s_maEngine, src.filepath.c_str(), 0, NULL, NULL, soundPtr.get());
            if (res == MA_SUCCESS) {
                src.soundHandle = soundPtr;
                ma_sound_set_looping(src.soundHandle.get(), src.loop ? MA_TRUE : MA_FALSE);
                ma_sound_set_spatialization_enabled(src.soundHandle.get(), src.spatial ? MA_TRUE : MA_FALSE);
                ma_sound_set_min_distance(src.soundHandle.get(), src.minDistance);
                ma_sound_set_max_distance(src.soundHandle.get(), src.maxDistance);
                ma_sound_set_volume(src.soundHandle.get(), src.volume);
                ma_sound_set_pitch(src.soundHandle.get(), src.pitch);

                if (src.playOnStart) {
                    src.play();
                }
            }
        }

        if (src.soundHandle) {
            if (src.spatial) {
                ma_sound_set_position(src.soundHandle.get(), tr.position.x, tr.position.y, tr.position.z);
            }
            src.isPlaying = ma_sound_is_playing(src.soundHandle.get()) == MA_TRUE;
        }
    }
}

ma_engine* Engine::GetNativeEngine() {
    return s_initialized ? &s_maEngine : nullptr;
}

void AudioSourceComponent::play() {
    if (soundHandle) {
        ma_sound_start(soundHandle.get());
        isPlaying = true;
    }
}

void AudioSourceComponent::stop() {
    if (soundHandle) {
        ma_sound_stop(soundHandle.get());
        ma_sound_seek_to_pcm_frame(soundHandle.get(), 0);
        isPlaying = false;
    }
}

void AudioSourceComponent::pause() {
    if (soundHandle) {
        ma_sound_stop(soundHandle.get());
        isPlaying = false;
    }
}

void AudioSourceComponent::setVolume(float v) {
    volume = v;
    if (soundHandle) {
        ma_sound_set_volume(soundHandle.get(), v);
    }
}

void AudioSourceComponent::setPitch(float p) {
    pitch = p;
    if (soundHandle) {
        ma_sound_set_pitch(soundHandle.get(), p);
    }
}

} // namespace Audio
