#pragma once
namespace DefaultShaders {

// Unlit
inline const char* kUnlitVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
out vec3 vColor; out vec2 vUV;
uniform mat4 uMVP; uniform mat4 uModel;
void main(){ vColor=aColor; vUV=aUV; gl_Position=uMVP*vec4(aPos,1.0); }
)";
inline const char* kUnlitFS = R"(#version 460 core
in vec3 vColor; in vec2 vUV; out vec4 FragColor;
uniform sampler2D uDiffuseMap; uniform bool uUseDiffuseMap;
void main(){ vec3 c=vColor; if(uUseDiffuseMap) c*=texture(uDiffuseMap,vUV).rgb; FragColor=vec4(c,1.0); }
)";

// ---- Z-Prepass (Reverse-Z depth only) ----
// Рисуем только глубину, reverse-Z: far=0.0, near=1.0
inline const char* kZPrepassVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=4) in mat4 aInstanceModel; // для инстансов, unused для обычных
uniform mat4 uMVP;
void main(){ gl_Position = uMVP * vec4(aPos, 1.0); }
)";
inline const char* kZPrepassFS = R"(#version 460 core
void main(){} // depth-only, ничего не пишем
)";

// Z-Prepass Instanced
inline const char* kZPrepassInstancedVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=4) in mat4 aInstanceModel;
uniform mat4 uView; uniform mat4 uProj;
void main(){ gl_Position = uProj * uView * aInstanceModel * vec4(aPos, 1.0); }
)";

// ---- Velocity Buffer шейдеры (MRT: depth + velocity) ----
inline const char* kVelocityVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
out vec3 vWorldPos;
out vec4 vCurrClip;
out vec4 vPrevClip;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uPrevModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uPrevViewProj;
void main(){
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vCurrClip = uProj * uView * wp;
    vec4 prevWp = uPrevModel * vec4(aPos, 1.0);
    vPrevClip = uPrevViewProj * prevWp;
    gl_Position = vCurrClip;
}
)";

inline const char* kVelocityFS = R"(#version 460 core
in vec3 vWorldPos;
in vec4 vCurrClip;
in vec4 vPrevClip;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;
void main(){
    FragColor = vec4(0.0);
    vec2 currNDC = (vCurrClip.xy / vCurrClip.w) * 0.5 + 0.5;
    vec2 prevNDC = (vPrevClip.xy / vPrevClip.w) * 0.5 + 0.5;
    Velocity = currNDC - prevNDC;
}
)";

// Velocity Instanced (для ClusterLOD/Batcher)
inline const char* kVelocityInstancedVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
layout(location=4) in mat4 aInstanceModel;
layout(location=8) in mat4 aPrevInstanceModel;
layout(location=12) in vec4 aInstanceAlbedo;
layout(location=13) in vec4 aInstanceEmissive;
out vec3 vWorldPos;
out vec4 vCurrClip;
out vec4 vPrevClip;
uniform mat4 uView; uniform mat4 uProj;
uniform mat4 uPrevViewProj;
void main(){
    vec4 wp = aInstanceModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vCurrClip = uProj * uView * wp;
    vec4 prevWp = aPrevInstanceModel * vec4(aPos, 1.0);
    vPrevClip = uPrevViewProj * prevWp;
    gl_Position = vCurrClip;
}
)";

// Shadow depth pass — рисуем только глубину с точки света
inline const char* kShadowDepthVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
uniform mat4 uLightMatrix;
void main(){ gl_Position = uLightMatrix * vec4(aPos,1.0); }
)";
inline const char* kShadowDepthFS = R"(#version 460 core
void main(){}
)";

// Lit — красивый Blinn-Phong + Fresnel + Rim + Fog + Gamma + Tonemapping
inline const char* kLitVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
out vec3 vColor; out vec2 vUV; out vec3 vNormal; out vec3 vWorldPos; out vec3 vTangent; out vec3 vBitangent;
out vec4 vCurrClip; out vec4 vPrevClip;
uniform mat4 uMVP; uniform mat4 uModel; uniform mat3 uNormalMat;
uniform mat4 uPrevModel; uniform mat4 uPrevViewProj;
void main(){
    vColor=aColor; vUV=aUV;
    vec4 wp=uModel*vec4(aPos,1.0); vWorldPos=wp.xyz;
    vNormal=normalize(uNormalMat*aNormal);
    vec3 N = vNormal;
    vec3 c1 = cross(N, vec3(0.0, 0.0, 1.0));
    vec3 c2 = cross(N, vec3(0.0, 1.0, 0.0));
    vec3 T = length(c1) > length(c2) ? normalize(c1) : normalize(c2);
    vec3 B = cross(N, T);
    vTangent = T;
    vBitangent = B;
    // Motion vectors
    vCurrClip = uMVP * vec4(aPos, 1.0);
    vec4 prevWp = uPrevModel * vec4(aPos, 1.0);
    vPrevClip = uPrevViewProj * prevWp;
    gl_Position = vCurrClip;
}
)";
inline const char* kLitFS = R"(#version 460 core
in vec3 vColor; in vec2 vUV; in vec3 vNormal; in vec3 vWorldPos; in vec3 vTangent; in vec3 vBitangent;
in vec4 vCurrClip; in vec4 vPrevClip;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalRoughness;
uniform vec3 uViewPos;
uniform vec3 uAmbientColor; uniform float uAmbientIntensity;
struct DirLight{ vec3 direction; vec3 color; float intensity; };
uniform DirLight uDirLight; uniform bool uHasDirLight;
struct PointLight{ vec4 posRad; vec4 colInt; };
layout(std430, binding = 0) readonly buffer LightBuffer { uint uNumLights; PointLight pointLights[]; };
struct Cluster { uint lightCount; uint lightIndices[127]; };
layout(std430, binding = 1) readonly buffer ClusterBuffer { Cluster clusters[]; };

uniform mat4 uView;
uniform vec2 uScreenSize;
uniform float uZNear;
uniform float uZFar;
const uvec3 GRID_SIZE = uvec3(16, 9, 24);

