#pragma once
#include <string>
#include <memory>
#include <glad/gl.h>

class Texture {
public:
    Texture() = default;
    Texture(const std::string& path, bool flipY = true, bool srgb = false);
    Texture(int width, int height, const unsigned char* data, int channels = 4, bool srgb = false);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& o) noexcept;
    Texture& operator=(Texture&& o) noexcept;

    void bind(int slot = 0) const;
    static void unbind(int slot = 0);

    GLuint id() const { return m_id; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    int channels() const { return m_channels; }
    bool valid() const { return m_id != 0; }

    static std::shared_ptr<Texture> Load(const std::string& path, bool flipY = true, bool srgb = false);
    static std::shared_ptr<Texture> White();
    static std::shared_ptr<Texture> Black();
    static std::shared_ptr<Texture> FlatNormal();
    static std::shared_ptr<Texture> Checker(int size = 256, int checker = 32);

private:
    void create(int w, int h, const unsigned char* data, int ch, bool srgb);
    GLuint m_id = 0;
    int m_width = 0, m_height = 0, m_channels = 0;
};
