#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Animation {

enum class InterpMode : int {
    Linear = 0,
    Smooth = 1,   // Cubic Hermite / Smoothstep
    Constant = 2  // Step
};

struct FloatKey {
    float time = 0.0f;
    float value = 0.0f;
    InterpMode interp = InterpMode::Smooth;
};

struct Vec3Key {
    float time = 0.0f;
    glm::vec3 value{0.0f};
    InterpMode interp = InterpMode::Smooth;
};

class AnimFile {
public:
    std::string name = "NewAnimation";
    float duration = 5.0f; // seconds (no arbitrary limits)
    float frameRate = 30.0f;
    bool isLooping = true;

    // Transform channels
    std::vector<Vec3Key> positionKeys;
    std::vector<Vec3Key> rotationKeys; // in degrees (pitch, yaw, roll)
    std::vector<Vec3Key> scaleKeys;

    // Arbitrary named float channels (e.g. "Camera.FOV", "Light.Intensity")
    std::unordered_map<std::string, std::vector<FloatKey>> floatChannels;

    AnimFile() = default;
    AnimFile(const std::string& animName, float dur = 5.0f, float fps = 30.0f)
        : name(animName), duration(dur), frameRate(fps) {}

    // --- Sampling Methods ---
    glm::vec3 samplePosition(float time, const glm::vec3& defaultVal = glm::vec3(0.0f)) const {
        return sampleVec3Track(positionKeys, time, defaultVal);
    }

    glm::vec3 sampleRotation(float time, const glm::vec3& defaultVal = glm::vec3(0.0f)) const {
        return sampleVec3Track(rotationKeys, time, defaultVal);
    }

    glm::vec3 sampleScale(float time, const glm::vec3& defaultVal = glm::vec3(1.0f)) const {
        return sampleVec3Track(scaleKeys, time, defaultVal);
    }

    float sampleFloat(const std::string& channel, float time, float defaultVal = 0.0f) const {
        auto it = floatChannels.find(channel);
        if (it == floatChannels.end() || it->second.empty()) return defaultVal;
        return sampleFloatTrack(it->second, time, defaultVal);
    }

    // --- Keyframe Insertion / Mutation ---
    void addPositionKey(float time, const glm::vec3& val, InterpMode interp = InterpMode::Smooth) {
        insertVec3Key(positionKeys, time, val, interp);
        if (time > duration) duration = time;
    }

    void addRotationKey(float time, const glm::vec3& val, InterpMode interp = InterpMode::Smooth) {
        insertVec3Key(rotationKeys, time, val, interp);
        if (time > duration) duration = time;
    }

    void addScaleKey(float time, const glm::vec3& val, InterpMode interp = InterpMode::Smooth) {
        insertVec3Key(scaleKeys, time, val, interp);
        if (time > duration) duration = time;
    }

    void addFloatKey(const std::string& channel, float time, float val, InterpMode interp = InterpMode::Smooth) {
        insertFloatKey(floatChannels[channel], time, val, interp);
        if (time > duration) duration = time;
    }

    void clearAllKeys() {
        positionKeys.clear();
        rotationKeys.clear();
        scaleKeys.clear();
        floatChannels.clear();
    }

