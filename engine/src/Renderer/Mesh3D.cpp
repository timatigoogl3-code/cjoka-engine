#include "engine/Renderer/Mesh3D.h"
#include <limits>

Mesh3D::Mesh3D(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
    : m_vertices(std::move(vertices)), m_indices(std::move(indices)) { setup(); }

Mesh3D::~Mesh3D() {
    if (m_instanceVBO) glDeleteBuffers(1, &m_instanceVBO);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}
Mesh3D::Mesh3D(Mesh3D&& o) noexcept
    : m_vertices(std::move(o.m_vertices)), m_indices(std::move(o.m_indices)),
      m_vao(o.m_vao), m_vbo(o.m_vbo), m_ebo(o.m_ebo), m_instanceVBO(o.m_instanceVBO), m_indexCount(o.m_indexCount),
      m_minExtents(o.m_minExtents), m_maxExtents(o.m_maxExtents) {
    o.m_vao = o.m_vbo = o.m_ebo = o.m_instanceVBO = 0; o.m_indexCount = 0;
}
Mesh3D& Mesh3D::operator=(Mesh3D&& o) noexcept {
    if (this != &o) {
        if (m_instanceVBO) glDeleteBuffers(1, &m_instanceVBO);
        if (m_ebo) glDeleteBuffers(1, &m_ebo);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        m_vertices = std::move(o.m_vertices);
        m_indices  = std::move(o.m_indices);
        m_vao = o.m_vao; m_vbo = o.m_vbo; m_ebo = o.m_ebo; m_instanceVBO = o.m_instanceVBO;
        m_indexCount = o.m_indexCount;
        m_minExtents = o.m_minExtents;
        m_maxExtents = o.m_maxExtents;
        o.m_vao = o.m_vbo = o.m_ebo = o.m_instanceVBO = 0; o.m_indexCount = 0;
    }
    return *this;
}
void Mesh3D::setupInstancing() const {
    if (m_instanceVBO) return;
    glGenBuffers(1, &m_instanceVBO);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    // layout 4,5,6,7 = mat4 model (4×vec4)
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(4 + i);
        glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(4 + i, 1);
    }
    // layout 8 = albedo+shininess
    glEnableVertexAttribArray(8);
    glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)sizeof(glm::mat4));
    glVertexAttribDivisor(8, 1);
    // layout 9 = emissive
    glEnableVertexAttribArray(9);
    glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(sizeof(glm::mat4) + sizeof(glm::vec4)));
    glVertexAttribDivisor(9, 1);
    glBindVertexArray(0);
}
void Mesh3D::setup() {
    if (m_vertices.empty() || m_indices.empty()) return;
    
    m_minExtents = glm::vec3(std::numeric_limits<float>::max());
    m_maxExtents = glm::vec3(std::numeric_limits<float>::lowest());
    for (const auto& v : m_vertices) {
        m_minExtents = glm::min(m_minExtents, v.position);
        m_maxExtents = glm::max(m_maxExtents, v.position);
    }

    m_indexCount = static_cast<uint32_t>(m_indices.size());
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size()*sizeof(Vertex)), m_vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices.size()*sizeof(uint32_t)), m_indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glBindVertexArray(0);
}
void Mesh3D::draw() const {
    if (empty()) return;
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLint>(m_indexCount), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
void Mesh3D::drawInstanced(const InstanceData* data, size_t count) const {
    if (empty() || count == 0 || !data) return;
    setupInstancing();
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(count * sizeof(InstanceData)), data, GL_DYNAMIC_DRAW);
    glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLint>(m_indexCount), GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(count));
    glBindVertexArray(0);
}
Mesh3D Mesh3D::Triangle(glm::vec3 c0, glm::vec3 c1, glm::vec3 c2) {
    std::vector<Vertex> v = {{{0,0.6f,0},c0},{{-0.5f,-0.3f,0},c1},{{0.5f,-0.3f,0},c2}};
    std::vector<uint32_t> i = {0,1,2};
    return Mesh3D(std::move(v), std::move(i));
}
Mesh3D Mesh3D::Quad(float s) {
    float h=s*0.5f;
    std::vector<Vertex> v = {{ {-h,-h,0},{1,1,1},{0,0,1},{0,0}},{{h,-h,0},{1,1,1},{0,0,1},{1,0}},{{h,h,0},{1,1,1},{0,0,1},{1,1}},{{-h,h,0},{1,1,1},{0,0,1},{0,1}}};
    std::vector<uint32_t> i = {0,1,2,2,3,0};
    return Mesh3D(std::move(v), std::move(i));
}
Mesh3D Mesh3D::Cube(float s) {
    float h=s*0.5f; std::vector<Vertex> v; v.reserve(24);
    auto push=[&](glm::vec3 p0,glm::vec3 p1,glm::vec3 p2,glm::vec3 p3,glm::vec3 n,glm::vec3 c){
        v.push_back({p0,c,n,{0,0}}); v.push_back({p1,c,n,{1,0}}); v.push_back({p2,c,n,{1,1}}); v.push_back({p3,c,n,{0,1}});
    };
    push({h,-h,-h},{h,h,-h},{h,h,h},{h,-h,h},{1,0,0},{1,0.3f,0.3f});
    push({-h,-h,h},{-h,h,h},{-h,h,-h},{-h,-h,-h},{-1,0,0},{0.3f,1,0.3f});
    push({-h,h,-h},{-h,h,h},{h,h,h},{h,h,-h},{0,1,0},{0.3f,0.3f,1});
    push({-h,-h,h},{-h,-h,-h},{h,-h,-h},{h,-h,h},{0,-1,0},{1,1,0.3f});
    push({-h,-h,h},{h,-h,h},{h,h,h},{-h,h,h},{0,0,1},{1,0.3f,1});
    push({h,-h,-h},{-h,-h,-h},{-h,h,-h},{h,h,-h},{0,0,-1},{0.3f,1,1});
    std::vector<uint32_t> idx; idx.reserve(36);
    for(uint32_t f=0;f<6;++f){uint32_t o=f*4; idx.insert(idx.end(),{o,o+1,o+2,o+2,o+3,o});}
    return Mesh3D(std::move(v), std::move(idx));
}
