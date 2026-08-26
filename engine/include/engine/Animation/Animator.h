#pragma once
#include "engine/Animation/SkinnedMesh.h"
#include <vector>
#include <memory>
#include <string>
#include <iostream>

namespace Animation {

class Animator {
public:
    static constexpr size_t MAX_BONES = 128;

    Animator() {
        m_finalBoneMatrices.resize(MAX_BONES, glm::mat4(1.0f));
    }

    explicit Animator(std::shared_ptr<SkinnedMesh> mesh) : Animator() {
        setMesh(std::move(mesh));
    }

    void setMesh(std::shared_ptr<SkinnedMesh> mesh) {
        m_mesh = std::move(mesh);
        if (m_mesh && !m_mesh->animations().empty()) {
            play(m_mesh->animations()[0].name);
        }
    }

    void play(const std::string& clipName, bool loop = true, float blendTime = 0.25f) {
        if (!m_mesh) return;
        const AnimationClip* clip = m_mesh->findAnimation(clipName);
        if (!clip) return;

        if (m_currentClip == clip && m_isPlaying) return;

        if (m_currentClip && blendTime > 0.0f && m_isPlaying) {
            m_prevClip = m_currentClip;
            m_prevTime = m_currentTime;
            m_blending = true;
            m_blendDuration = blendTime;
            m_blendTimer = 0.0f;
        } else {
            m_blending = false;
        }

        m_currentClip = clip;
        m_currentTime = 0.0f;
        m_isLooping = loop;
        m_isPlaying = true;
        calculateBoneTransforms();
    }

    void pause() { m_isPlaying = false; }
    void resume() { m_isPlaying = true; }
    void stop() { m_isPlaying = false; m_currentTime = 0.0f; calculateBoneTransforms(); }
    void reset() { m_currentTime = 0.0f; m_blending = false; calculateBoneTransforms(); }

    void setSpeed(float speed) { m_speed = speed; }
    float speed() const { return m_speed; }

    void setLoop(bool loop) { m_isLooping = loop; }
    bool isLooping() const { return m_isLooping; }

    bool isPlaying() const { return m_isPlaying; }
    float currentTime() const { return m_currentTime; }
    float duration() const { return m_currentClip ? m_currentClip->duration : 0.0f; }
    std::string currentClipName() const { return m_currentClip ? m_currentClip->name : ""; }

    void setTime(float time) {
        if (!m_currentClip) return;
        m_currentTime = time;
        if (m_isLooping && m_currentClip->duration > 0.0f) {
            m_currentTime = std::fmod(m_currentTime, m_currentClip->duration);
            if (m_currentTime < 0.0f) m_currentTime += m_currentClip->duration;
        } else {
            m_currentTime = glm::clamp(m_currentTime, 0.0f, m_currentClip->duration);
        }
        calculateBoneTransforms();
    }

    void update(float dt) {
        if (!m_mesh || !m_currentClip) return;

        if (m_isPlaying) {
            float dur = m_currentClip->duration;
            m_currentTime += dt * m_speed;

            if (dur > 0.0f) {
                if (m_currentTime >= dur) {
                    if (m_isLooping) {
                        m_currentTime = std::fmod(m_currentTime, dur);
                    } else {
                        m_currentTime = dur;
                        m_isPlaying = false;
                    }
                } else if (m_currentTime < 0.0f) {
                    if (m_isLooping) {
                        m_currentTime = dur - std::fmod(-m_currentTime, dur);
                    } else {
                        m_currentTime = 0.0f;
                        m_isPlaying = false;
                    }
                }
            }

            if (m_blending) {
                m_blendTimer += dt;
                if (m_prevClip && m_prevClip->duration > 0.0f) {
                    m_prevTime += dt * m_speed;
                    if (m_prevTime >= m_prevClip->duration) m_prevTime = std::fmod(m_prevTime, m_prevClip->duration);
                }
                if (m_blendTimer >= m_blendDuration) {
                    m_blending = false;
                    m_prevClip = nullptr;
                }
            }
        }

        calculateBoneTransforms();
    }