    // --- File Serialization (.anim JSON format) ---
    bool saveToFile(const std::string& path) const {
        std::ofstream f(path);
        if (!f.is_open()) return false;

        f << "{\n";
        f << "  \"name\": \"" << name << "\",\n";
        f << "  \"duration\": " << duration << ",\n";
        f << "  \"frameRate\": " << frameRate << ",\n";
        f << "  \"loop\": " << (isLooping ? "true" : "false") << ",\n";

        // Position Keys
        f << "  \"position\": [\n";
        for (size_t i = 0; i < positionKeys.size(); ++i) {
            const auto& k = positionKeys[i];
            f << "    { \"t\": " << k.time << ", \"val\": [" << k.value.x << ", " << k.value.y << ", " << k.value.z << "], \"interp\": " << (int)k.interp << " }"
              << (i + 1 < positionKeys.size() ? ",\n" : "\n");
        }
        f << "  ],\n";

        // Rotation Keys
        f << "  \"rotation\": [\n";
        for (size_t i = 0; i < rotationKeys.size(); ++i) {
            const auto& k = rotationKeys[i];
            f << "    { \"t\": " << k.time << ", \"val\": [" << k.value.x << ", " << k.value.y << ", " << k.value.z << "], \"interp\": " << (int)k.interp << " }"
              << (i + 1 < rotationKeys.size() ? ",\n" : "\n");
        }
        f << "  ],\n";

        // Scale Keys
        f << "  \"scale\": [\n";
        for (size_t i = 0; i < scaleKeys.size(); ++i) {
            const auto& k = scaleKeys[i];
            f << "    { \"t\": " << k.time << ", \"val\": [" << k.value.x << ", " << k.value.y << ", " << k.value.z << "], \"interp\": " << (int)k.interp << " }"
              << (i + 1 < scaleKeys.size() ? ",\n" : "\n");
        }
        f << "  ],\n";

        // Float Channels
        f << "  \"floatChannels\": {\n";
        size_t chIdx = 0;
        for (const auto& [cName, keys] : floatChannels) {
            f << "    \"" << cName << "\": [\n";
            for (size_t i = 0; i < keys.size(); ++i) {
                const auto& k = keys[i];
                f << "      { \"t\": " << k.time << ", \"val\": " << k.value << ", \"interp\": " << (int)k.interp << " }"
                  << (i + 1 < keys.size() ? ",\n" : "\n");
            }
            f << "    ]" << (++chIdx < floatChannels.size() ? ",\n" : "\n");
        }
        f << "  }\n";
        f << "}\n";

        std::cout << "[AnimFile] Saved animation clip to " << path << " (" << duration << "s)\n";
        return true;
    }

    bool loadFromFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return false;

        clearAllKeys();
        std::string line;
        std::string currentArray = "";
        std::string currentChannel = "";

        while (std::getline(f, line)) {
            auto start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            std::string trimmed = line.substr(start);

            if (trimmed.find("\"name\":") != std::string::npos) {
                size_t q1 = trimmed.find('"', 7);
                size_t q2 = trimmed.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos)
                    name = trimmed.substr(q1 + 1, q2 - q1 - 1);
            } else if (trimmed.find("\"duration\":") != std::string::npos) {
                sscanf(trimmed.c_str(), "\"duration\": %f", &duration);
            } else if (trimmed.find("\"frameRate\":") != std::string::npos) {
                sscanf(trimmed.c_str(), "\"frameRate\": %f", &frameRate);
            } else if (trimmed.find("\"loop\":") != std::string::npos) {
                isLooping = (trimmed.find("true") != std::string::npos);
            } else if (trimmed.find("\"position\": [") != std::string::npos) {
                currentArray = "position";
            } else if (trimmed.find("\"rotation\": [") != std::string::npos) {
                currentArray = "rotation";
            } else if (trimmed.find("\"scale\": [") != std::string::npos) {
                currentArray = "scale";
            } else if (trimmed.find("]") != std::string::npos) {
                currentArray = "";
                currentChannel = "";
            } else if (!currentArray.empty() && trimmed.find("{ \"t\":") != std::string::npos) {
                float t = 0.0f, x = 0.0f, y = 0.0f, z = 0.0f;
                int interp = 1;
                size_t valPos = trimmed.find("\"val\": [");
                if (valPos != std::string::npos) {
                    sscanf(trimmed.c_str(), "{ \"t\": %f, \"val\": [%f, %f, %f], \"interp\": %d", &t, &x, &y, &z, &interp);
                    glm::vec3 val{x, y, z};
                    if (currentArray == "position") positionKeys.push_back({t, val, (InterpMode)interp});
                    else if (currentArray == "rotation") rotationKeys.push_back({t, val, (InterpMode)interp});
                    else if (currentArray == "scale") scaleKeys.push_back({t, val, (InterpMode)interp});
                }
            }
        }

        std::sort(positionKeys.begin(), positionKeys.end(), [](const Vec3Key& a, const Vec3Key& b) { return a.time < b.time; });
        std::sort(rotationKeys.begin(), rotationKeys.end(), [](const Vec3Key& a, const Vec3Key& b) { return a.time < b.time; });
        std::sort(scaleKeys.begin(), scaleKeys.end(), [](const Vec3Key& a, const Vec3Key& b) { return a.time < b.time; });

        std::cout << "[AnimFile] Loaded animation clip '" << name << "' from " << path << " (" << duration << "s, "
                  << positionKeys.size() + rotationKeys.size() + scaleKeys.size() << " keys)\n";
        return true;
    }

    static std::shared_ptr<AnimFile> Load(const std::string& path) {
        static std::unordered_map<std::string, std::shared_ptr<AnimFile>> s_cache;
        auto it = s_cache.find(path);
        if (it != s_cache.end() && it->second) return it->second;

        auto anim = std::make_shared<AnimFile>();
        if (anim->loadFromFile(path)) {
            s_cache[path] = anim;
            return anim;
        }
        return nullptr;
    }

