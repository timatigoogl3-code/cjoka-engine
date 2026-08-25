#pragma once
// SceneLoader — загрузка сцены из JSON без хардкода
//   Закинул модель в assets/models/, текстуру в assets/textures/, добавил 5 строк в assets/scene.json — готово.
//   Формат assets/scene.json:
//   {
//     "objects": [
//       { "model": "assets/models/indoor_plant.obj", "texture": "assets/textures/indoor_plant_COL.jpg",
//         "pos": [0,-0.9,-2], "rot": [0,0,0], "scale": [0.45,0.45,0.45], "name": "PlantMain" },
//       { "model": "cube", "pos": [-1.7,0.5,0], "scale": [1,1,1], "color": [1,1,1], "shininess": 64, "name": "CubeA" }
//     ]
//   }
//   model: "cube"|"quad"|"sphere"| путь к .obj
//   F5 в игре — hot-reload scene.json
#include "engine/Scene/Scene.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace SceneLoader {

namespace detail {
inline std::string readFile(const std::string& p) {
    std::ifstream f(p);
    if (!f) return {};
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}
inline std::string trim(const std::string& s){ size_t a=s.find_first_not_of(" \t\r\n"); if(a==std::string::npos) return {}; size_t b=s.find_last_not_of(" \t\r\n"); return s.substr(a,b-a+1); }
inline bool extractStr(const std::string& block, const std::string& key, std::string& out){
    size_t k=block.find(key); if(k==std::string::npos) return false;
    size_t colon=block.find(':',k); if(colon==std::string::npos) return false;
    size_t q1=block.find('"',colon); if(q1==std::string::npos) return false;
    size_t q2=block.find('"',q1+1); if(q2==std::string::npos) return false;
    out=block.substr(q1+1,q2-q1-1); return true;
}
inline bool extractVec3(const std::string& block, const std::string& key, glm::vec3& out){
    size_t k=block.find(key); if(k==std::string::npos) return false;
    size_t lb=block.find('[',k); size_t rb=block.find(']',lb); if(lb==std::string::npos||rb==std::string::npos) return false;
    std::string inside=block.substr(lb+1,rb-lb-1);
    std::replace(inside.begin(), inside.end(), ',', ' ');
    std::stringstream ss(inside);
    float x=0,y=0,z=0; ss>>x>>y>>z; out=glm::vec3(x,y,z); return true;
}
inline bool extractFloat(const std::string& block, const std::string& key, float& out){
    size_t k=block.find(key); if(k==std::string::npos) return false;
    size_t colon=block.find(':',k); if(colon==std::string::npos) return false;
    size_t start=block.find_first_of("-0123456789.",colon+1); if(start==std::string::npos) return false;
    size_t end=block.find_first_not_of("-0123456789.eE+",start); 
    std::string num=block.substr(start,end-start);
    try{ out=std::stof(num); return true; }catch(...){ return false; }
}
} // detail

inline bool load(Scene& scene, const std::string& path = "assets/scene.json") {
    std::string txt = detail::readFile(path);
    if (txt.empty()) {
        std::cout << "[SceneLoader] no " << path << " — пропуск (создай файл чтобы добавлять объекты без кода)\n";
        return false;
    }
    // найдём массив objects
    size_t arr = txt.find("\"objects\"");
    if (arr==std::string::npos) { std::cerr << "[SceneLoader] нет objects в " << path << "\n"; return false; }
    size_t lb = txt.find('[', arr); if(lb==std::string::npos) return false;
    // парсим объекты по балансу {}
    int created=0;
    size_t pos = lb;
    while (true) {
        size_t ob = txt.find('{', pos);
        if (ob==std::string::npos) break;
        // найдём закрывающую } соответствующего уровня (первую } после { с учётом вложенных [] но не {} внутри нет вложенных объектов)
        // объекты не вложены, поэтому ищем ближайшую }
        size_t ce = txt.find('}', ob);
        // но внутри есть массивы pos [x,y,z], нужно пропустить ]
        // найдём реальную } после следующего ]?
        // просто ищем } после последнего ] в блоке: расширяем до следующей }
        // наш формат простой — один объект = { ... } без вложенных {}, значит ce корректно
        // проверим что после ce нет ещё [ не закрытого — но массивы закрываются до }
        if (ce==std::string::npos) break;
        // если внутри блока есть "model": и тд, то это объект
        std::string block = txt.substr(ob, ce-ob+1);
        if (block.find("\"model\"")!=std::string::npos || block.find("\"texture\"")!=std::string::npos) {
            std::string model; detail::extractStr(block,"\"model\"",model);
            std::string texture; detail::extractStr(block,"\"texture\"",texture);
            std::string name; detail::extractStr(block,"\"name\"",name);
            glm::vec3 posV{0}, rotV{0}, sclV{1};
            detail::extractVec3(block,"\"pos\"",posV);
            detail::extractVec3(block,"\"rot\"",rotV);
            detail::extractVec3(block,"\"scale\"",sclV);
            glm::vec3 color; bool hasColor = detail::extractVec3(block,"\"color\"",color);
            float shin=48; detail::extractFloat(block,"\"shininess\"",shin);
            if (model.empty()) model="cube";
            Transform tr{posV, rotV, sclV};
            Material mat; mat.shininess=shin;
            if (hasColor) mat.albedo=color;
            if (!texture.empty()) { mat.diffuseMap=Assets::Texture(texture); mat.useDiffuseMap=mat.diffuseMap && mat.diffuseMap->valid(); }
            // для удобства: если model = cube/quad/sphere — используем примитив
            if (model=="cube" || model=="Cube") scene.createCube(tr, mat, name.empty()?"Cube":name);
            else if (model=="quad" || model=="Quad") scene.createQuad(tr, mat, 1.0f, name.empty()?"Quad":name);
            else if (model=="sphere" || model=="Sphere") scene.createSphere(tr, mat, sclV.x, name.empty()?"Sphere":name);
            else scene.createTexturedModel(model, texture, tr, mat, name);
            ++created;
        }
        pos = ce+1;
        if (pos > lb + 80000) break; // safety
        // закончим когда дошли до закрывающего ] массива objects
        if (txt.find(']', pos) < txt.find('{', pos)) break;
    }
    std::cout << "[SceneLoader] loaded " << created << " objects from " << path << "\n";
    return created>0;
}

inline bool reload(Scene& scene, const std::string& path = "assets/scene.json") {
    // удаляет только спавненные из json? пока просто клир+перезагрузка — вызывай до создания камеры/света если нужно
    // удобный хот-релоад: F5 чистит все MeshRenderer кроме камеры/света
    std::vector<Entity> toDel;
    for (Entity e: scene.registry().view<MeshRenderer>()) toDel.push_back(e);
    for (auto e: toDel) scene.destroy(e);
    return load(scene, path);
}

} // namespace SceneLoader
