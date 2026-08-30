#pragma once
#include <glm/glm.hpp>
#include <string>
#include <algorithm>
#include "engine/Environment/WorldEnvironment.h"
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "Environment/RainVFX.h"

namespace game {

enum class WeatherPreset {
    Clear = 0,
    PartlyCloudy,
    Overcast,
    Rain,
    HeavyRain,
    Thunderstorm,
    Foggy
};

class WeatherController {
public:
    static WeatherController& Get() {
        static WeatherController s_instance;
        return s_instance;
    }

    WeatherPreset currentPreset() const { return m_currentPreset; }
    WeatherPreset targetPreset() const { return m_targetPreset; }

    void setPreset(WeatherPreset preset, float transitionSec = 2.5f) {
        m_targetPreset = preset;
        m_transitionDuration = std::max(0.1f, transitionSec);
        m_transitionTimer = 0.0f;
    }

    void update(float dt, Registry& reg, const glm::vec3& camPos) {
        auto& env = cjoka::WorldEnvironment::Get();

        // 1. Smooth preset transition on the engine parameter bus
        if (m_currentPreset != m_targetPreset) {
            m_transitionTimer += dt;
            float t = glm::clamp(m_transitionTimer / m_transitionDuration, 0.0f, 1.0f);
            float targetPrecip = getPresetPrecipitation(m_targetPreset);
            float sourcePrecip = getPresetPrecipitation(m_currentPreset);
            env.precipitation = glm::mix(sourcePrecip, targetPrecip, t);

            if (t >= 1.0f) {
                m_currentPreset = m_targetPreset;
            }
        } else {
            env.precipitation = getPresetPrecipitation(m_currentPreset);
        }

        // 2. Road & Ground Wetness Accumulation / Evaporation
        if (env.precipitation > 0.05f) {
            env.groundWetness = std::min(1.0f, env.groundWetness + env.precipitation * 0.25f * dt);
        } else {
            env.groundWetness = std::max(0.0f, env.groundWetness - 0.035f * dt);
        }

        // 3. Selective Road & Outdoor Wetness (Targeted, NOT every mesh in the whole scene!)
        for (Entity e : reg.view<Tag, MeshRenderer>()) {
            auto& tag = reg.get<Tag>(e);
            if (tag.tag.find("Road") != std::string::npos || tag.tag.find("Ground") != std::string::npos || tag.tag.find("Outdoor") != std::string::npos) {
                auto& mr = reg.get<MeshRenderer>(e);
                mr.material.wetness = env.groundWetness;
            }
        }

        // 4. Lightning simulation for thunderstorm
        if (m_currentPreset == WeatherPreset::Thunderstorm || (m_targetPreset == WeatherPreset::Thunderstorm && env.precipitation > 0.5f)) {
            m_lightningTimer -= dt;
            if (m_lightningTimer <= 0.0f) {
                m_lightningActive = true;
                m_lightningIntensity = 5.0f + static_cast<float>(rand() % 100) / 15.0f;
                m_lightningTimer = 3.0f + static_cast<float>(rand() % 100) / 15.0f;
            }
            if (m_lightningActive) {
                m_lightningIntensity -= dt * 18.0f;
                if (m_lightningIntensity <= 0.0f) {
                    m_lightningActive = false;
                    m_lightningIntensity = 0.0f;
                }
            }
        } else {
            m_lightningActive = false;
            m_lightningIntensity = 0.0f;
        }

        // 5. Modulate Sun Light on lightning flash
        if (m_lightningActive) {
            for (Entity e : reg.view<DirectionalLight>()) {
                auto& dl = reg.get<DirectionalLight>(e);
                dl.intensity = 2.5f + m_lightningIntensity;
                dl.color = glm::vec3(0.9f, 0.95f, 1.2f);
            }
        }

        // 6. Update Game Rain VFX with scene obstacle and roof detection
        RainVFX::Get().update(dt, camPos, &reg);
    }

    void renderVFX(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos) {
        RainVFX::Get().render(view, proj, camPos);
    }

    float lightningIntensity() const { return m_lightningIntensity; }

private:
    float getPresetPrecipitation(WeatherPreset p) const {
        switch (p) {
            case WeatherPreset::Clear: return 0.0f;
            case WeatherPreset::PartlyCloudy: return 0.0f;
            case WeatherPreset::Overcast: return 0.05f;
            case WeatherPreset::Rain: return 0.65f;
            case WeatherPreset::HeavyRain: return 0.95f;
            case WeatherPreset::Thunderstorm: return 1.0f;
            case WeatherPreset::Foggy: return 0.15f;
        }
        return 0.0f;
    }

    WeatherPreset m_currentPreset = WeatherPreset::Clear;
    WeatherPreset m_targetPreset = WeatherPreset::Clear;
    float m_transitionTimer = 0.0f;
    float m_transitionDuration = 2.5f;

    bool m_lightningActive = false;
    float m_lightningTimer = 5.0f;
    float m_lightningIntensity = 0.0f;
};

} // namespace game