// PBR
uniform vec3 uAlbedo; uniform float uMetallic; uniform float uRoughness; uniform float uAO;
uniform vec3 uEmissive;
uniform sampler2D uDiffuseMap; uniform bool uUseDiffuseMap;
uniform sampler2D uNormalMap; uniform bool uUseNormalMap;
uniform sampler2D uSpecularMap; uniform bool uUseSpecularMap;
// cascaded shadows
uniform mat4 uLightMatrices[3];
uniform float uCascadeSplits[4];
uniform sampler2DArray uShadowMapArray;
uniform bool uHasShadow;
uniform vec3 uFogColor; uniform float uFogDensity; uniform float uExposure;
// sky environment IBL
uniform vec3 uSkyTop; uniform vec3 uSkyHorizon; uniform vec3 uSkyBottom; uniform float uSkyExposure;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float rough){
    float a = rough*rough; float a2 = a*a;
    float NoH = max(dot(N,H),0.0);
    float d = NoH*NoH*(a2-1.0)+1.0;
    return a2/(PI*d*d+1e-7);
}
float GeometrySmith(float NoV, float NoL, float rough){
    float k = (rough+1.0); k = k*k/8.0;
    float gv = NoV/(NoV*(1.0-k)+k);
    float gl = NoL/(NoL*(1.0-k)+k);
    return gv*gl;
}
vec3 FresnelSchlick(float cosTheta, vec3 F0){
    return F0 + (1.0-F0)*pow(clamp(1.0-cosTheta,0.0,1.0),5.0);
}
float shadowFactor(vec3 worldPos, vec3 N, vec3 L, float viewDist){
    if(!uHasShadow) return 1.0;
    
    // Select cascade index based on view distance
    int cascadeIndex = 2;
    if (viewDist < uCascadeSplits[1]) {
        cascadeIndex = 0;
    } else if (viewDist < uCascadeSplits[2]) {
        cascadeIndex = 1;
    }

    vec4 lp = uLightMatrices[cascadeIndex] * vec4(worldPos, 1.0);
    vec3 proj = lp.xyz / lp.w * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 1.0;

    float bias = max(0.0020 * (1.0 - dot(N, L)), 0.0006) * (1.0 + float(cascadeIndex) * 1.5);
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMapArray, 0).xy);
    float radius = (1.4 + float(cascadeIndex) * 0.8) * texelSize.x;

    vec2 poisson[16] = vec2[](
        vec2(-0.326,-0.406), vec2(-0.840,-0.074), vec2(-0.696, 0.457), vec2(-0.203, 0.621),
        vec2( 0.962,-0.195), vec2( 0.473,-0.480), vec2( 0.519, 0.767), vec2( 0.185,-0.893),
        vec2( 0.507, 0.064), vec2( 0.896, 0.412), vec2(-0.321,-0.932), vec2(-0.791,-0.597),
        vec2(-0.308, 0.916), vec2( 0.134,-0.452), vec2(-0.612, 0.143), vec2( 0.421,-0.842)
    );

    float s = 0.0;
    for(int i = 0; i < 16; ++i) {
        float d = texture(uShadowMapArray, vec3(proj.xy + poisson[i] * radius, float(cascadeIndex))).r;
        s += (proj.z - bias > d) ? 0.0 : 1.0;
    }
    return s * 0.0625;
}
vec3 ApplyLight(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 F0, vec3 albedo, float rough, float metallic){
    vec3 H = normalize(V+L);
    float NoV = max(dot(N,V),1e-4);
    float NoL = max(dot(N,L),0.0);
    float NoH = max(dot(N,H),0.0);
    float VoH = max(dot(V,H),0.0);
    vec3 F = FresnelSchlick(VoH, F0);
    float D = DistributionGGX(N,H,rough);
    float G = GeometrySmith(NoV,NoL,rough);
    vec3 spec = D*F*G/max(4.0*NoV*NoL,1e-4);
    vec3 kd = (1.0-F)*(1.0-metallic);
    return (kd*albedo/PI + spec)*radiance*NoL;
}

vec3 SampleSky(vec3 dir, float rough) {
    float t = clamp(dir.y * (1.0 - rough * 0.7) * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyColor = mix(uSkyBottom, uSkyTop, t);
    if(dir.y > -0.15 && dir.y < 0.35) {
        float h = 1.0 - abs(dir.y - 0.1) / 0.25;
        skyColor = mix(skyColor, uSkyHorizon, clamp(h, 0.0, 1.0));
    }
    return skyColor * uSkyExposure;
}

void main(){
    vec3 N = normalize(vNormal);
    
    // Normal mapping
    if(uUseNormalMap) {
        vec3 T = normalize(vTangent);
        vec3 B = normalize(vBitangent);
        mat3 TBN = mat3(T, B, N);
        vec3 normalMap = texture(uNormalMap, vUV).rgb;
        normalMap = normalMap * 2.0 - 1.0;
        N = normalize(TBN * normalMap);
    }
    
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-V, N);
    float NoV = max(dot(N, V), 1e-4);

    vec3 albedo = uAlbedo * vColor;
    if(uUseDiffuseMap) albedo *= texture(uDiffuseMap,vUV).rgb;
    float metallic = clamp(uMetallic,0.0,1.0);
    float rough = clamp(uRoughness,0.04,1.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // Specular map: R=specular intensity, G=glossiness(1-roughness), B=fresnel
    if(uUseSpecularMap) {
        vec3 specMap = texture(uSpecularMap, vUV).rgb;
        rough = clamp(1.0 - specMap.g, 0.04, 1.0);
        F0 = mix(F0, albedo, specMap.r * 0.5);
    }

    // PBR Environment IBL (Diffuse + Specular)
    vec3 F_env = F0 + (max(vec3(1.0 - rough), F0) - F0) * pow(clamp(1.0 - NoV, 0.0, 1.0), 5.0);
    vec3 kD_env = (vec3(1.0) - F_env) * (1.0 - metallic);
    vec3 diffuseEnv = kD_env * albedo * (uAmbientColor * uAmbientIntensity * (0.5 + 0.5 * N.y));
    
    vec3 skySpecular = SampleSky(R, rough);
    vec3 specularEnv = F_env * skySpecular;
    vec3 ambient = (diffuseEnv + specularEnv) * uAO;

    vec3 Lo = vec3(0.0);
    if(uHasDirLight){
        vec3 L = normalize(-uDirLight.direction);
        float NoL = dot(N, L);
        if(NoL > 0.0){
            float sh = shadowFactor(vWorldPos,N,L, length(vWorldPos-uViewPos));
            vec3 radiance = uDirLight.color*uDirLight.intensity;
            Lo += ApplyLight(N,V,L,radiance,F0,albedo,rough,metallic)*sh;
        }
    }
    
    // Point lights evaluation (Full PBR Radiance)
    uint nLights = min(uNumLights, 64u);
    for(uint i = 0u; i < nLights; ++i){
        PointLight pl = pointLights[i];
        vec3 lpos = pl.posRad.xyz;
        float lrange = pl.posRad.w;
        vec3 lcol = pl.colInt.xyz;
        float lintensity = pl.colInt.w;
        
        vec3 toL = lpos - vWorldPos;
        float dist = length(toL);
        if(dist >= lrange || dist <= 1e-4) continue;
        vec3 L = toL / dist;
        
        float atten = 1.0 / (1.0 + 0.08 * dist + 0.025 * dist * dist);
        float dRel = dist / lrange;
        float rangeFade = clamp(1.0 - dRel * dRel * dRel * dRel, 0.0, 1.0);
        rangeFade = rangeFade * rangeFade;
        Lo += ApplyLight(N, V, L, lcol * lintensity * atten * rangeFade, F0, albedo, rough, metallic);
    }

    vec3 color = ambient + Lo + uEmissive;
    // fog
    float fogF = exp(-uFogDensity*length(vWorldPos-uViewPos));
    color = mix(uFogColor,color,clamp(fogF,0.0,1.0));
    FragColor = vec4(color,1.0);
    vec3 viewN = normalize(mat3(uView) * N);
    NormalRoughness = vec4(viewN, rough);
}
)";

