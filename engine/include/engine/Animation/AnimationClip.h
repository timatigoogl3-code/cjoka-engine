#pragma once
#include <string>
#include <vector>
#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Animation {

struct KeyVec3 {
    float time = 0.0f; // seconds
    glm::vec3 value{0.0f};
};

struct KeyQuat {
    float time = 0.0f; // seconds
    glm::quat value{1.0f, 0.0f, 0.0f, 0.0f};
};

struct BoneTrack {
    int boneId = -1;
    std::string boneName;
    std::vector<KeyVec3> positions;
    std::vector<KeyQuat> rotations;
    std::vector<KeyVec3> scales;

    glm::vec3 samplePosition(float time) const {
        if (positions.empty()) return glm::vec3(0.0f);
        if (positions.size() == 1 || time <= positions.front().time) return positions.front().value;
        if (time >= positions.back().time) return positions.back().value;

        auto it = std::upper_bound(positions.begin(), positions.end(), time,
            [](float t, const KeyVec3& k) { return t < k.time; });
        size_t idx1 = static_cast<size_t>(it - positions.begin());
        size_t idx0 = idx1 > 0 ? idx1 - 1 : 0;
        float t0 = positions[idx0].time;
        float t1 = positions[idx1].time;
        float factor = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
        return glm::mix(positions[idx0].value, positions[idx1].value, factor);
    }

    glm::quat sampleRotation(float time) const {
        if (rotations.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        if (rotations.size() == 1 || time <= rotations.front().time) return rotations.front().value;
        if (time >= rotations.back().time) return rotations.back().value;

        auto it = std::upper_bound(rotations.begin(), rotations.end(), time,
            [](float t, const KeyQuat& k) { return t < k.time; });
        size_t idx1 = static_cast<size_t>(it - rotations.begin());
        size_t idx0 = idx1 > 0 ? idx1 - 1 : 0;
        float t0 = rotations[idx0].time;
        float t1 = rotations[idx1].time;
        float factor = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
        return glm::slerp(rotations[idx0].value, rotations[idx1].value, factor);
    }

    glm::vec3 sampleScale(float time) const {
        if (scales.empty()) return glm::vec3(1.0f);
        if (scales.size() == 1 || time <= scales.front().time) return scales.front().value;
        if (time >= scales.back().time) return scales.back().value;

        auto it = std::upper_bound(scales.begin(), scales.end(), time,
            [](float t, const KeyVec3& k) { return t < k.time; });
        size_t idx1 = static_cast<size_t>(it - scales.begin());
        size_t idx0 = idx1 > 0 ? idx1 - 1 : 0;
        float t0 = scales[idx0].time;
        float t1 = scales[idx1].time;
        float factor = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
        return glm::mix(scales[idx0].value, scales[idx1].value, factor);
    }

    glm::mat4 sampleTransform(float time) const {
        glm::vec3 pos = samplePosition(time);
        glm::quat rot = sampleRotation(time);
        glm::vec3 scl = sampleScale(time);

        glm::mat4 t = glm::translate(glm::mat4(1.0f), pos);
        glm::mat4 r = glm::toMat4(rot);
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scl);
        return t * r * s;
    }
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f; // seconds
    float frameRate = 30.0f;
    std::vector<BoneTrack> tracks;

    const BoneTrack* findTrack(int boneId) const {
        for (const auto& track : tracks) {
            if (track.boneId == boneId) return &track;
        }
        return nullptr;
    }

    const BoneTrack* findTrack(const std::string& boneName) const {
        for (const auto& track : tracks) {
            if (track.boneName == boneName) return &track;
        }
        return nullptr;
    }
};

} // namespace Animation
