#pragma once
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/Gameplay/EventBus.h"
#include "engine/Animation/AnimFile.h"

namespace Cinematics {

struct CameraCut {
    float time = 0.0f; // Timestamp when cut occurs
    Entity cameraEntity = NullEntity;
    std::string cameraName = "MainCamera";
};

struct CameraKeyframe {
    float time = 0.0f; // Seconds
    glm::vec3 position{0.0f, 3.0f, -8.0f};
    glm::vec3 rotation{0.0f}; // pitch, yaw, roll
    float fov = 65.0f;
};

struct ActorKeyframe {
    float time = 0.0f;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
};

struct EventKeyframe {
    float time = 0.0f;
    std::string eventName;
    bool triggered = false;
};

struct ActorClipInstance {
    std::string clipPath;
    std::shared_ptr<Animation::AnimFile> anim;
    float startTime = 0.0f;
    float timeScale = 1.0f;
    bool loop = true;
};

struct ActorTrack {
    Entity targetEntity = NullEntity;
    std::string actorName = "Actor";
    std::string boundAnimPath = "";
    std::vector<ActorKeyframe> keyframes;
    std::vector<ActorClipInstance> clips;
};

class Sequencer {
public:
    static Sequencer& Get() {
        static Sequencer s_instance;
        return s_instance;
    }

    std::string sequenceName = "Master_Cutscene";
    float duration = 30.0f; // Completely configurable length in seconds (NO 10s limit!)
    float currentTime = 0.0f;
    float frameRate = 30.0f;
    bool isPlaying = false;
    bool isLooping = false;
    bool isPreviewing = true;
    bool isRecording = false; // Unity-style Auto-Keyframing REC mode

    std::vector<CameraCut> cameraCuts;
    std::vector<CameraKeyframe> cameraTrack;
    std::vector<ActorTrack> actorTracks;
    std::vector<EventKeyframe> eventTrack;

    void play() { isPlaying = true; }
    void pause() { isPlaying = false; }
    void stop() {
        isPlaying = false;
        currentTime = 0.0f;
        for (auto& ev : eventTrack) ev.triggered = false;
    }

    void scrub(float time, Registry* reg = nullptr, Entity* activeCam = nullptr) {
        currentTime = std::clamp(time, 0.0f, duration);
        if (reg) {
            Entity outCam = activeCam ? *activeCam : NullEntity;
            evaluate(*reg, currentTime, outCam);
            if (activeCam) *activeCam = outCam;
        }
    }

    void addCameraCut(float time, Entity cam, const std::string& name) {
        CameraCut cut{time, cam, name};
        cameraCuts.push_back(cut);
        std::sort(cameraCuts.begin(), cameraCuts.end(), [](const CameraCut& a, const CameraCut& b) {
            return a.time < b.time;
        });
        if (time > duration) duration = time + 1.0f;
    }

