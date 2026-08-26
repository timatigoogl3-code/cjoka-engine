#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Animation {

struct BoneInfo {
    int id = -1;
    std::string name;
    int parentId = -1;
    glm::mat4 offsetMatrix{1.0f};      // Inverse Bind Pose matrix
    glm::mat4 localBindTransform{1.0f};
};

struct Skeleton {
    std::vector<BoneInfo> bones;
    std::unordered_map<std::string, int> boneMapping;

    int findBone(const std::string& name) const {
        auto it = boneMapping.find(name);
        return (it != boneMapping.end()) ? it->second : -1;
    }

    size_t boneCount() const { return bones.size(); }
    bool empty() const { return bones.empty(); }
};

} // namespace Animation
