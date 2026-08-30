#include "engine/Renderer/MeshLoader.h"
#include <iostream>
#include <numbers>
#include <unordered_map>
#include <glm/gtc/constants.hpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

std::shared_ptr<Mesh3D> MeshLoader::LoadOBJ(const std::string& path, bool flipY) { return LoadOBJ(path.c_str(), flipY); }

std::shared_ptr<Mesh3D> MeshLoader::LoadOBJ(const char* path, bool flipY) {
    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate = true;
    cfg.vertex_color = true;
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, cfg)) {
        std::cerr << "[MeshLoader] failed " << path << ": " << reader.Error() << "\n";
        return std::make_shared<Mesh3D>(Mesh3D::Cube(1.0f));
    }
    if (!reader.Warning().empty()) std::cout << "[MeshLoader] warn: " << reader.Warning() << "\n";
    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();

    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;
    verts.reserve(32768);
    idx.reserve(65536);

    struct IndexKey {
        int v, vt, vn;
        bool operator==(const IndexKey& o) const { return v == o.v && vt == o.vt && vn == o.vn; }
    };
    struct IndexKeyHash {
        size_t operator()(const IndexKey& k) const noexcept {
            size_t h = std::hash<int>{}(k.v);
            h = (h * 397) ^ std::hash<int>{}(k.vt);
            h = (h * 397) ^ std::hash<int>{}(k.vn);
            return h;
        }
    };
    std::unordered_map<IndexKey, uint32_t, IndexKeyHash> uniqueVerts;
    uniqueVerts.reserve(65536);

    for (auto& shape : shapes) {
        size_t offset = 0;
        for (size_t f=0; f<shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];
            for (int v=0; v<fv; ++v) {
                tinyobj::index_t i = shape.mesh.indices[offset+v];
                IndexKey key{ i.vertex_index, i.texcoord_index, i.normal_index };
                auto it = uniqueVerts.find(key);
                if (it != uniqueVerts.end()) {
                    idx.push_back(it->second);
                } else {
                    uint32_t newIdx = static_cast<uint32_t>(verts.size());
                    uniqueVerts[key] = newIdx;
                    idx.push_back(newIdx);

                    Vertex vert;
                    vert.position = {
                        attrib.vertices[3*i.vertex_index+0],
                        attrib.vertices[3*i.vertex_index+1],
                        attrib.vertices[3*i.vertex_index+2]
                    };
                    if (i.normal_index>=0) {
                        vert.normal = {
                            attrib.normals[3*i.normal_index+0],
                            attrib.normals[3*i.normal_index+1],
                            attrib.normals[3*i.normal_index+2]
                        };
                    } else {
                        vert.normal = {0,0,1};
                    }
                    if (i.texcoord_index>=0) {
                        vert.texCoord = {
                            attrib.texcoords[2*i.texcoord_index+0],
                            attrib.texcoords[2*i.texcoord_index+1]
                        };
                        if (flipY) vert.texCoord.y = 1.0f - vert.texCoord.y;
                    }
                    if (!attrib.colors.empty() && i.vertex_index*3+2 < (int)attrib.colors.size()) {
                        vert.color = { attrib.colors[3*i.vertex_index+0], attrib.colors[3*i.vertex_index+1], attrib.colors[3*i.vertex_index+2] };
                    } else vert.color = glm::vec3(1.0f);
                    verts.push_back(vert);
                }
            }
            offset += fv;
        }
    }
    if (verts.empty()) {
        std::cerr << "[MeshLoader] empty " << path << " — fallback cube\n";
        return std::make_shared<Mesh3D>(Mesh3D::Cube(1.0f));
    }
    bool hasNormal = false;
    for(auto& v: verts) if(glm::length(v.normal)>0.5f) {hasNormal=true; break;}
    if(!hasNormal){
        for(size_t i=0;i<idx.size();i+=3){
            Vertex &a=verts[idx[i]], &b=verts[idx[i+1]], &c=verts[idx[i+2]];
            glm::vec3 n = glm::normalize(glm::cross(b.position-a.position, c.position-a.position));
            a.normal=b.normal=c.normal=n;
        }
    }
    // bbox log для удобства масштабирования
    glm::vec3 mn = verts[0].position, mx = mn;
    for(auto& v: verts){ mn = glm::min(mn, v.position); mx = glm::max(mx, v.position); }
    glm::vec3 size = mx - mn; glm::vec3 cen = (mn+mx)*0.5f;
    std::cout << "[MeshLoader] " << path << " verts=" << verts.size() << " tris=" << idx.size()/3
              << " bbox " << size.x << "x" << size.y << "x" << size.z << " center " << cen.x << "," << cen.y << "," << cen.z << "\n";
    return std::make_shared<Mesh3D>(std::move(verts), std::move(idx));
}

std::shared_ptr<Mesh3D> MeshLoader::Cube(float s){ return std::make_shared<Mesh3D>(Mesh3D::Cube(s)); }
std::shared_ptr<Mesh3D> MeshLoader::Quad(float s){ return std::make_shared<Mesh3D>(Mesh3D::Quad(s)); }
std::shared_ptr<Mesh3D> MeshLoader::Plane(float w, float d, int gx, int gz, float uvX, float uvZ){ return std::make_shared<Mesh3D>(Mesh3D::Plane(w, d, gx, gz, uvX, uvZ)); }
std::shared_ptr<Mesh3D> MeshLoader::Sphere(float r,int sec,int stk){
    std::vector<Vertex> v; std::vector<uint32_t> idx;
    for(int i=0;i<=stk;++i){
        float phi = std::numbers::pi_v<float>* (float)i/(float)stk;
        for(int j=0;j<=sec;++j){
            float th = 2*std::numbers::pi_v<float> * (float)j/(float)sec;
            float x = r*std::sin(phi)*std::cos(th);
            float y = r*std::cos(phi);
            float z = r*std::sin(phi)*std::sin(th);
            glm::vec3 p{x,y,z};
            glm::vec3 n = glm::normalize(p);
            glm::vec2 uv{(float)j/sec, 1.0f-(float)i/stk};
            v.push_back({p, glm::vec3(1), n, uv});
        }
    }
    for(int i=0;i<stk;++i) for(int j=0;j<sec;++j){
        int a=i*(sec+1)+j, b=a+sec+1;
        idx.insert(idx.end(), {(uint32_t)a,(uint32_t)b,(uint32_t)(a+1), (uint32_t)b,(uint32_t)(b+1),(uint32_t)(a+1)});
    }
    return std::make_shared<Mesh3D>(std::move(v), std::move(idx));
}
