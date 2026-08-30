#pragma once
#include "engine/Renderer/Mesh3D.h"
#include <string>
#include <memory>

class MeshLoader {
public:
    // Загрузка .obj (tinyobjloader) -> Mesh3D с pos/normal/uv/color
    // flipY если нужно инвертировать V координату
    static std::shared_ptr<Mesh3D> LoadOBJ(const std::string& path, bool flipY = false);
    static std::shared_ptr<Mesh3D> LoadOBJ(const char* path, bool flipY = false);

    // Генерация примитивов уже в Mesh3D, но дублируем для удобства
    static std::shared_ptr<Mesh3D> Cube(float size = 1.0f);
    static std::shared_ptr<Mesh3D> Quad(float size = 1.0f);
    static std::shared_ptr<Mesh3D> Plane(float width = 10.0f, float depth = 10.0f, int gridX = 10, int gridZ = 10, float uvTileX = 1.0f, float uvTileZ = 1.0f);
    static std::shared_ptr<Mesh3D> Sphere(float radius = 0.5f, int sectors = 32, int stacks = 16);
};
