#pragma once
#include <glm/glm.hpp>
#include <algorithm>

namespace cjoka {

// ------------------------------------------------------------------
// WorldEnvironment — чистая шина физических параметров окружающей среды.
// Не содержит игрового контента, таймеров, жестко зашитых VFX или мутаций ECS.
// ------------------------------------------------------------------
struct WorldEnvironment {
    float precipitation = 0.0f;       // 0.0 (сухо) .. 1.0 (ливень / снегопад)
    float groundWetness = 0.0f;       // 0.0 (сухо) .. 1.0 (мокрые дороги / лужи)
    glm::vec3 wind{ -0.5f, 0.0f, 0.2f }; // вектор скорости ветра (м/с)
    float temperature = 20.0f;        // температура в градусах Цельсия
    float fogMultiplier = 1.0f;       // множитель плотности тумана

    static WorldEnvironment& Get() {
        static WorldEnvironment s_instance;
        return s_instance;
    }
};

} // namespace cjoka
