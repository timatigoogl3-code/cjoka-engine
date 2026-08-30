#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"

namespace Core {

class Prefab {
public:
    // Saves an entity and its primary components to a standalone .prefab.json file
    static bool Save(Registry& reg, Entity e, const std::string& filepath) {
        if (!reg.valid(e)) return false;

        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        file << "{\n";
        std::string name = reg.has<Name>(e) ? reg.get<Name>(e).value : "PrefabEntity";
        file << "  \"name\": \"" << name << "\",\n";

        if (reg.has<Transform>(e)) {
            const auto& tr = reg.get<Transform>(e);
            file << "  \"pos\": [" << tr.position.x << ", " << tr.position.y << ", " << tr.position.z << "],\n";
            file << "  \"rot\": [" << tr.rotation.x << ", " << tr.rotation.y << ", " << tr.rotation.z << "],\n";
            file << "  \"scale\": [" << tr.scale.x << ", " << tr.scale.y << ", " << tr.scale.z << "]";
        }

        if (reg.has<MeshRenderer>(e)) {
            const auto& mr = reg.get<MeshRenderer>(e);
            file << ",\n  \"mesh\": {\n";
            file << "    \"assetPath\": \"" << mr.assetPath << "\",\n";
            file << "    \"diffusePath\": \"" << mr.material.diffuseMapPath << "\",\n";
            file << "    \"normalPath\": \"" << mr.material.normalMapPath << "\",\n";
            file << "    \"albedo\": [" << mr.material.albedo.r << ", " << mr.material.albedo.g << ", " << mr.material.albedo.b << "],\n";
            file << "    \"metallic\": " << mr.material.metallic << ",\n";
            file << "    \"roughness\": " << mr.material.roughness << ",\n";
            file << "    \"castShadow\": " << (mr.castShadow ? "true" : "false") << "\n";
            file << "  }";
        }

        if (reg.has<PointLight>(e)) {
            const auto& pl = reg.get<PointLight>(e);
            file << ",\n  \"light\": {\n";
            file << "    \"color\": [" << pl.color.r << ", " << pl.color.g << ", " << pl.color.b << "],\n";
            file << "    \"intensity\": " << pl.intensity << ",\n";
            file << "    \"range\": " << pl.range << "\n";
            file << "  }";
        }

        file << "\n}\n";
        std::cout << "[Prefab] Saved prefab to " << filepath << "\n";
        return true;
    }

    // Instantiates a prefab into the scene at a given world position
    static Entity Instantiate(Registry& reg, const std::string& filepath, const glm::vec3& spawnPos, float yaw = 0.0f) {
        if (!std::filesystem::exists(filepath)) {
            std::cerr << "[Prefab] File not found: " << filepath << "\n";
            return NullEntity;
        }

        std::ifstream file(filepath);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        Entity e = reg.create();
        reg.emplace<Name>(e, "Prefab_Instance");

        Transform tr;
        tr.position = spawnPos;
        tr.rotation = glm::vec3(0.0f, yaw, 0.0f);
        tr.scale = glm::vec3(1.0f);

        // Simple tag parser
        if (content.find("\"mesh\":") != std::string::npos) {
            Material mat;
            std::string assetPath = "";
            size_t aPos = content.find("\"assetPath\":");
            if (aPos != std::string::npos) {
                size_t q1 = content.find("\"", aPos + 12);
                size_t q2 = content.find("\"", q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    assetPath = content.substr(q1 + 1, q2 - q1 - 1);
                }
            }

            size_t dPos = content.find("\"diffusePath\":");
            if (dPos != std::string::npos) {
                size_t q1 = content.find("\"", dPos + 14);
                size_t q2 = content.find("\"", q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    std::string diffPath = content.substr(q1 + 1, q2 - q1 - 1);
                    if (!diffPath.empty() && std::filesystem::exists(diffPath)) {
                        mat.diffuseMapPath = diffPath;
                        mat.diffuseMap = Assets::Texture(diffPath, true);
                        mat.useDiffuseMap = (mat.diffuseMap && mat.diffuseMap->valid());
                    }
                }
            }

            std::shared_ptr<Mesh3D> mesh;
            if (assetPath.find(".obj") != std::string::npos && std::filesystem::exists(assetPath)) {
                mesh = Assets::Mesh(assetPath);
            } else if (assetPath.find("sphere") != std::string::npos) {
                mesh = Assets::Sphere(0.5f);
            } else if (assetPath.find("plane") != std::string::npos) {
                mesh = Assets::Plane(10.0f, 10.0f);
            } else {
                mesh = Assets::Cube(1.0f);
            }

            MeshRenderer mr(mesh, mat);
            mr.assetPath = assetPath;
            reg.emplace<MeshRenderer>(e, mr);
        }

        reg.emplace<Transform>(e, tr);
        std::cout << "[Prefab] Instantiated prefab " << filepath << " at (" << spawnPos.x << ", " << spawnPos.y << ", " << spawnPos.z << ")\n";
        return e;
    }
};

} // namespace Core
