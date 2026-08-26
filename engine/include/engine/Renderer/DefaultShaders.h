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
out vec3 vColor; out vec2 vUV; out vec3 vNormal; out vec3 vWorldPos;
uniform mat4 uMVP; uniform mat4 uModel; uniform mat3 uNormalMat;
void main(){
    vColor=aColor; vUV=aUV;
    vec4 wp=uModel*vec4(aPos,1.0); vWorldPos=wp.xyz;
    vNormal=normalize(uNormalMat*aNormal);
    gl_Position=uMVP*vec4(aPos,1.0);
}
)";
inline const char* kLitFS = R"(#version 460 core
in vec3 vColor; in vec2 vUV; in vec3 vNormal; in vec3 vWorldPos;
out vec4 FragColor;
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
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-V, N);
    float NoV = max(dot(N, V), 1e-4);

    vec3 albedo = uAlbedo * vColor;
    if(uUseDiffuseMap) albedo *= texture(uDiffuseMap,vUV).rgb;
    float metallic = clamp(uMetallic,0.0,1.0);
    float rough = clamp(uRoughness,0.04,1.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

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
    
    // Cluster index calculation
    vec4 viewPos = uView * vec4(vWorldPos, 1.0);
    if(viewPos.z < 0.0) {
        float slice = log(-viewPos.z / uZNear) * float(GRID_SIZE.z) / log(uZFar / uZNear);
        uint zSlice = uint(clamp(int(slice), 0, int(GRID_SIZE.z) - 1));
        uint xTile = uint(gl_FragCoord.x * float(GRID_SIZE.x) / uScreenSize.x);
        uint yTile = uint(gl_FragCoord.y * float(GRID_SIZE.y) / uScreenSize.y);
        
        if (xTile < GRID_SIZE.x && yTile < GRID_SIZE.y) {
            uint clusterIndex = xTile + yTile * GRID_SIZE.x + zSlice * (GRID_SIZE.x * GRID_SIZE.y);
            uint lCount = clusters[clusterIndex].lightCount;
            
            for(uint i=0; i < lCount; ++i){
                uint li = clusters[clusterIndex].lightIndices[i];
                PointLight pl = pointLights[li];
                
                vec3 lpos = pl.posRad.xyz;
                float lrange = pl.posRad.w;
                vec3 lcol = pl.colInt.xyz;
                float lintensity = pl.colInt.w;
                
                vec3 toL = lpos - vWorldPos;
                float dist = length(toL);
                if(dist > lrange) continue;
                vec3 L = toL/dist;
                
                // standard quadratic falloff: 1 / (1 + (d/r)^2 * falloff_params)
                // but simpler for RT: atten = max(0, 1 - (dist/range)^4)^2 / (dist^2 + 1)
                float atten = 1.0 / (1.0 + 0.14*dist + 0.07*dist*dist);
                float rangeFade = 1.0 - clamp((dist - lrange*0.7)/(lrange*0.3), 0.0, 1.0);
                Lo += ApplyLight(N,V,L,lcol*lintensity*atten*rangeFade,F0,albedo,rough,metallic);
            }
        }
    }

    vec3 color = ambient + Lo + uEmissive;
    // fog
    float fogF = exp(-uFogDensity*length(vWorldPos-uViewPos));
    color = mix(uFogColor,color,clamp(fogF,0.0,1.0));
    // линейный HDR out — exposure+ACES+gamma делает composite для всего кадра
    FragColor = vec4(color,1.0);
}
)";

