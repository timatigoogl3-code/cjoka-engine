#include "engine/Renderer/Texture.h"
#include <iostream>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

Texture::Texture(const std::string& path, bool flipY, bool srgb) {
    stbi_set_flip_vertically_on_load(flipY);
    int w,h,ch;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (!data) {
        std::cerr << "[Texture] failed load: " << path << " — " << stbi_failure_reason() << "\n";
        return;
    }
    create(w,h,data,ch,srgb);
    stbi_image_free(data);
}

Texture::Texture(int width, int height, const unsigned char* data, int channels, bool srgb) {
    create(width,height,data,channels,srgb);
}

Texture::~Texture() { if (m_id) glDeleteTextures(1,&m_id); }

Texture::Texture(Texture&& o) noexcept : m_id(o.m_id), m_width(o.m_width), m_height(o.m_height), m_channels(o.m_channels) { o.m_id=0; }
Texture& Texture::operator=(Texture&& o) noexcept {
    if(this!=&o){ if(m_id) glDeleteTextures(1,&m_id); m_id=o.m_id; m_width=o.m_width; m_height=o.m_height; m_channels=o.m_channels; o.m_id=0; }
    return *this;
}

void Texture::create(int w,int h,const unsigned char* data,int ch,bool srgb) {
    m_width=w; m_height=h; m_channels=ch;
    glGenTextures(1,&m_id);
    glBindTexture(GL_TEXTURE_2D,m_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // анизотропия — резко улучшает горшок на расстоянии
    if (GLAD_GL_EXT_texture_filter_anisotropic) {
        float maxAniso = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
        float aniso = std::min(8.0f, maxAniso);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
    }
    GLenum fmt = GL_RGB;
    GLenum internal = GL_RGB;
    if (ch==4){ fmt=GL_RGBA; internal = srgb?GL_SRGB_ALPHA:GL_RGBA; }
    else if(ch==3){ fmt=GL_RGB; internal = srgb?GL_SRGB:GL_RGB; }
    else if(ch==1){ fmt=GL_RED; internal=GL_RED; }
    glTexImage2D(GL_TEXTURE_2D,0,internal,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
    glGenerateMipmap(GL_TEXTURE_2D);
    // трилинейка уже, но явно выставим LOD
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 8);
    glBindTexture(GL_TEXTURE_2D,0);
}

void Texture::bind(int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_id);
}
void Texture::unbind(int slot) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::shared_ptr<Texture> Texture::Load(const std::string& path, bool flipY, bool srgb) {
    auto t = std::make_shared<Texture>(path, flipY, srgb);
    if (!t->valid()) return White();
    return t;
}
std::shared_ptr<Texture> Texture::White() {
    static std::shared_ptr<Texture> white;
    if (!white) {
        unsigned char px[4]={255,255,255,255};
        white = std::make_shared<Texture>(1,1,px,4);
    }
    return white;
}
std::shared_ptr<Texture> Texture::Checker(int size, int checker) {
    std::vector<unsigned char> data(size*size*4);
    for(int y=0;y<size;++y) for(int x=0;x<size;++x){
        bool c = ((x/checker)+(y/checker))%2==0;
        unsigned char v = c?255:40;
        int i=(y*size+x)*4;
        data[i]=v; data[i+1]=v; data[i+2]=v; data[i+3]=255;
    }
    return std::make_shared<Texture>(size,size,data.data(),4);
}