// Lit Instanced — для батчинга (model per instance) + motion vectors + normal maps
inline const char* kLitInstancedVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
layout(location=4) in mat4 aInstanceModel; // 4,5,6,7
layout(location=8) in mat4 aPrevInstanceModel; // 8,9,10,11 — motion vectors
layout(location=12) in vec4 aInstanceAlbedo; // rgb=albedo, a=roughness
layout(location=13) in vec4 aInstanceEmissive; // rgb=emissive, a=metallic
out vec3 vColor; out vec2 vUV; out vec3 vNormal; out vec3 vWorldPos;
out vec3 vTangent; out vec3 vBitangent;
out vec4 vInstAlbedo; out vec4 vInstEmissive;
out vec4 vCurrClip; out vec4 vPrevClip;
uniform mat4 uView; uniform mat4 uProj;
uniform mat4 uPrevViewProj;
void main(){
    vColor=aColor; vUV=aUV;
    vInstAlbedo=aInstanceAlbedo; vInstEmissive=aInstanceEmissive;
    vec4 wp=aInstanceModel*vec4(aPos,1.0); vWorldPos=wp.xyz;
    mat3 nm=transpose(inverse(mat3(aInstanceModel)));
    vNormal=normalize(nm*aNormal);
    vec3 T=normalize(nm*vec3(1.0,0.0,0.0)); vec3 N=vNormal;
    T=normalize(T-dot(T,N)*N); vTangent=T; vBitangent=cross(N,T);
    vCurrClip=uProj*uView*wp;
    vec4 prevWp=aPrevInstanceModel*vec4(aPos,1.0);
    vPrevClip=uPrevViewProj*prevWp;
    gl_Position=vCurrClip;
}
)";
inline const char* kLitInstancedFS = R"(#version 460 core
in vec3 vColor; in vec2 vUV; in vec3 vNormal; in vec3 vWorldPos;
in vec3 vTangent; in vec3 vBitangent;
in vec4 vInstAlbedo; in vec4 vInstEmissive;
in vec4 vCurrClip; in vec4 vPrevClip;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalRoughness;
uniform vec3 uViewPos;
uniform vec3 uAmbientColor; uniform float uAmbientIntensity;
struct DirLight{ vec3 direction; vec3 color; float intensity; }; uniform DirLight uDirLight; uniform bool uHasDirLight;
struct PointLight{ vec4 posRad; vec4 colInt; };
layout(std430, binding = 0) readonly buffer LightBuffer { uint uNumLights; PointLight pointLights[]; };
struct Cluster { uint lightCount; uint lightIndices[127]; };
layout(std430, binding = 1) readonly buffer ClusterBuffer { Cluster clusters[]; };

uniform mat4 uView;
uniform vec2 uScreenSize;
uniform float uZNear;
uniform float uZFar;
const uvec3 GRID_SIZE = uvec3(16, 9, 24);

uniform sampler2D uDiffuseMap; uniform bool uUseDiffuseMap;
uniform sampler2D uNormalMap; uniform bool uUseNormalMap;
uniform sampler2D uSpecularMap; uniform bool uUseSpecularMap;
uniform vec3 uFogColor; uniform float uFogDensity;
// cascaded shadows
uniform mat4 uLightMatrices[3];
uniform float uCascadeSplits[4];
uniform sampler2DArray uShadowMapArray;
uniform bool uHasShadow;
// sky environment IBL
uniform vec3 uSkyTop; uniform vec3 uSkyHorizon; uniform vec3 uSkyBottom; uniform float uSkyExposure;

const float PI = 3.14159265359;

vec3 SampleSkyI(vec3 dir, float rough) {
    float t = clamp(dir.y * (1.0 - rough * 0.7) * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyColor = mix(uSkyBottom, uSkyTop, t);
    if(dir.y > -0.15 && dir.y < 0.35) {
        float h = 1.0 - abs(dir.y - 0.1) / 0.25;
        skyColor = mix(skyColor, uSkyHorizon, clamp(h, 0.0, 1.0));
    }
    return skyColor * uSkyExposure;
}

float DistributionGGX(vec3 N, vec3 H, float rough){
    float a = rough*rough; float a2 = a*a;
    float NoH = max(dot(N,H),0.0);
    float d = NoH*NoH*(a2-1.0)+1.0;
    return a2/(PI*d*d+1e-7);
}
float GeometrySmith(float NoV, float NoL, float rough){
    float k = (rough+1.0); k = k*k/8.0;
    float gv = NoV/(NoV*(1.0-k)+k);
    float gl = NoL/(NoL*(1.0-k)+k);
    return gv*gl;
}
vec3 FresnelSchlick(float cosTheta, vec3 F0){
    return F0 + (1.0-F0)*pow(clamp(1.0-cosTheta,0.0,1.0),5.0);
}
float shadowFactorI(vec3 worldPos, vec3 N, vec3 L, float viewDist){
    if(!uHasShadow) return 1.0;
    
    int cascadeIndex = 2;
    if (viewDist < uCascadeSplits[1]) {
        cascadeIndex = 0;
    } else if (viewDist < uCascadeSplits[2]) {
        cascadeIndex = 1;
    }

    vec4 lp = uLightMatrices[cascadeIndex] * vec4(worldPos, 1.0);
    vec3 proj = lp.xyz / lp.w * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 1.0;

    float bias = max(0.0020 * (1.0 - dot(N, L)), 0.0006) * (1.0 + float(cascadeIndex) * 1.5);
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMapArray, 0).xy);
    float radius = (1.4 + float(cascadeIndex) * 0.8) * texelSize.x;

    vec2 poisson[16] = vec2[](
        vec2(-0.326,-0.406), vec2(-0.840,-0.074), vec2(-0.696, 0.457), vec2(-0.203, 0.621),
        vec2( 0.962,-0.195), vec2( 0.473,-0.480), vec2( 0.519, 0.767), vec2( 0.185,-0.893),
        vec2( 0.507, 0.064), vec2( 0.896, 0.412), vec2(-0.321,-0.932), vec2(-0.791,-0.597),
        vec2(-0.308, 0.916), vec2( 0.134,-0.452), vec2(-0.612, 0.143), vec2( 0.421,-0.842)
    );

    float s = 0.0;
    for(int i = 0; i < 16; ++i) {
        float d = texture(uShadowMapArray, vec3(proj.xy + poisson[i] * radius, float(cascadeIndex))).r;
        s += (proj.z - bias > d) ? 0.0 : 1.0;
    }
    return s * 0.0625;
}
vec3 ApplyLight(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 F0, vec3 albedo, float rough, float metallic){
    vec3 Hv = normalize(V+L);
    float NoV = max(dot(N,V),1e-4);
    float NoL = max(dot(N,L),0.0);
    float VoH = max(dot(V,Hv),0.0);
    vec3 F = FresnelSchlick(VoH, F0);
    float D = DistributionGGX(N,Hv,rough);
    float G = GeometrySmith(NoV,NoL,rough);
    vec3 spec = D*F*G/max(4.0*NoV*NoL,1e-4);
    vec3 kd = (1.0-F)*(1.0-metallic);
    return (kd*albedo/PI + spec)*radiance*NoL;
}