// Lit Instanced — для батчинга (model per instance)
inline const char* kLitInstancedVS = R"(#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
layout(location=4) in mat4 aInstanceModel; // 4,5,6,7
layout(location=8) in vec4 aInstanceAlbedo; // rgb=albedo, a=shininess
layout(location=9) in vec4 aInstanceEmissive;
out vec3 vColor; out vec2 vUV; out vec3 vNormal; out vec3 vWorldPos;
out vec4 vInstAlbedo; out vec4 vInstEmissive;
uniform mat4 uView; uniform mat4 uProj;
void main(){
    vColor=aColor; vUV=aUV;
    vInstAlbedo=aInstanceAlbedo; vInstEmissive=aInstanceEmissive;
    vec4 wp=aInstanceModel*vec4(aPos,1.0); vWorldPos=wp.xyz;
    mat3 nm=transpose(inverse(mat3(aInstanceModel)));
    vNormal=normalize(nm*aNormal);
    gl_Position=uProj*uView*wp;
}
)";
inline const char* kLitInstancedFS = R"(#version 460 core
in vec3 vColor; in vec2 vUV; in vec3 vNormal; in vec3 vWorldPos;
in vec4 vInstAlbedo; in vec4 vInstEmissive;
out vec4 FragColor;
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
    vec3 N=normalize(vNormal); vec3 V=normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-V, N);
    float NoV = max(dot(N, V), 1e-4);

    vec3 albedo=vInstAlbedo.rgb * vColor;
    float rough = clamp(vInstAlbedo.a, 0.04, 1.0);
    float metallic = clamp(vInstEmissive.a, 0.0, 1.0);
    if(uUseDiffuseMap) albedo*=texture(uDiffuseMap,vUV).rgb;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // PBR Environment IBL (Diffuse + Specular)
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
    // Cluster index calculation
    vec4 viewPos = uView * vec4(vWorldPos, 1.0);
    if(viewPos.z < 0.0) {
        float slice = log(-viewPos.z / uZNear) * float(GRID_SIZE.z) / log(uZFar / uZNear);
        uint zSlice = uint(clamp(int(slice), 0, int(GRID_SIZE.z) - 1));
        uint xTile = uint(gl_FragCoord.x * float(GRID_SIZE.x) / uScreenSize.x);
        uint yTile = uint(gl_FragCoord.y * float(GRID_SIZE.y) / uScreenSize.y);
        
        if (xTile < GRID_SIZE.x && yTile < GRID_SIZE.y) {
            uint clusterIndex = xTile + yTile * GRID_SIZE.x + zSlice * (GRID_SIZE.x * GRID_SIZE.y);
            uint lCount = clusters[clusterIndex].lightCount;
            
            for(uint i=0; i < lCount; ++i){
                uint li = clusters[clusterIndex].lightIndices[i];
                PointLight pl = pointLights[li];
                
                vec3 lpos = pl.posRad.xyz;
                float lrange = pl.posRad.w;
                vec3 lcol = pl.colInt.xyz;
                float lintensity = pl.colInt.w;
                
                vec3 toL = lpos - vWorldPos;
                float dist = length(toL);
                if(dist > lrange) continue;
                vec3 L = toL/dist;
                
                float atten = 1.0 / (1.0 + 0.14*dist + 0.07*dist*dist);
                float rangeFade = 1.0 - clamp((dist - lrange*0.7)/(lrange*0.3), 0.0, 1.0);
                Lo += ApplyLight(N,V,L,lcol*lintensity*atten*rangeFade,F0,albedo,rough,metallic);
            }
        }
    }

    vec3 color = ambient + Lo + vInstEmissive.rgb;
    float fogF=exp(-uFogDensity*length(vWorldPos-uViewPos));
    color=mix(uFogColor,color,clamp(fogF,0.0,1.0));
    FragColor=vec4(color,1.0); // линейный HDR — тонемап в composite
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
in vec3 vDir; out vec4 FragColor;
uniform vec3 uTopColor; uniform vec3 uHorizonColor; uniform vec3 uBottomColor;
uniform float uTime;
void main(){
    vec3 dir=normalize(vDir);
    float t=clamp(dir.y*0.5+0.5, 0.0,1.0);
    // three-stop gradient
    vec3 col=mix(uBottomColor, uHorizonColor, smoothstep(0.0,0.45,t));
    col=mix(col, uTopColor, smoothstep(0.45,1.0,t));
    // sun disc
    vec3 sunDir=normalize(vec3(0.4,0.6,0.2));
    float sun=pow(max(dot(dir,sunDir),0.0), 128.0)*0.9;
    col+=sun*vec3(1.0,0.95,0.7);
    // subtle stars via noise
    float n=fract(sin(dot(dir.xz, vec2(12.9898,78.233)))*43758.5);
    float stars=smoothstep(0.985,1.0,n)*pow(max(dir.y,0.0),2.0)*0.4;
    col+=stars;
    FragColor=vec4(col,1.0);
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
uniform mat4 uView; uniform mat4 uProj;
void main(){
    vColor=aColor; vUV=aUV;
    vec4 wp = aModel * vec4(aPos,1.0);
    vWorldPos = wp.xyz;
    vNormal = normalize(transpose(inverse(mat3(aModel))) * aNormal);
    gl_Position = uProj * uView * wp;
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

// Skinned Lit VS — скелетная анимация (до 128 костей)
inline const char* kSkinnedLitVS = R"(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in ivec4 aBoneIds;
layout(location=4) in vec4 aWeights;
out vec3 vColor; out vec2 vUV; out vec3 vNormal; out vec3 vWorldPos;
uniform mat4 uMVP; uniform mat4 uModel; uniform mat3 uNormalMat;
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
    vNormal = normalize(uNormalMat * localNorm);
    gl_Position = uMVP * localPos;
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

// Temporal Anti-Aliasing (TAA) with YCoCg Neighborhood Color Clamping
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
    worldPos /= worldPos.w;

    vec4 prevClipPos = uPrevViewProj * worldPos;
    prevClipPos /= prevClipPos.w;
    vec2 prevUV = prevClipPos.xy * 0.5 + 0.5;

    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        FragColor = current;
        return;
    }

    vec4 history = texture(uHistoryColor, prevUV);
    vec3 historyYCoCg = RGBToYCoCg(history.rgb);

    historyYCoCg = clamp(historyYCoCg, minColor, maxColor);
    vec3 clampedHistory = YCoCgToRGB(historyYCoCg);

    float blend = clamp(uFeedbackFactor, 0.82, 0.95);
    vec3 finalColor = mix(current.rgb, clampedHistory, blend);

    FragColor = vec4(finalColor, 1.0);
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