    const std::vector<glm::mat4>& finalBoneMatrices() const { return m_finalBoneMatrices; }
    const std::shared_ptr<SkinnedMesh>& mesh() const { return m_mesh; }

private:
    void calculateBoneTransforms() {
        const Skeleton& skeleton = m_mesh->skeleton();
        size_t count = std::min(skeleton.bones.size(), MAX_BONES);
        if (count == 0) return;

        std::vector<glm::mat4> globalTransforms(count, glm::mat4(1.0f));
        float blendFactor = m_blending ? glm::clamp(m_blendTimer / m_blendDuration, 0.0f, 1.0f) : 1.0f;

        for (size_t i = 0; i < count; ++i) {
            const BoneInfo& bone = skeleton.bones[i];
            glm::mat4 localTransform = bone.localBindTransform;

            const BoneTrack* curTrack = m_currentClip->findTrack(bone.id);
            if (!curTrack) curTrack = m_currentClip->findTrack(bone.name);

            if (curTrack) {
                glm::mat4 curMat = curTrack->sampleTransform(m_currentTime);
                if (m_blending && m_prevClip) {
                    const BoneTrack* prevTrack = m_prevClip->findTrack(bone.id);
                    if (!prevTrack) prevTrack = m_prevClip->findTrack(bone.name);
                    if (prevTrack) {
                        glm::vec3 pos0 = prevTrack->samplePosition(m_prevTime);
                        glm::quat rot0 = prevTrack->sampleRotation(m_prevTime);
                        glm::vec3 scl0 = prevTrack->sampleScale(m_prevTime);

                        glm::vec3 pos1 = curTrack->samplePosition(m_currentTime);
                        glm::quat rot1 = curTrack->sampleRotation(m_currentTime);
                        glm::vec3 scl1 = curTrack->sampleScale(m_currentTime);

                        glm::vec3 pos = glm::mix(pos0, pos1, blendFactor);
                        glm::quat rot = glm::slerp(rot0, rot1, blendFactor);
                        glm::vec3 scl = glm::mix(scl0, scl1, blendFactor);

                        localTransform = glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(rot) * glm::scale(glm::mat4(1.0f), scl);
                    } else {
                        localTransform = curMat;
                    }
                } else {
                    localTransform = curMat;
                }

                // Корневая кость (Root Bone / Pelvis):
                // Фиксируем X и Z на месте (in-place animation), сохраняя покачивание Y и вращение.
                // Это предотвращает накопление смещения и телепортацию при зацикливании анимации.
                if (bone.parentId < 0) {
                    glm::vec3 rPos = curTrack->samplePosition(m_currentTime);
                    glm::quat rRot = curTrack->sampleRotation(m_currentTime);
                    glm::vec3 rScl = curTrack->sampleScale(m_currentTime);
                    rPos.x = 0.0f;
                    rPos.z = 0.0f;
                    localTransform = glm::translate(glm::mat4(1.0f), rPos) * glm::toMat4(rRot) * glm::scale(glm::mat4(1.0f), rScl);
                }
            }

            if (bone.parentId >= 0 && static_cast<size_t>(bone.parentId) < count) {
                globalTransforms[i] = globalTransforms[static_cast<size_t>(bone.parentId)] * localTransform;
            } else {
                globalTransforms[i] = localTransform;
            }

            m_finalBoneMatrices[i] = globalTransforms[i] * bone.offsetMatrix;
        }
    }

    std::shared_ptr<SkinnedMesh> m_mesh;
    const AnimationClip* m_currentClip = nullptr;
    const AnimationClip* m_prevClip = nullptr;

    float m_currentTime = 0.0f;
    float m_prevTime = 0.0f;
    float m_speed = 1.0f;
    bool m_isLooping = true;
    bool m_isPlaying = false;

    bool m_blending = false;
    float m_blendDuration = 0.2f;
    float m_blendTimer = 0.0f;

    std::vector<glm::mat4> m_finalBoneMatrices;
};

} // namespace Animation