void main(){
    vec3 N=normalize(vNormal);
    if(uUseNormalMap){vec3 T=normalize(vTangent);vec3 B=normalize(vBitangent);mat3 TBN=mat3(T,B,N);vec3 nm=texture(uNormalMap,vUV).rgb*2.0-1.0;N=normalize(TBN*nm);}
    vec3 V=normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-V, N);
    float NoV = max(dot(N, V), 1e-4);

    vec3 albedo=vInstAlbedo.rgb * vColor;
    float rough = clamp(vInstAlbedo.a, 0.04, 1.0);
    float metallic = clamp(vInstEmissive.a, 0.0, 1.0);
    if(uUseDiffuseMap) albedo*=texture(uDiffuseMap,vUV).rgb;
    if(uUseSpecularMap){vec3 sm=texture(uSpecularMap,vUV).rgb;rough=clamp(1.0-sm.g,0.04,1.0);albedo=mix(albedo,albedo*sm.r,0.3);}
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 F_env = F0 + (max(vec3(1.0 - rough), F0) - F0) * pow(clamp(1.0 - NoV, 0.0, 1.0), 5.0);
    vec3 kD_env = (vec3(1.0) - F_env) * (1.0 - metallic);
    vec3 diffuseEnv = kD_env * albedo * (uAmbientColor * uAmbientIntensity * (0.5 + 0.5 * N.y));
    
    vec3 skySpecular = SampleSkyI(R, rough);
    vec3 specularEnv = F_env * skySpecular;
    vec3 ambient = (diffuseEnv + specularEnv);

    vec3 Lo=vec3(0.0);
    if(uHasDirLight){
        vec3 L=normalize(-uDirLight.direction);
        float NoL = dot(N, L);
        if(NoL > 0.0){
            float sh=shadowFactorI(vWorldPos,N,L, length(vWorldPos-uViewPos));
            Lo += ApplyLight(N,V,L,uDirLight.color*uDirLight.intensity,F0,albedo,rough,metallic)*sh;
        }
    }
    // Point lights evaluation (Full PBR Radiance)
    uint nLights = min(uNumLights, 64u);
    for(uint i = 0u; i < nLights; ++i){
        PointLight pl = pointLights[i];
        vec3 lpos = pl.posRad.xyz;
        float lrange = pl.posRad.w;
        vec3 lcol = pl.colInt.xyz;
        float lintensity = pl.colInt.w;
        
        vec3 toL = lpos - vWorldPos;
        float dist = length(toL);
        if(dist >= lrange || dist <= 1e-4) continue;
        vec3 L = toL / dist;
        
        float atten = 1.0 / (1.0 + 0.08 * dist + 0.025 * dist * dist);
        float dRel = dist / lrange;
        float rangeFade = clamp(1.0 - dRel * dRel * dRel * dRel, 0.0, 1.0);
        rangeFade = rangeFade * rangeFade;
        Lo += ApplyLight(N, V, L, lcol * lintensity * atten * rangeFade, F0, albedo, rough, metallic);
    }

    vec3 color = ambient + Lo + vInstEmissive.rgb;
    float fogF=exp(-uFogDensity*length(vWorldPos-uViewPos));
    color=mix(uFogColor,color,clamp(fogF,0.0,1.0));
    FragColor=vec4(color,1.0);
    vec3 viewN = normalize(mat3(uView) * N);
    NormalRoughness = vec4(viewN, rough);
}
)";

// Sky — градиент
inline const char* kSkyVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
out vec3 vDir;
uniform mat4 uView; uniform mat4 uProj;
void main(){
    vDir=aPos;
    vec4 pos=uProj*uView*vec4(aPos,1.0);
    gl_Position=pos.xyww;
}
)";
inline const char* kSkyFS = R"(#version 460 core
layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 NormalRoughness;
in vec3 vDir;
uniform vec3 uTopColor; uniform vec3 uHorizonColor; uniform vec3 uBottomColor; uniform float uTime;
void main(){
    vec3 dir=normalize(vDir);
    float t=clamp(dir.y*0.5+0.5, 0.0,1.0);
    vec3 col=mix(uBottomColor, uHorizonColor, smoothstep(0.0,0.45,t));
    col=mix(col, uTopColor, smoothstep(0.45,1.0,t));
    vec3 sunDir=normalize(vec3(0.4,0.6,0.2));
    float sun=pow(max(dot(dir,sunDir),0.0), 128.0)*0.9;
    col+=sun*vec3(1.0,0.95,0.7);
    float n=fract(sin(dot(dir.xz, vec2(12.9898,78.233)))*43758.5);
    float stars=smoothstep(0.985,1.0,n)*pow(max(dir.y,0.0),2.0)*0.4;
    col+=stars;
    FragColor=vec4(col,1.0);
    NormalRoughness=vec4(0.0,0.0,-1.0,1.0);
}
)";

