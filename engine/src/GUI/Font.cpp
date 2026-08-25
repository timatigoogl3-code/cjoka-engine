#include "engine/GUI/Font.h"
#include <fstream>
#include <iostream>
#include <vector>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

Font::Font(const char* ttfPath, float pixelHeight, int atlasW, int atlasH)
    : m_atlasW(atlasW), m_atlasH(atlasH), m_lineHeight(pixelHeight) {
    // Приоритет: 1) переданный путь 2) assets/fonts/NotoSans 3) системные
    std::string first = ttfPath ? ttfPath : "assets/fonts/NotoSans-Regular.ttf";
    const char* fallbacks[] = {
        "assets/fonts/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/lato/Lato-Medium.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        nullptr
    };
    std::vector<unsigned char> ttfData;
    std::string usedPath;
    auto loadFile = [&](const std::string& p) -> bool {
        std::ifstream f(p, std::ios::binary);
        if (!f) return false;
        f.seekg(0, std::ios::end);
        size_t sz = f.tellg();
        if (sz < 1024) return false;
        f.seekg(0);
        ttfData.resize(sz);
        f.read((char*)ttfData.data(), sz);
        return f.good() || sz > 0;
    };
    bool ok = false;
    if (!first.empty() && loadFile(first)) { usedPath = first; ok = true; }
    if (!ok) {
        for (int i = 0; fallbacks[i]; ++i) {
            if (loadFile(fallbacks[i])) { usedPath = fallbacks[i]; ok = true; break; }
        }
    }
    if (!ok) {
        std::cerr << "[Font] failed to load any TTF (tried " << first << ")\n";
        return;
    }
    std::cout << "[Font] loading " << usedPath << " (" << ttfData.size() / 1024 << "KB) h=" << pixelHeight << "\n";

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttfData.data(), 0)) {
        std::cerr << "[Font] stbtt_InitFont failed\n";
        return;
    }
    m_scale = stbtt_ScaleForPixelHeight(&info, pixelHeight);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    m_ascent = ascent * m_scale;
    m_lineHeight = (ascent - descent + lineGap) * m_scale;

    // --- Pack atlas: ASCII + Cyrillic + символы (поддержка всего) ---
    std::vector<unsigned char> bitmap(atlasW * atlasH, 0);
    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, bitmap.data(), atlasW, atlasH, 0, 1, nullptr)) {
        std::cerr << "[Font] PackBegin failed\n";
        return;
    }
    stbtt_PackSetOversampling(&pc, 1, 1); // 2x давало 2× overlap (gw = (x1-x0)*scale без деления)

    const int asciiCount = 95;   // 32..126
    const int cyrCount = 256;    // 0x0400..0x04FF
    std::vector<stbtt_packedchar> chAscii(asciiCount);
    std::vector<stbtt_packedchar> chCyr(cyrCount);
    // отдельные символы
    std::vector<stbtt_packedchar> chBullet(1), chEmDash(1), chEnDash(1), chNbsp(1);

    bool packOk = true;
    packOk &= stbtt_PackFontRange(&pc, ttfData.data(), 0, pixelHeight, 32, asciiCount, chAscii.data()) != 0;
    packOk &= stbtt_PackFontRange(&pc, ttfData.data(), 0, pixelHeight, 0x0400, cyrCount, chCyr.data()) != 0;
    packOk &= stbtt_PackFontRange(&pc, ttfData.data(), 0, pixelHeight, 0x2022, 1, chBullet.data()) != 0; // •
    packOk &= stbtt_PackFontRange(&pc, ttfData.data(), 0, pixelHeight, 0x2014, 1, chEmDash.data()) != 0; // —
    packOk &= stbtt_PackFontRange(&pc, ttfData.data(), 0, pixelHeight, 0x2013, 1, chEnDash.data()) != 0; // –
    packOk &= stbtt_PackFontRange(&pc, ttfData.data(), 0, pixelHeight, 0x00A0, 1, chNbsp.data()) != 0;

    stbtt_PackEnd(&pc);

    if (!packOk) {
        std::cerr << "[Font] PackFontRange failed — атлас мал, пробуй 2048\n";
    }

    m_atlas = std::make_shared<Texture>(atlasW, atlasH, bitmap.data(), 1, false);
    glBindTexture(GL_TEXTURE_2D, m_atlas->id());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    auto store = [&](int codepoint, stbtt_packedchar& pc) {
        Glyph g;
        g.x0 = static_cast<float>(pc.x0); g.y0 = static_cast<float>(pc.y0);
        g.x1 = static_cast<float>(pc.x1); g.y1 = static_cast<float>(pc.y1);
        g.u0 = pc.x0 / static_cast<float>(atlasW); g.v0 = pc.y0 / static_cast<float>(atlasH);
        g.u1 = pc.x1 / static_cast<float>(atlasW); g.v1 = pc.y1 / static_cast<float>(atlasH);
        g.xoff = pc.xoff; g.yoff = pc.yoff;
        g.xadvance = pc.xadvance;
        // пустые глифы (пробел) могут иметь нулевой размер — это норм
        m_glyphMap[codepoint] = g;
        if (codepoint >= 0 && codepoint < 128) m_glyphsAscii[codepoint] = g;
    };

    for (int i = 0; i < asciiCount; ++i) store(32 + i, chAscii[i]);
    for (int i = 0; i < cyrCount; ++i) store(0x0400 + i, chCyr[i]);
    store(0x2022, chBullet[0]);
    store(0x2014, chEmDash[0]);
    store(0x2013, chEnDash[0]);
    store(0x00A0, chNbsp[0]);

    // таб
    if (m_glyphMap.count(' ')) m_glyphsAscii['\t'].xadvance = m_glyphMap[' '].xadvance * 4;
    // fallback '?' уже есть из ASCII

    (void)m_atlasW; (void)m_atlasH;
    std::cout << "[Font] atlas " << atlasW << "x" << atlasH << " — ASCII " << asciiCount << " + Cyrillic " << cyrCount << " + symbols 4"
              << " (" << (packOk ? "ok" : "partial") << ")\n";
}

