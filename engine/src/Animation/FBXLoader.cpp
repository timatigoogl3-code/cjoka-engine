#include "engine/Animation/FBXLoader.h"
#include "ufbx/ufbx.h"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <glm/gtc/type_ptr.hpp>

namespace Animation {

namespace {

inline glm::mat4 toGlmMat4(const ufbx_matrix& m) {
    return glm::mat4(
        m.m00, m.m10, m.m20, 0.0f,
        m.m01, m.m11, m.m21, 0.0f,
        m.m02, m.m12, m.m22, 0.0f,
        m.m03, m.m13, m.m23, 1.0f
    );
}

inline glm::vec3 toGlmVec3(const ufbx_vec3& v) {
    return glm::vec3(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
}

inline glm::quat toGlmQuat(const ufbx_quat& q) {
    return glm::quat(static_cast<float>(q.w), static_cast<float>(q.x), static_cast<float>(q.y), static_cast<float>(q.z));
}

} // anonymous namespace

std::shared_ptr<SkinnedMesh> FBXLoader::LoadFBX(const std::string& path) {
    ufbx_load_opts opts = {};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;
    opts.generate_missing_normals = true;

    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);
    if (!scene) {
        std::cerr << "[FBXLoader] Failed to load " << path << ": " << error.description.data << std::endl;
        return nullptr;
    }

    // 1. Поиск первого подходящего скин-меша
    const ufbx_mesh* targetMesh = nullptr;
    const ufbx_skin_deformer* skin = nullptr;

    for (size_t i = 0; i < scene->meshes.count; ++i) {
        const ufbx_mesh* m = scene->meshes.data[i];
        if (m->skin_deformers.count > 0) {
            targetMesh = m;
            skin = m->skin_deformers.data[0];
            break;
        }
    }

    if (!targetMesh && scene->meshes.count > 0) {
        targetMesh = scene->meshes.data[0];
    }

    if (!targetMesh) {
        std::cerr << "[FBXLoader] No meshes found in " << path << std::endl;
        ufbx_free_scene(scene);
        return nullptr;
    }

    // 2. Построение скелета
    Skeleton skeleton;
    std::unordered_map<const ufbx_node*, int> nodeToBoneId;

    if (skin) {
        for (size_t i = 0; i < skin->clusters.count; ++i) {
            const ufbx_skin_cluster* cluster = skin->clusters.data[i];
            const ufbx_node* boneNode = cluster->bone_node;
            if (!boneNode) continue;

            int boneId = static_cast<int>(skeleton.bones.size());
            nodeToBoneId[boneNode] = boneId;

            BoneInfo b;
            b.id = boneId;
            b.name = boneNode->name.data;
            b.offsetMatrix = toGlmMat4(cluster->geometry_to_bone);
            b.localBindTransform = toGlmMat4(boneNode->node_to_parent);

            skeleton.bones.push_back(b);
            skeleton.boneMapping[b.name] = boneId;
        }

        // Устанавливаем parentId для костей
        for (size_t i = 0; i < skin->clusters.count; ++i) {
            const ufbx_skin_cluster* cluster = skin->clusters.data[i];
            const ufbx_node* boneNode = cluster->bone_node;
            if (!boneNode) continue;

            int boneId = nodeToBoneId[boneNode];
            const ufbx_node* parentNode = boneNode->parent;
            while (parentNode) {
                auto it = nodeToBoneId.find(parentNode);
                if (it != nodeToBoneId.end()) {
                    skeleton.bones[static_cast<size_t>(boneId)].parentId = it->second;
                    break;
                }
                parentNode = parentNode->parent;
            }
        }
    }

    // 3. Сборка вершин со скиннингом
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32_t> indices;

    size_t numTriangles = targetMesh->num_triangles;
    vertices.reserve(numTriangles * 3);
    indices.reserve(numTriangles * 3);

