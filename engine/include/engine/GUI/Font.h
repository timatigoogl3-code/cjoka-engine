#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include "engine/Renderer/Texture.h"

struct Glyph {
    // экранные метрики
    float x0=0,y0=0,x1=0,y1=0;
    float u0=0,v0=0,u1=0,v1=0;
    float xadvance=0;
    float xoff=0, yoff=0;
};

class Font {
public:
    // ttfPath — если nullptr, берет /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
    Font(const char* ttfPath = nullptr, float pixelHeight = 32.0f, int atlasW = 1024, int atlasH = 1024);
    ~Font() = default;

    bool valid() const { return m_atlas && m_atlas->valid(); }
    std::shared_ptr<Texture> atlasTexture() const { return m_atlas; }
    GLuint atlasID() const { return m_atlas ? m_atlas->id() : 0; }

    // codepoint — Unicode (UTF-32), поддерживает ASCII + кириллица + • — и т.д.
    Glyph glyph(uint32_t codepoint) const;
    Glyph glyph(unsigned char c) const { return glyph((uint32_t)c); }

    float lineHeight() const { return m_lineHeight; }
    float ascent() const { return m_ascent; }
    float scale() const { return m_scale; }

    glm::vec2 measure(const std::string& text, float scale = 1.0f) const;

    // утилита декодирования UTF-8
    static uint32_t decodeUTF8(const std::string& s, size_t& i);

private:
    std::shared_ptr<Texture> m_atlas;
    // мапа codepoint -> glyph (покрывает ASCII + кириллицу + символы)
    std::unordered_map<uint32_t, Glyph> m_glyphMap;
    Glyph m_glyphsAscii[128]{}; // быстрый путь для ASCII
    float m_lineHeight = 32.0f;
    float m_ascent = 0;
    float m_scale = 1.0f;
    int m_atlasW = 1024, m_atlasH = 1024;
};