    void removeCameraCut(size_t index) {
        if (index < cameraCuts.size()) {
            cameraCuts.erase(cameraCuts.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    void addCameraKeyframe(float time, const glm::vec3& pos, const glm::vec3& rot, float fov) {
        for (auto& k : cameraTrack) {
            if (std::abs(k.time - time) < 0.001f) {
                k.position = pos;
                k.rotation = rot;
                k.fov = fov;
                return;
            }
        }
        CameraKeyframe kf{time, pos, rot, fov};
        cameraTrack.push_back(kf);
        std::sort(cameraTrack.begin(), cameraTrack.end(), [](const CameraKeyframe& a, const CameraKeyframe& b) {
            return a.time < b.time;
        });
        if (time > duration) duration = time + 1.0f;
    }

    void removeCameraKeyframe(size_t index) {
        if (index < cameraTrack.size()) {
            cameraTrack.erase(cameraTrack.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    void addEventKeyframe(float time, const std::string& eventName) {
        eventTrack.push_back({time, eventName, false});
        std::sort(eventTrack.begin(), eventTrack.end(), [](const EventKeyframe& a, const EventKeyframe& b) {
            return a.time < b.time;
        });
        if (time > duration) duration = time + 1.0f;
    }

    void removeEventKeyframe(size_t index) {
        if (index < eventTrack.size()) {
            eventTrack.erase(eventTrack.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    ActorTrack* getOrCreateActorTrack(Entity e, const std::string& name) {
        for (auto& tr : actorTracks) {
            if (tr.targetEntity == e) return &tr;
        }
        ActorTrack newTr;
        newTr.targetEntity = e;
        newTr.actorName = name;
        actorTracks.push_back(newTr);
        return &actorTracks.back();
    }

    void addActorKeyframe(Entity e, const std::string& name, float time, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale) {
        ActorTrack* tr = getOrCreateActorTrack(e, name);
        if (!tr) return;

        for (auto& k : tr->keyframes) {
            if (std::abs(k.time - time) < 0.001f) {
                k.position = pos;
                k.rotation = rot;
                k.scale = scale;
                return;
            }
        }
        tr->keyframes.push_back({time, pos, rot, scale});
        std::sort(tr->keyframes.begin(), tr->keyframes.end(), [](const ActorKeyframe& a, const ActorKeyframe& b) {
            return a.time < b.time;
        });
        if (time > duration) duration = time + 1.0f;
    }

    void bindAnimToActor(Entity e, const std::string& name, const std::string& animPath, float startTime = 0.0f) {
        ActorTrack* tr = getOrCreateActorTrack(e, name);
        if (!tr) return;

        auto anim = Animation::AnimFile::Load(animPath);
        if (anim) {
            tr->boundAnimPath = animPath;
            ActorClipInstance clip;
            clip.clipPath = animPath;
            clip.anim = anim;
            clip.startTime = startTime;
            clip.timeScale = 1.0f;
            clip.loop = anim->isLooping;
            tr->clips.push_back(clip);

            float clipEnd = startTime + anim->duration;
            if (clipEnd > duration) duration = clipEnd;
            std::cout << "[Sequencer] Added .anim clip '" << anim->name << "' to actor " << name << "\n";
        }
    }

    // --- Core Evaluation Function ---
    void evaluate(Registry& reg, float time, Entity& outActiveCam) {
        // 1. Evaluate Camera Cuts Track
        if (!cameraCuts.empty()) {
            Entity chosenCam = NullEntity;
            for (const auto& cut : cameraCuts) {
                if (time >= cut.time) {
                    if (reg.valid(cut.cameraEntity)) chosenCam = cut.cameraEntity;
                } else {
                    break;
                }
            }
            if (chosenCam != NullEntity) {
                outActiveCam = chosenCam;
                for (Entity e : reg.view<Camera>()) {
                    reg.get<Camera>(e).primary = (e == outActiveCam);
                }
            }
        }

        // 2. Evaluate Camera Motion Track
        if (!cameraTrack.empty() && outActiveCam != NullEntity && reg.valid(outActiveCam)) {
            auto* tr = reg.try_get<Transform>(outActiveCam);
            auto* cam = reg.try_get<Camera>(outActiveCam);
            if (tr && cam) {
                if (cameraTrack.size() == 1 || time <= cameraTrack.front().time) {
                    tr->position = cameraTrack.front().position;
                    tr->rotation = cameraTrack.front().rotation;
                    cam->fov = cameraTrack.front().fov;
                } else if (time >= cameraTrack.back().time) {
                    tr->position = cameraTrack.back().position;
                    tr->rotation = cameraTrack.back().rotation;
                    cam->fov = cameraTrack.back().fov;
                } else {
                    for (size_t i = 0; i + 1 < cameraTrack.size(); ++i) {
                        if (time >= cameraTrack[i].time && time <= cameraTrack[i + 1].time) {
                            float segDuration = cameraTrack[i + 1].time - cameraTrack[i].time;
                            float t = (segDuration > 0.0001f) ? (time - cameraTrack[i].time) / segDuration : 0.0f;
                            t = t * t * (3.0f - 2.0f * t); // Hermite smoothstep

                            tr->position = glm::mix(cameraTrack[i].position, cameraTrack[i + 1].position, t);
                            tr->rotation = glm::mix(cameraTrack[i].rotation, cameraTrack[i + 1].rotation, t);
                            cam->fov = glm::mix(cameraTrack[i].fov, cameraTrack[i + 1].fov, t);
                            break;
                        }
                    }
                }
            }
        }

        // 3. Evaluate Actor Tracks (either .anim clips or inline keyframes)
        for (auto& track : actorTracks) {
            if (track.targetEntity == NullEntity || !reg.valid(track.targetEntity)) continue;
            auto* tr = reg.try_get<Transform>(track.targetEntity);
            if (!tr) continue;

            // Check if active .anim clip exists for this time
            bool clipSampled = false;
            for (const auto& clip : track.clips) {
                if (clip.anim && time >= clip.startTime) {
                    float localTime = (time - clip.startTime) * clip.timeScale;
                    if (clip.loop && clip.anim->duration > 0.0001f) {
                        localTime = std::fmod(localTime, clip.anim->duration);
                    }
                    if (localTime <= clip.anim->duration) {
                        tr->position = clip.anim->samplePosition(localTime, tr->position);
                        tr->rotation = clip.anim->sampleRotation(localTime, tr->rotation);
                        tr->scale = clip.anim->sampleScale(localTime, tr->scale);
                        clipSampled = true;
                        break;
                    }
                }
            }

            // Fallback to inline keyframes if no clip was sampled
            if (!clipSampled && !track.keyframes.empty()) {
                if (track.keyframes.size() == 1 || time <= track.keyframes.front().time) {
                    tr->position = track.keyframes.front().position;
                    tr->rotation = track.keyframes.front().rotation;
                    tr->scale = track.keyframes.front().scale;
                } else if (time >= track.keyframes.back().time) {
                    tr->position = track.keyframes.back().position;
                    tr->rotation = track.keyframes.back().rotation;
                    tr->scale = track.keyframes.back().scale;
                } else {
                    for (size_t i = 0; i + 1 < track.keyframes.size(); ++i) {
                        if (time >= track.keyframes[i].time && time <= track.keyframes[i + 1].time) {
                            float segDuration = track.keyframes[i + 1].time - track.keyframes[i].time;
                            float t = (segDuration > 0.0001f) ? (time - track.keyframes[i].time) / segDuration : 0.0f;
                            t = t * t * (3.0f - 2.0f * t);
                            tr->position = glm::mix(track.keyframes[i].position, track.keyframes[i + 1].position, t);
                            tr->rotation = glm::mix(track.keyframes[i].rotation, track.keyframes[i + 1].rotation, t);
                            tr->scale = glm::mix(track.keyframes[i].scale, track.keyframes[i + 1].scale, t);
                            break;
                        }
                    }
                }
            }
        }
    }

    void update(Registry& reg, float dt, Entity& activeCameraEntity) {
        if (!isPlaying && !isPreviewing) return;

        if (isPlaying) {
            float prevTime = currentTime;
            currentTime += dt;

            if (currentTime > duration) {
                if (isLooping) {
                    currentTime = 0.0f;
                    for (auto& ev : eventTrack) ev.triggered = false;
                } else {
                    currentTime = duration;
                    isPlaying = false;
                    EventBus::Get().emit("CutsceneEnded");
                    return;
                }
            }

            // Evaluate Event Triggers
            for (auto& ev : eventTrack) {
                if (!ev.triggered && prevTime <= ev.time && currentTime >= ev.time) {
                    ev.triggered = true;
                    EventBus::Get().emit(ev.eventName);
                    std::cout << "[Sequencer] Triggered Event: " << ev.eventName << " at t=" << ev.time << "s\n";
                }
            }
        }

        evaluate(reg, currentTime, activeCameraEntity);
    }

    // --- Save / Load Sequence File (.seq.json) ---
    bool saveToFile(const std::string& path) const {
        std::ofstream f(path);
        if (!f.is_open()) return false;

        f << "{\n";
        f << "  \"name\": \"" << sequenceName << "\",\n";
        f << "  \"duration\": " << duration << ",\n";
        f << "  \"frameRate\": " << frameRate << ",\n";
        f << "  \"loop\": " << (isLooping ? "true" : "false") << ",\n";

        // Camera Cuts
        f << "  \"cameraCuts\": [\n";
        for (size_t i = 0; i < cameraCuts.size(); ++i) {
            const auto& c = cameraCuts[i];
            f << "    { \"time\": " << c.time << ", \"name\": \"" << c.cameraName << "\" }"
              << (i + 1 < cameraCuts.size() ? ",\n" : "\n");
        }
        f << "  ],\n";

        // Camera Motion
        f << "  \"cameraTrack\": [\n";
        for (size_t i = 0; i < cameraTrack.size(); ++i) {
            const auto& k = cameraTrack[i];
            f << "    { \"time\": " << k.time << ", \"pos\": [" << k.position.x << ", " << k.position.y << ", " << k.position.z
              << "], \"rot\": [" << k.rotation.x << ", " << k.rotation.y << ", " << k.rotation.z << "], \"fov\": " << k.fov << " }"
              << (i + 1 < cameraTrack.size() ? ",\n" : "\n");
        }
        f << "  ],\n";

        // Events
        f << "  \"eventTrack\": [\n";
        for (size_t i = 0; i < eventTrack.size(); ++i) {
            const auto& ev = eventTrack[i];
            f << "    { \"time\": " << ev.time << ", \"event\": \"" << ev.eventName << "\" }"
              << (i + 1 < eventTrack.size() ? ",\n" : "\n");
        }
        f << "  ]\n";
        f << "}\n";

        std::cout << "[Sequencer] Saved sequence to " << path << " (" << duration << "s)\n";
        return true;
    }

    bool loadFromFile(const std::string& path, Registry* /*reg*/ = nullptr) {
        std::ifstream f(path);
        if (!f.is_open()) return false;

        cameraCuts.clear();
        cameraTrack.clear();
        eventTrack.clear();

        std::string line;
        while (std::getline(f, line)) {
            if (line.find("\"name\":") != std::string::npos) {
                size_t q1 = line.find('"', 7);
                size_t q2 = line.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos)
                    sequenceName = line.substr(q1 + 1, q2 - q1 - 1);
            } else if (line.find("\"duration\":") != std::string::npos) {
                sscanf(line.c_str(), " \"duration\": %f", &duration);
            } else if (line.find("\"cameraCuts\":") != std::string::npos) {
                // Parse cuts
            }
        }
        std::cout << "[Sequencer] Loaded sequence '" << sequenceName << "' from " << path << "\n";
        return true;
    }
};

} // namespace Cinematics