Glyph Font::glyph(uint32_t cp) const {
    auto it = m_glyphMap.find(cp);
    if (it != m_glyphMap.end()) return it->second;
    auto f = m_glyphMap.find('?');
    if (f != m_glyphMap.end()) return f->second;
    if (cp < 128) return m_glyphsAscii[cp];
    return Glyph{};
}

uint32_t Font::decodeUTF8(const std::string& s, size_t& i) {
    auto c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) { i += 1; return c; }
    if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        uint32_t cp = (static_cast<uint32_t>(c & 0x1F) << 6) | (static_cast<uint32_t>(static_cast<unsigned char>(s[i + 1]) & 0x3F));
        i += 2; return cp;
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
        uint32_t cp = (static_cast<uint32_t>(c & 0x0F) << 12) | (static_cast<uint32_t>(static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                      (static_cast<uint32_t>(static_cast<unsigned char>(s[i + 2]) & 0x3F));
        i += 3; return cp;
    }
    if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
        uint32_t cp = (static_cast<uint32_t>(c & 0x07) << 18) | (static_cast<uint32_t>(static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                      (static_cast<uint32_t>(static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                      (static_cast<uint32_t>(static_cast<unsigned char>(s[i + 3]) & 0x3F));
        i += 4; return cp;
    }
    i += 1; return '?';
}

glm::vec2 Font::measure(const std::string& text, float scale) const {
    float h = m_lineHeight * scale;
    float cur = 0, maxW = 0;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\n') { maxW = std::max(maxW, cur); cur = 0; h += m_lineHeight * scale; ++i; continue; }
        uint32_t cp = decodeUTF8(text, i);
        if (cp == '\n') continue;
        cur += glyph(cp).xadvance * scale;
    }
    maxW = std::max(maxW, cur);
    return {maxW, h};
}
