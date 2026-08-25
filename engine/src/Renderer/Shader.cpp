#include "engine/Renderer/Shader.h"
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

GLuint Shader::compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error: " << log << "\n";
    }
    return s;
}
Shader::Shader(const char* vs, const char* fs) {
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    m_id = glCreateProgram();
    glAttachShader(m_id, v);
    glAttachShader(m_id, f);
    glLinkProgram(m_id);
    GLint ok = 0; glGetProgramiv(m_id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetProgramInfoLog(m_id, sizeof(log), nullptr, log);
        std::cerr << "Program link error: " << log << "\n";
    }
    glDeleteShader(v);
    glDeleteShader(f);
}
Shader::~Shader() { if (m_id) glDeleteProgram(m_id); }
Shader::Shader(Shader&& o) noexcept : m_id(o.m_id) { o.m_id = 0; }
Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) { if (m_id) glDeleteProgram(m_id); m_id = o.m_id; o.m_id = 0; }
    return *this;
}
void Shader::use() const { glUseProgram(m_id); }
void Shader::setBool(const char* n, bool v) const { glUniform1i(glGetUniformLocation(m_id, n), int(v)); }
void Shader::setInt(const char* n, int v) const { glUniform1i(glGetUniformLocation(m_id, n), v); }
void Shader::setFloat(const char* n, float v) const { glUniform1f(glGetUniformLocation(m_id, n), v); }
void Shader::setVec2(const char* n, glm::vec2 v) const { glUniform2fv(glGetUniformLocation(m_id, n), 1, glm::value_ptr(v)); }
void Shader::setVec3(const char* n, glm::vec3 v) const { glUniform3fv(glGetUniformLocation(m_id, n), 1, glm::value_ptr(v)); }
void Shader::setVec4(const char* n, glm::vec4 v) const { glUniform4fv(glGetUniformLocation(m_id, n), 1, glm::value_ptr(v)); }
void Shader::setMat3(const char* n, const glm::mat3& m) const { glUniformMatrix3fv(glGetUniformLocation(m_id, n), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::setMat4(const char* n, const glm::mat4& m) const { glUniformMatrix4fv(glGetUniformLocation(m_id, n), 1, GL_FALSE, glm::value_ptr(m)); }

Shader Shader::FromFiles(const char* vertPath, const char* fragPath) {
    auto readFile = [](const char* p)->std::string {
        FILE* f = fopen(p,"rb");
        if(!f) { std::cerr << "[Shader] not found: " << p << "\n"; return ""; }
        fseek(f,0,SEEK_END); long len=ftell(f); fseek(f,0,SEEK_SET);
        std::string s(len,' '); fread(s.data(),1,len,f); fclose(f); return s;
    };
    std::string vs = readFile(vertPath);
    std::string fs = readFile(fragPath);
    return Shader(vs.c_str(), fs.c_str());
}