private:
    static void insertVec3Key(std::vector<Vec3Key>& keys, float time, const glm::vec3& val, InterpMode interp) {
        for (auto& k : keys) {
            if (std::abs(k.time - time) < 0.001f) {
                k.value = val;
                k.interp = interp;
                return;
            }
        }
        keys.push_back({time, val, interp});
        std::sort(keys.begin(), keys.end(), [](const Vec3Key& a, const Vec3Key& b) { return a.time < b.time; });
    }

    static void insertFloatKey(std::vector<FloatKey>& keys, float time, float val, InterpMode interp) {
        for (auto& k : keys) {
            if (std::abs(k.time - time) < 0.001f) {
                k.value = val;
                k.interp = interp;
                return;
            }
        }
        keys.push_back({time, val, interp});
        std::sort(keys.begin(), keys.end(), [](const FloatKey& a, const FloatKey& b) { return a.time < b.time; });
    }

    static glm::vec3 sampleVec3Track(const std::vector<Vec3Key>& keys, float time, const glm::vec3& defaultVal) {
        if (keys.empty()) return defaultVal;
        if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
        if (time >= keys.back().time) return keys.back().value;

        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            if (time >= keys[i].time && time <= keys[i + 1].time) {
                float segDur = keys[i + 1].time - keys[i].time;
                float t = (segDur > 0.0001f) ? (time - keys[i].time) / segDur : 0.0f;
                if (keys[i].interp == InterpMode::Constant) {
                    return keys[i].value;
                } else if (keys[i].interp == InterpMode::Smooth) {
                    t = t * t * (3.0f - 2.0f * t);
                }
                return glm::mix(keys[i].value, keys[i + 1].value, t);
            }
        }
        return keys.back().value;
    }

    static float sampleFloatTrack(const std::vector<FloatKey>& keys, float time, float defaultVal) {
        if (keys.empty()) return defaultVal;
        if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
        if (time >= keys.back().time) return keys.back().value;

        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            if (time >= keys[i].time && time <= keys[i + 1].time) {
                float segDur = keys[i + 1].time - keys[i].time;
                float t = (segDur > 0.0001f) ? (time - keys[i].time) / segDur : 0.0f;
                if (keys[i].interp == InterpMode::Constant) {
                    return keys[i].value;
                } else if (keys[i].interp == InterpMode::Smooth) {
                    t = t * t * (3.0f - 2.0f * t);
                }
                return glm::mix(keys[i].value, keys[i + 1].value, t);
            }
        }
        return keys.back().value;
    }
};

} // namespace Animation