// Post — fullscreen
inline const char* kPostVS = R"(#version 460 core
layout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV;
out vec2 vUV;
void main(){ vUV=aUV; gl_Position=vec4(aPos,0.0,1.0); }
)";
inline const char* kTonemapFS = R"(#version 460 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uHDR; uniform float uExposure; uniform float uGamma;
vec3 aces(vec3 x){ float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14; return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0); }
void main(){
    vec3 hdr=texture(uHDR, vUV).rgb;
    hdr*=uExposure;
    vec3 mapped=aces(hdr);
    mapped=pow(mapped, vec3(1.0/uGamma));
    float lum=dot(mapped, vec3(0.2126,0.7152,0.0722));
    mapped=mix(vec3(lum), mapped, 1.06);
    FragColor=vec4(mapped,1.0);
}
)";
inline const char* kBloomExtractFS = R"(#version 460 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uHDR; uniform float uThreshold;
void main(){
    vec3 c=texture(uHDR, vUV).rgb;
    float bright=dot(c, vec3(0.2126,0.7152,0.0722));
    // soft threshold
    float t = clamp((bright - uThreshold)*2.0, 0.0, 1.0);
    FragColor=vec4(c*t,1.0);
}
)";
inline const char* kBlurFS = R"(#version 460 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uTex; uniform vec2 uDir; // (1/w,0) or (0,1/h)
void main(){
    vec2 texel = uDir;
    vec3 c=texture(uTex, vUV).rgb * 0.227027;
    c+=texture(uTex, vUV+texel*1.384615).rgb * 0.316216;
    c+=texture(uTex, vUV-texel*1.384615).rgb * 0.316216;
    c+=texture(uTex, vUV+texel*3.230769).rgb * 0.070270;
    c+=texture(uTex, vUV-texel*3.230769).rgb * 0.070270;
    FragColor=vec4(c,1.0);
}
)";
inline const char* kCompositeFS = R"(#version 460 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uScene; uniform sampler2D uBloom; uniform float uBloomIntensity; uniform float uVignette;
uniform float uExposure; uniform float uGamma; uniform float uSaturation;
vec3 aces(vec3 x){ float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14; return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0); }
void main(){
    vec3 scene=texture(uScene, vUV).rgb;
    vec3 bloom=texture(uBloom, vUV).rgb;
    vec3 col=scene + bloom*uBloomIntensity;
    // vignette
    float dist=distance(vUV, vec2(0.5));
    float vign=1.0 - uVignette*pow(dist*1.8, 1.8);
    col*=vign;
    // HDR -> LDR: exposure + ACES + gamma + лёгкая сатурация
    col *= uExposure;
    col = aces(col);
    col = pow(col, vec3(1.0/uGamma));
    float lum = dot(col, vec3(0.2126,0.7152,0.0722));
    col = mix(vec3(lum), col, uSaturation);
    FragColor=vec4(col,1.0);
}
)";
inline const char* kFXAAFS = R"(#version 460 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uTex; uniform vec2 uTexel; // 1/w,1/h
void main(){
    vec3 rgbNW=texture(uTex, vUV+vec2(-1,-1)*uTexel).rgb;
    vec3 rgbNE=texture(uTex, vUV+vec2( 1,-1)*uTexel).rgb;
    vec3 rgbSW=texture(uTex, vUV+vec2(-1, 1)*uTexel).rgb;
    vec3 rgbSE=texture(uTex, vUV+vec2( 1, 1)*uTexel).rgb;
    vec3 rgbM =texture(uTex, vUV).rgb;
    float lumaNW=dot(rgbNW, vec3(0.299,0.587,0.114));
    float lumaNE=dot(rgbNE, vec3(0.299,0.587,0.114));
    float lumaSW=dot(rgbSW, vec3(0.299,0.587,0.114));
    float lumaSE=dot(rgbSE, vec3(0.299,0.587,0.114));
    float lumaM =dot(rgbM,  vec3(0.299,0.587,0.114));
    float lumaMin=min(lumaM, min(min(lumaNW,lumaNE),min(lumaSW,lumaSE)));
    float lumaMax=max(lumaM, max(max(lumaNW,lumaNE),max(lumaSW,lumaSE)));
    vec2 dir=vec2(-((lumaNW+lumaNE)-(lumaSW+lumaSE)), ((lumaNW+lumaSW)-(lumaNE+lumaSE)));
    float dirReduce=max((lumaNW+lumaNE+lumaSW+lumaSE)*0.25*0.007, 0.007);
    float rcpDirMin=1.0/(min(abs(dir.x),abs(dir.y))+dirReduce);
    dir=min(vec2(8.0), max(vec2(-8.0), dir*rcpDirMin))*uTexel;
    vec3 rgbA=0.5*(texture(uTex,vUV+dir*0.16666).rgb + texture(uTex,vUV-dir*0.16666).rgb);
    vec3 rgbB=rgbA*0.5 + 0.25*(texture(uTex,vUV+dir*0.5).rgb + texture(uTex,vUV-dir*0.5).rgb);
    float lumaB=dot(rgbB, vec3(0.299,0.587,0.114));
    if(lumaB < lumaMin || lumaB > lumaMax) FragColor=vec4(rgbA,1.0);
    else FragColor=vec4(rgbB,1.0);
}
)";

// kGUI — текст/панели (2D ortho)
inline const char* kGUIVS = R"(#version 460 core
layout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV; layout(location=2) in vec4 aColor;
out vec2 vUV; out vec4 vColor;
uniform mat4 uProj;
void main(){ vUV=aUV; vColor=aColor; gl_Position=uProj*vec4(aPos,0.0,1.0); }
)";
inline const char* kGUIFS = R"(#version 460 core
in vec2 vUV; in vec4 vColor; out vec4 FragColor;
uniform sampler2D uFont; uniform bool uUseFont; uniform vec4 uTint;
void main(){
    vec4 c=vColor * uTint;
    if(uUseFont){
        float a=texture(uFont, vUV).r;
        // signed distance field like smoothing
        float edge=0.45; float w=fwidth(a);
        float alpha=smoothstep(edge-w, edge+w, a);
        c.a*=alpha;
        if(c.a<0.02) discard;
    }
    FragColor=c;
}
)";

// Instanced cluster lit — model matrix per instance, всё остальное из uniform.
// Выходы те же что kLitVS → kLitFS работает без изменений.
inline const char* kClusterInstancedVS = R"(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
layout(location=4) in mat4 aModel; // 4,5,6,7 — per-instance
out vec3 vColor; out vec2 vUV; out vec3 vNormal; out vec3 vWorldPos;
out vec3 vTangent; out vec3 vBitangent;
out vec4 vCurrClip; out vec4 vPrevClip;
uniform mat4 uView; uniform mat4 uProj;
void main(){
    vColor=aColor; vUV=aUV;
    vec4 wp = aModel * vec4(aPos,1.0);
    vWorldPos = wp.xyz;
    mat3 nm = transpose(inverse(mat3(aModel)));
    vec3 N = normalize(nm * aNormal);
    vNormal = N;
    vec3 T = normalize(nm * vec3(1.0, 0.0, 0.0));
    T = normalize(T - dot(T, N) * N);
    vTangent = T;
    vBitangent = cross(N, T);
    vCurrClip = uProj * uView * wp;
    vPrevClip = vCurrClip;
    gl_Position = vCurrClip;
}
)";

// Instanced cluster shadow — depth only
inline const char* kShadowInstancedVS = R"(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=4) in mat4 aModel; // per-instance
uniform mat4 uLightVP;
void main(){ gl_Position = uLightVP * aModel * vec4(aPos,1.0); }
)";

