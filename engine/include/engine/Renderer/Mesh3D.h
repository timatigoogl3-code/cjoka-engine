#pragma once
#include <vector>
#include <cstdint>
#include <glad/gl.h>
#include <glm/glm.hpp>

// Вершина — pos/color/normal/uv, layout 0/1/2/3
struct Vertex {
    glm::vec3 position{0};
    glm::vec3 color{1};
    glm::vec3 normal{0, 0, 1};
    glm::vec2 texCoord{0};
    Vertex() = default;
    Vertex(glm::vec3 p, glm::vec3 c = {1,1,1}, glm::vec3 n = {0,0,1}, glm::vec2 uv = {0,0})
        : position(p), color(c), normal(n), texCoord(uv) {}
};

struct InstanceData {
    glm::mat4 model{1.0f};
    glm::mat4 prevModel{1.0f};    // для motion vectors
    glm::vec4 albedo{1.0f, 1.0f, 1.0f, 0.5f};   // rgb + roughness (PBR)
    glm::vec4 emissive{0.0f, 0.0f, 0.0f, 0.0f}; // rgb + metallic (PBR)
};

class Mesh3D {
public:
    Mesh3D() = default;
    Mesh3D(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
    ~Mesh3D();

    Mesh3D(const Mesh3D&) = delete;
    Mesh3D& operator=(const Mesh3D&) = delete;
    Mesh3D(Mesh3D&& other) noexcept;
    Mesh3D& operator=(Mesh3D&& other) noexcept;

    glm::vec3 minExtents() const { return m_minExtents; }
    glm::vec3 maxExtents() const { return m_maxExtents; }

    void draw() const;
    // инстансинг — один draw на N объектов
    void drawInstanced(const InstanceData* data, size_t count) const;
    void drawInstanced(const std::vector<InstanceData>& v) const { drawInstanced(v.data(), v.size()); }

    uint32_t indexCount() const { return m_indexCount; }
    bool empty() const { return m_indexCount == 0; }
    GLuint vao() const { return m_vao; }
    // доступ к CPU-копии (для ClusteredMesh/Nanite-lite)
    const std::vector<Vertex>& vertices() const { return m_vertices; }
    const std::vector<uint32_t>& indices() const { return m_indices; }

    static Mesh3D Triangle(glm::vec3 c0={1,0.2f,0.2f}, glm::vec3 c1={0.2f,1,0.2f}, glm::vec3 c2={0.2f,0.2f,1});
    static Mesh3D Quad(float size = 1.0f);
    static Mesh3D Cube(float size = 1.0f);
    static Mesh3D Plane(float width = 10.0f, float depth = 10.0f, int gridX = 10, int gridZ = 10, float uvTileX = 1.0f, float uvTileZ = 1.0f);

private:
    void setup();
    void setupInstancing() const;
    std::vector<Vertex>   m_vertices;
    std::vector<uint32_t> m_indices;
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    mutable GLuint m_instanceVBO = 0;
    uint32_t m_indexCount = 0;
    glm::vec3 m_minExtents{0.0f};
    glm::vec3 m_maxExtents{0.0f};
};
