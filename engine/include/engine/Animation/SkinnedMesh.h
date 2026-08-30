#pragma once
#include "engine/Animation/Skeleton.h"
#include "engine/Animation/AnimationClip.h"
#include <vector>
#include <memory>
#include <glad/gl.h>
#include <glm/glm.hpp>

namespace Animation {

struct SkinnedVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 texCoord{0.0f};
    glm::ivec4 boneIds{0, 0, 0, 0};
    glm::vec4 weights{0.0f, 0.0f, 0.0f, 0.0f};
};

class SkinnedMesh {
public:
    SkinnedMesh() = default;
    SkinnedMesh(std::vector<SkinnedVertex> vertices, std::vector<uint32_t> indices, Skeleton skeleton, std::vector<AnimationClip> animations)
        : m_vertices(std::move(vertices)), m_indices(std::move(indices)), m_skeleton(std::move(skeleton)), m_animations(std::move(animations)) {
        setupMesh();
    }

    ~SkinnedMesh() {
        cleanup();
    }

    SkinnedMesh(const SkinnedMesh&) = delete;
    SkinnedMesh& operator=(const SkinnedMesh&) = delete;

    SkinnedMesh(SkinnedMesh&& o) noexcept
        : m_vertices(std::move(o.m_vertices)), m_indices(std::move(o.m_indices)),
          m_skeleton(std::move(o.m_skeleton)), m_animations(std::move(o.m_animations)),
          m_vao(o.m_vao), m_vbo(o.m_vbo), m_ebo(o.m_ebo) {
        o.m_vao = 0; o.m_vbo = 0; o.m_ebo = 0;
    }

    SkinnedMesh& operator=(SkinnedMesh&& o) noexcept {
        if (this != &o) {
            cleanup();
            m_vertices = std::move(o.m_vertices);
            m_indices = std::move(o.m_indices);
            m_skeleton = std::move(o.m_skeleton);
            m_animations = std::move(o.m_animations);
            m_vao = o.m_vao; m_vbo = o.m_vbo; m_ebo = o.m_ebo;
            o.m_vao = 0; o.m_vbo = 0; o.m_ebo = 0;
        }
        return *this;
    }

    void draw() const {
        if (m_vao && !m_indices.empty()) {
            glBindVertexArray(m_vao);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }
    }

    const Skeleton& skeleton() const { return m_skeleton; }
    Skeleton& skeleton() { return m_skeleton; }

    const std::vector<AnimationClip>& animations() const { return m_animations; }
    std::vector<AnimationClip>& animations() { return m_animations; }

    const AnimationClip* findAnimation(const std::string& name) const {
        for (const auto& anim : m_animations) {
            if (anim.name == name) return &anim;
        }
        return m_animations.empty() ? nullptr : &m_animations[0];
    }

    size_t vertexCount() const { return m_vertices.size(); }
    size_t indexCount() const { return m_indices.size(); }
    const std::vector<uint32_t>& indices() const { return m_indices; }
    bool empty() const { return m_indices.empty(); }

private:
    void setupMesh() {
        if (m_vertices.empty() || m_indices.empty()) return;

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(SkinnedVertex)), m_vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices.size() * sizeof(uint32_t)), m_indices.data(), GL_STATIC_DRAW);

        // Position: location 0
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void*)offsetof(SkinnedVertex, position));

        // Normal: location 1
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void*)offsetof(SkinnedVertex, normal));

        // UV: location 2
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void*)offsetof(SkinnedVertex, texCoord));

        // Bone IDs: location 3 (IPointer for integer types)
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 4, GL_INT, sizeof(SkinnedVertex), (void*)offsetof(SkinnedVertex, boneIds));

        // Bone Weights: location 4
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void*)offsetof(SkinnedVertex, weights));

        glBindVertexArray(0);
    }

    void cleanup() {
        if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
        if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
        if (m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
    }

    std::vector<SkinnedVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    Skeleton m_skeleton;
    std::vector<AnimationClip> m_animations;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
};

} // namespace Animation