// Skinned Lit VS — скелетная анимация + motion vectors (до 128 костей)
inline const char* kSkinnedLitVS = R"(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in ivec4 aBoneIds;
layout(location=4) in vec4 aWeights;
out vec3 vColor; out vec2 vUV; out vec3 vNormal; out vec3 vWorldPos;
out vec3 vTangent; out vec3 vBitangent;
out vec4 vCurrClip; out vec4 vPrevClip;
uniform mat4 uMVP; uniform mat4 uModel; uniform mat3 uNormalMat;
uniform mat4 uPrevModel; uniform mat4 uPrevViewProj;
const int MAX_BONES = 128;
uniform mat4 uBones[MAX_BONES];
void main(){
    vColor = vec3(1.0);
    vUV = aUV;
    mat4 skinMat = aWeights.x * uBones[aBoneIds.x] +
                   aWeights.y * uBones[aBoneIds.y] +
                   aWeights.z * uBones[aBoneIds.z] +
                   aWeights.w * uBones[aBoneIds.w];
    vec4 localPos = skinMat * vec4(aPos, 1.0);
    vec4 wp = uModel * localPos;
    vWorldPos = wp.xyz;
    vec3 localNorm = mat3(skinMat) * aNormal;
    vec3 N = normalize(uNormalMat * localNorm);
    vNormal = N;
    vec3 T = normalize(uNormalMat * vec3(1.0, 0.0, 0.0));
    T = normalize(T - dot(T, N) * N);
    vTangent = T;
    vBitangent = cross(N, T);
    vCurrClip = uMVP * localPos;
    vec4 prevWp = uPrevModel * localPos;
    vPrevClip = uPrevViewProj * prevWp;
    gl_Position = vCurrClip;
}
)";

// Skinned Shadow VS — глубина для скелетных моделей
inline const char* kSkinnedShadowVS = R"(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=3) in ivec4 aBoneIds;
layout(location=4) in vec4 aWeights;
uniform mat4 uLightMatrix; uniform mat4 uModel;
const int MAX_BONES = 128;
uniform mat4 uBones[MAX_BONES];
void main(){
    mat4 skinMat = aWeights.x * uBones[aBoneIds.x] +
                   aWeights.y * uBones[aBoneIds.y] +
                   aWeights.z * uBones[aBoneIds.z] +
                   aWeights.w * uBones[aBoneIds.w];
    gl_Position = uLightMatrix * uModel * (skinMat * vec4(aPos, 1.0));
}
)";

// Decal Shader (Projected OBB / Surface Decals)
inline const char* kDecalVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec4 vClipPos;
out vec3 vLocalPos;
void main(){
    vLocalPos = aPos;
    vClipPos = uMVP * vec4(aPos, 1.0);
    gl_Position = vClipPos;
}
)";

inline const char* kDecalFS = R"(#version 460 core
in vec4 vClipPos;
in vec3 vLocalPos;
out vec4 FragColor;

uniform sampler2D uDecalTexture;
uniform vec4 uDecalColor;
uniform bool uIsProjected;
uniform sampler2D uDepthTexture;
uniform mat4 uInvViewProj;
uniform mat4 uInvModel;

void main(){
    if (!uIsProjected) {
        vec2 uv = vLocalPos.xy + 0.5;
        vec4 col = texture(uDecalTexture, uv) * uDecalColor;
        if (col.a < 0.02) discard;
        FragColor = col;
        return;
    }

    vec2 screenUV = (vClipPos.xy / vClipPos.w) * 0.5 + 0.5;
    float depth = texture(uDepthTexture, screenUV).r;
    if (depth >= 1.0) discard;

    vec4 ndc = vec4(screenUV * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = uInvViewProj * ndc;
    worldPos /= worldPos.w;

    vec4 localPos = uInvModel * worldPos;
    if (abs(localPos.x) > 0.5 || abs(localPos.y) > 0.5 || abs(localPos.z) > 0.5) {
        discard;
    }

    vec2 decalUV = localPos.xz + 0.5;
    vec4 col = texture(uDecalTexture, decalUV) * uDecalColor;
    float edgeFade = clamp((0.5 - abs(localPos.y)) * 4.0, 0.0, 1.0);
    col.a *= edgeFade;

    if (col.a < 0.02) discard;
    FragColor = col;
}
)";

// Temporal Anti-Aliasing (TAA) with Velocity Buffer Reprojection
inline const char* kTAAFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uCurrentColor;
uniform sampler2D uHistoryColor;
uniform sampler2D uDepth;
uniform mat4 uInvViewProj;
uniform mat4 uPrevViewProj;
uniform vec2 uTexelSize;
uniform float uFeedbackFactor;

vec3 RGBToYCoCg(vec3 c) {
    return vec3(
         0.25 * c.r + 0.5 * c.g + 0.25 * c.b,
         0.5  * c.r             - 0.5  * c.b,
        -0.25 * c.r + 0.5 * c.g - 0.25 * c.b
    );
}

vec3 YCoCgToRGB(vec3 c) {
    return vec3(
        c.x + c.y - c.z,
        c.x + c.z,
        c.x - c.y - c.z
    );
}

void main() {
    vec4 current = texture(uCurrentColor, vUV);

    // 3x3 Neighborhood bounding box in YCoCg space
    vec3 minColor = RGBToYCoCg(current.rgb);
    vec3 maxColor = minColor;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec3 neighbor = RGBToYCoCg(texture(uCurrentColor, vUV + vec2(x, y) * uTexelSize).rgb);
            minColor = min(minColor, neighbor);
            maxColor = max(maxColor, neighbor);
        }
    }

    float depth = texture(uDepth, vUV).r;
    vec4 clipPos = vec4(vUV * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = uInvViewProj * clipPos;
    worldPos /= max(worldPos.w, 1e-5);

    vec4 prevClipPos = uPrevViewProj * worldPos;
    vec2 prevUV = (prevClipPos.xy / max(prevClipPos.w, 1e-5)) * 0.5 + 0.5;

    // Discard history outside screen boundaries
    if (prevUV.x < 0.001 || prevUV.x > 0.999 || prevUV.y < 0.001 || prevUV.y > 0.999) {
        FragColor = current;
        return;
    }

    vec4 history = texture(uHistoryColor, prevUV);
    vec3 historyYCoCg = RGBToYCoCg(history.rgb);

    // Strict clamping to current 3x3 bounding box completely prevents ghosting/smear
    historyYCoCg = clamp(historyYCoCg, minColor, maxColor);
    vec3 clampedHistory = YCoCgToRGB(historyYCoCg);

    // Motion-based history rejection: fast camera movement drops history to 0 instantly
    float motion = length(prevUV - vUV);
    float motionFade = clamp(motion * 60.0, 0.0, 1.0);
    float blend = mix(0.85, 0.0, motionFade);

    vec3 finalColor = mix(current.rgb, clampedHistory, blend);

    FragColor = vec4(finalColor, 1.0);
}
)";

// Screen-Space Reflections (SSR) Ray Marching
inline const char* kSSRFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uColorTex;
uniform sampler2D uDepthTex;
uniform sampler2D uNormalRoughnessTex;
uniform mat4 uProj;
uniform mat4 uInvProj;
uniform vec2 uScreenSize;
uniform float uMaxDistance;
uniform float uThickness;
uniform int uMaxSteps;