    for (size_t faceIdx = 0; faceIdx < targetMesh->num_faces; ++faceIdx) {
        ufbx_face face = targetMesh->faces.data[faceIdx];
        size_t numTriInFace = face.num_indices >= 3 ? face.num_indices - 2 : 0;

        for (size_t tri = 0; tri < numTriInFace; ++tri) {
            uint32_t cornerIndices[3] = {
                face.index_begin,
                static_cast<uint32_t>(face.index_begin + tri + 1),
                static_cast<uint32_t>(face.index_begin + tri + 2)
            };

            for (int c = 0; c < 3; ++c) {
                uint32_t corner = cornerIndices[c];
                uint32_t vIndex = targetMesh->vertex_indices.data[corner];

                SkinnedVertex v;
                ufbx_vec3 p = ufbx_get_vertex_vec3(&targetMesh->vertex_position, corner);
                v.position = toGlmVec3(p);

                if (targetMesh->vertex_normal.exists) {
                    ufbx_vec3 n = ufbx_get_vertex_vec3(&targetMesh->vertex_normal, corner);
                    v.normal = toGlmVec3(n);
                }

                if (targetMesh->vertex_uv.exists) {
                    ufbx_vec2 uv = ufbx_get_vertex_vec2(&targetMesh->vertex_uv, corner);
                    v.texCoord = glm::vec2(static_cast<float>(uv.x), static_cast<float>(uv.y));
                }

                // Скиннинг веса
                if (skin && vIndex < skin->vertices.count) {
                    ufbx_skin_vertex skinVert = skin->vertices.data[vIndex];
                    uint32_t numWeights = std::min(skinVert.num_weights, 4u);
                    float totalWeight = 0.0f;

                    for (uint32_t w = 0; w < numWeights; ++w) {
                        ufbx_skin_weight sw = skin->weights.data[skinVert.weight_begin + w];
                        int boneId = static_cast<int>(sw.cluster_index);
                        if (boneId >= 0 && boneId < static_cast<int>(skeleton.bones.size())) {
                            int iw = static_cast<int>(w);
                            v.boneIds[iw] = boneId;
                            v.weights[iw] = static_cast<float>(sw.weight);
                            totalWeight += v.weights[iw];
                        }
                    }

                    if (totalWeight > 0.0001f) {
                        v.weights /= totalWeight;
                    } else {
                        v.boneIds = glm::ivec4(0);
                        v.weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                    }
                }

                indices.push_back(static_cast<uint32_t>(vertices.size()));
                vertices.push_back(v);
            }
        }
    }

    // 4. Сборка анимационных клипов
    std::vector<AnimationClip> animations;

    for (size_t aIdx = 0; aIdx < scene->anim_stacks.count; ++aIdx) {
        const ufbx_anim_stack* stack = scene->anim_stacks.data[aIdx];
        if (!stack || !stack->anim) continue;

        AnimationClip clip;
        clip.name = stack->name.data ? stack->name.data : ("Anim_" + std::to_string(aIdx));
        float duration = static_cast<float>(stack->time_end - stack->time_begin);
        if (duration <= 0.0f) duration = 1.0f;
        clip.duration = duration;
        clip.frameRate = static_cast<float>(scene->settings.frames_per_second);
        if (clip.frameRate <= 0.0f) clip.frameRate = 30.0f;

        float sampleStep = 1.0f / clip.frameRate;
        size_t numSamples = static_cast<size_t>(std::ceil(clip.duration / sampleStep)) + 1;

        for (size_t bIdx = 0; bIdx < skeleton.bones.size(); ++bIdx) {
            const BoneInfo& bone = skeleton.bones[bIdx];
            const ufbx_node* boneNode = nullptr;

            for (const auto& pair : nodeToBoneId) {
                if (pair.second == bone.id) {
                    boneNode = pair.first;
                    break;
                }
            }
            if (!boneNode) continue;

            BoneTrack track;
            track.boneId = bone.id;
            track.boneName = bone.name;
            track.positions.reserve(numSamples);
            track.rotations.reserve(numSamples);
            track.scales.reserve(numSamples);

            for (size_t s = 0; s < numSamples; ++s) {
                float t = static_cast<float>(s) * sampleStep;
                if (t > clip.duration) t = clip.duration;
                // Сдвигаем точку вычисления от края (time_begin + 0.001) чтобы избежать дефолтного bind-кадра 0
                double evalTime = stack->time_begin + std::max(static_cast<double>(t), 0.001);

                ufbx_transform localTrans = ufbx_evaluate_transform(stack->anim, boneNode, evalTime);

                track.positions.push_back({t, toGlmVec3(localTrans.translation)});
                track.rotations.push_back({t, toGlmQuat(localTrans.rotation)});
                track.scales.push_back({t, toGlmVec3(localTrans.scale)});
            }

            // Бесшовное зацикливание: убираем любой глитч T-Pose на стыке цикла
            if (track.positions.size() >= 3) {
                track.positions.front().value = track.positions[1].value;
                track.rotations.front().value = track.rotations[1].value;
                track.scales.front().value = track.scales[1].value;

                track.positions.back().value = track.positions.front().value;
                track.rotations.back().value = track.rotations.front().value;
                track.scales.back().value = track.scales.front().value;
            }

            clip.tracks.push_back(std::move(track));
        }

        std::cout << "[FBXLoader] Animation '" << clip.name << "' loaded, duration: "
                  << clip.duration << "s, tracks: " << clip.tracks.size() << std::endl;
        animations.push_back(std::move(clip));
    }

    std::cout << "[FBXLoader] " << path << " loaded: verts=" << vertices.size()
              << " tris=" << indices.size()/3 << " bones=" << skeleton.bones.size()
              << " anims=" << animations.size() << std::endl;

    ufbx_free_scene(scene);

    return std::make_shared<SkinnedMesh>(std::move(vertices), std::move(indices), std::move(skeleton), std::move(animations));
}

} // namespace Animation
