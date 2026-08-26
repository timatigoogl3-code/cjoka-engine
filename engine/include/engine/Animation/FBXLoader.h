#pragma once
#include "engine/Animation/SkinnedMesh.h"
#include <string>
#include <memory>

namespace Animation {

class FBXLoader {
public:
    static std::shared_ptr<SkinnedMesh> LoadFBX(const std::string& path);
};

} // namespace Animation