vec3 getPosition(vec2 uv) {
    float depth = texture(uDepthTex, uv).r;
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInvProj * clip;
    return viewPos.xyz / max(viewPos.w, 1e-5);
}

void main() {
    float depth = texture(uDepthTex, vUV).r;
    if (depth >= 0.9999 || depth <= 0.0001) {
        FragColor = vec4(0.0);
        return;
    }

    vec4 normRough = texture(uNormalRoughnessTex, vUV);
    float roughness = normRough.a;
    if (roughness > 0.65) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 normal = normalize(normRough.xyz);
    if (length(normal) < 0.1) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 viewPos = getPosition(vUV);
    vec3 viewDir = normalize(viewPos);
    vec3 reflDir = reflect(viewDir, normal);
    
    if (reflDir.z > 0.0) {
        FragColor = vec4(0.0);
        return;
    }

    float stepSize = 0.22;
    vec3 rayPos = viewPos + reflDir * 0.12;
    vec3 rayStep = reflDir * stepSize;
    
    vec2 hitUV = vec2(0.0);
    float hitConfidence = 0.0;
    
    for (int i = 0; i < uMaxSteps; ++i) {
        rayPos += rayStep;
        
        vec4 proj = uProj * vec4(rayPos, 1.0);
        vec2 sampleUV = (proj.xy / max(proj.w, 1e-5)) * 0.5 + 0.5;
        
        if (sampleUV.x < 0.01 || sampleUV.x > 0.99 || sampleUV.y < 0.01 || sampleUV.y > 0.99) break;
        
        float sampleDepth = texture(uDepthTex, sampleUV).r;
        vec4 sampleClip = vec4(sampleUV * 2.0 - 1.0, sampleDepth * 2.0 - 1.0, 1.0);
        vec4 sampleViewPos = uInvProj * sampleClip;
        sampleViewPos /= max(sampleViewPos.w, 1e-5);
        
        float depthDiff = rayPos.z - sampleViewPos.z;
        if (depthDiff <= 0.0 && depthDiff > -uThickness) {
            // Binary search refinement
            vec3 refinePos = rayPos - rayStep;
            vec3 halfStep = rayStep * 0.5;
            for (int j = 0; j < 5; ++j) {
                refinePos += halfStep;
                vec4 rProj = uProj * vec4(refinePos, 1.0);
                vec2 rUV = (rProj.xy / max(rProj.w, 1e-5)) * 0.5 + 0.5;
                float rDepth = texture(uDepthTex, rUV).r;
                vec4 rClip = vec4(rUV * 2.0 - 1.0, rDepth * 2.0 - 1.0, 1.0);
                vec4 rView = uInvProj * rClip; rView /= max(rView.w, 1e-5);
                if (refinePos.z - rView.z <= 0.0) {
                    refinePos -= halfStep;
                }
                halfStep *= 0.5;
            }
            vec4 finalProj = uProj * vec4(refinePos, 1.0);
            hitUV = (finalProj.xy / max(finalProj.w, 1e-5)) * 0.5 + 0.5;
            
            // Screen edge vignette fade
            vec2 edge = smoothstep(0.0, 0.15, hitUV) * smoothstep(1.0, 0.85, hitUV);
            float edgeFade = edge.x * edge.y;
            
            // Fresnel factor (more reflective at grazing angles)
            float NoV = max(dot(-viewDir, normal), 0.0);
            float fresnel = pow(1.0 - NoV, 3.0) * 0.85 + 0.15;
            
            // Glossiness weight: smooth surfaces have sharp, clear reflections
            float gloss = (1.0 - roughness * 1.3);
            
            hitConfidence = edgeFade * fresnel * clamp(gloss, 0.0, 1.0);
            break;
        }
        stepSize *= 1.03;
        rayStep = reflDir * stepSize;
    }
    
    if (hitConfidence > 0.01) {
        vec3 reflColor = texture(uColorTex, hitUV).rgb;
        FragColor = vec4(reflColor, hitConfidence);
    } else {
        FragColor = vec4(0.0);
    }
}
)";

// SSR Composite blend
inline const char* kSSRCompositeFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSceneColor;
uniform sampler2D uSSRColor;
uniform float uSSRIntensity;

void main() {
    vec3 scene = texture(uSceneColor, vUV).rgb;
    vec4 ssr = texture(uSSRColor, vUV);
    vec3 result = scene + ssr.rgb * ssr.a * uSSRIntensity;
    FragColor = vec4(result, 1.0);
}
)";

// ---- GTAO (Ground Truth Ambient Occlusion) — half-res ----
inline const char* kGTAOFS = R"(
#version 460 core
in vec2 vUV;
out float FragColor;

uniform sampler2D uDepthTex;
uniform mat4 uProj;
uniform mat4 uView;
uniform vec2 uScreenSize;
uniform float uRadius;
uniform float uIntensity;
uniform int uDirections;
uniform int uSteps;

const float PI = 3.14159265;

vec3 reconstructViewPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view = uProj * clip;
    return view.xyz / view.w;
}

vec3 reconstructNormal(vec2 uv) {
    vec2 texel = 1.0 / uScreenSize;
    float dC = texture(uDepthTex, uv).r;
    vec3 P = reconstructViewPos(uv, dC);
    float dR = texture(uDepthTex, uv + vec2(texel.x, 0.0)).r;
    float dL = texture(uDepthTex, uv - vec2(texel.x, 0.0)).r;
    float dU = texture(uDepthTex, uv + vec2(0.0, texel.y)).r;
    float dD = texture(uDepthTex, uv - vec2(0.0, texel.y)).r;
    vec3 Pr = reconstructViewPos(uv + vec2(texel.x, 0.0), dR);
    vec3 Pl = reconstructViewPos(uv - vec2(texel.x, 0.0), dL);
    vec3 Pu = reconstructViewPos(uv + vec2(0.0, texel.y), dU);
    vec3 Pd = reconstructViewPos(uv - vec2(0.0, texel.y), dD);
    vec3 dx = (dR > 0.001 && dL > 0.001) ? Pr - Pl : vec3(1.0, 0.0, 0.0);
    vec3 dy = (dU > 0.001 && dD > 0.001) ? Pu - Pd : vec3(0.0, 1.0, 0.0);
    return normalize(cross(dx, dy));
}

float interleavedGradientNoise(vec2 pixel) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(pixel, magic.xy)));
}

