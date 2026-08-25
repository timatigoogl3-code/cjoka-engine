#pragma once
#include <string>
#include <glad/gl.h>
#include <glm/glm.hpp>

class Shader {
public:
    Shader(const char* vertexSrc, const char* fragmentSrc);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void use() const;
    GLuint id() const { return m_id; }

    void setBool(const char* name, bool v) const;
    void setInt(const char* name, int v) const;
    void setFloat(const char* name, float v) const;
    void setVec2(const char* name, glm::vec2 v) const;
    void setVec3(const char* name, glm::vec3 v) const;
    void setVec4(const char* name, glm::vec4 v) const;
    void setMat3(const char* name, const glm::mat3& m) const;
    void setMat4(const char* name, const glm::mat4& m) const;

    static Shader FromFiles(const char* vertPath, const char* fragPath);

private:
    GLuint m_id = 0;
    static GLuint compile(GLenum type, const char* src);
};