void main() {
    vec2 halfUV = vUV;
    float depth = texture(uDepthTex, halfUV).r;
    if (depth < 0.0001) { FragColor = 1.0; return; }

    vec3 P = reconstructViewPos(halfUV, depth);
    vec3 N = reconstructNormal(halfUV);

    float occlusion = 0.0;
    float randomAngle = interleavedGradientNoise(gl_FragCoord.xy) * 2.0 * PI;

    for (int i = 0; i < uDirections; ++i) {
        float angle = (float(i) / float(uDirections)) * PI + randomAngle;
        vec2 direction = vec2(cos(angle), sin(angle));
        float occluded = 0.0;
        float total = 0.0;

        for (int j = 1; j <= uSteps; ++j) {
            float t = float(j) / float(uSteps);
            vec2 sampleUV = halfUV + direction * t * uRadius / uScreenSize;
            float sampleDepth = texture(uDepthTex, sampleUV).r;
            vec3 S = reconstructViewPos(sampleUV, sampleDepth);
            vec3 horizonVec = S - P;
            float dist = length(horizonVec);
            if (dist < 0.001) continue;
            float cosTheta = dot(normalize(horizonVec), N);
            float weight = 1.0 - smoothstep(0.0, uRadius, dist);
            if (cosTheta > 0.0) occluded += cosTheta * weight;
            total += weight;
        }
        if (total > 0.0) occlusion += occluded / total;
    }

    occlusion /= float(uDirections);
    occlusion = 1.0 - occlusion * uIntensity;
    FragColor = clamp(occlusion, 0.0, 1.0);
}
)";

inline const char* kGTAOBlurFS = R"(
#version 460 core
in vec2 vUV;
out float FragColor;

uniform sampler2D uAOTex;
uniform sampler2D uDepthTex;
uniform vec2 uScreenSize;

void main() {
    vec2 texel = 1.0 / uScreenSize;
    float centerAO = texture(uAOTex, vUV).r;
    float centerDepth = texture(uDepthTex, vUV).r;
    float totalAO = 0.0;
    float totalWeight = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 off = vec2(float(x), float(y)) * texel;
            float ao = texture(uAOTex, vUV + off).r;
            float d = texture(uDepthTex, vUV + off).r;
            float depthDiff = abs(d - centerDepth);
            float depthWeight = exp(-depthDiff * 1000.0);
            float spatialWeight = exp(-0.5 * float(x*x + y*y));
            float w = depthWeight * spatialWeight;
            totalAO += ao * w;
            totalWeight += w;
        }
    }
    FragColor = (totalWeight > 0.0) ? totalAO / totalWeight : centerAO;
}
)";

inline const char* kAOMultiplyFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uHDR;
uniform sampler2D uAO;
void main() {
    vec3 hdr = texture(uHDR, vUV).rgb;
    float ao = texture(uAO, vUV).r;
    FragColor = vec4(hdr * ao, 1.0);
}
)";

// ---- Volumetric Fog — screen-space ray march ----
inline const char* kVolumetricFogFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uDepthTex;
uniform sampler2D uSceneTex;
uniform mat4 uInvProj;
uniform mat4 uInvView;
uniform mat4 uView;
uniform vec3 uCameraPos;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogHeight;
uniform float uFogStart;
uniform float uFogEnd;
uniform int uStepCount;
uniform float uTime;

// Height-based fog noise
float hash(vec3 p) {
    p = fract(p * vec3(443.8975, 397.2973, 491.1871));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

float noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i + vec3(0,0,0)), hash(i + vec3(1,0,0)), f.x),
                   mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
               mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
                   mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y), f.z);
}

float fbm(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; ++i) {
        v += a * noise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view = uInvProj * clip;
    view.xyz /= view.w;
    vec4 world = uInvView * vec4(view.xyz, 1.0);
    return world.xyz;
}

void main() {
    float depth = texture(uDepthTex, vUV).r;
    vec3 sceneColor = texture(uSceneTex, vUV).rgb;

    if (depth < 0.0001) {
        FragColor = vec4(sceneColor, 1.0);
        return;
    }

    vec3 worldPos = reconstructWorldPos(vUV, depth);
    vec3 viewDir = normalize(worldPos - uCameraPos);
    float distToObj = length(worldPos - uCameraPos);

    float fogStart = uFogStart;
    float fogEnd = min(uFogEnd, distToObj);

    if (fogEnd <= fogStart) {
        FragColor = vec4(sceneColor, 1.0);
        return;
    }

    float totalDensity = 0.0;
    vec3 totalLight = vec3(0.0);
    int steps = uStepCount;
    float stepSize = (fogEnd - fogStart) / float(steps);

    for (int i = 0; i < steps; ++i) {
        float t = fogStart + (float(i) + 0.5) * stepSize;
        vec3 samplePos = uCameraPos + viewDir * t;

        // Height-based density falloff
        float heightFactor = exp(-max(samplePos.y - uFogHeight, 0.0) * uFogHeightFalloff);

        // Animated noise for fog variation
        vec3 noisePos = samplePos * 0.05 + vec3(uTime * 0.1, 0.0, uTime * 0.05);
        float fogNoise = fbm(noisePos) * 0.6 + 0.4;

        float density = uFogDensity * heightFactor * fogNoise * stepSize;
        totalDensity += density;
    }

    totalDensity = min(totalDensity, 1.0);

    // Fog contribution with height-based tint
    vec3 fogContrib = uFogColor * (1.0 - exp(-totalDensity));

    vec3 result = mix(sceneColor, uFogColor, totalDensity);
    FragColor = vec4(result, 1.0);
}
)";

// Volumetric fog composite — upsample + apply
inline const char* kFogCompositeFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uSceneTex;
uniform sampler2D uFogTex;
void main() {
    vec3 scene = texture(uSceneTex, vUV).rgb;
    vec4 fog = texture(uFogTex, vUV);
    FragColor = vec4(fog.rgb, 1.0);
}
)";

// ---- Light Shafts (God Rays) — screen-space radial blur ----
inline const char* kLightShaftsFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSceneTex;
uniform vec2 uSunScreenPos; // sun position in screen space [0,1]
uniform float uDensity;
uniform float uWeight;
uniform float uDecay;
uniform float uExposure;
uniform int uSamples;

void main() {
    vec2 texCoord = vUV;
    vec2 deltaTexCoord = (texCoord - uSunScreenPos) * uDensity / float(uSamples);
    vec3 color = texture(uSceneTex, texCoord).rgb;

    vec3 illumination = vec3(0.0);
    float illuminationDecay = 1.0;

    vec2 sampleCoord = texCoord;
    for (int i = 0; i < uSamples; ++i) {
        sampleCoord -= deltaTexCoord;
        vec3 sampleColor = texture(uSceneTex, sampleCoord).rgb;
        sampleColor *= illuminationDecay * uWeight;
        illumination += sampleColor;
        illuminationDecay *= uDecay;
    }

    vec3 godRays = illumination * uExposure;
    FragColor = vec4(color + godRays, 1.0);
}
)";

} // namespace DefaultShaders

// Плоский цвет без света (debug-визуализация LOD кластеров)
inline const char* kFlatVS = R"(
#version 460 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main(){ gl_Position = uMVP * vec4(aPos,1.0); }
)";
inline const char* kFlatFS = R"(
#version 460 core
uniform vec3 uColor;
out vec4 FragColor;
void main(){ FragColor = vec4(uColor,1.0); }
)";

