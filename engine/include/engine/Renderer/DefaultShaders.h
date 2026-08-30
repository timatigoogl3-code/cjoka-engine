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
uniform vec2 uUVTiling;
void main(){
    vec2 tiling = (uUVTiling.x > 0.0 && uUVTiling.y > 0.0) ? uUVTiling : vec2(1.0);
    vColor=aColor; vUV=aUV * tiling;
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
uniform vec3 uAlbedo; uniform float uMetallic; uniform float uRoughness; uniform float uAO; uniform float uWetness;
uniform vec3 uEmissive;
uniform sampler2D uDiffuseMap; uniform bool uUseDiffuseMap;
uniform sampler2D uNormalMap; uniform bool uUseNormalMap; uniform float uNormalScale;
uniform sampler2D uSpecularMap; uniform bool uUseSpecularMap;
// cascaded shadows
uniform mat4 uLightMatrices[3];
uniform float uCascadeSplits[4];
uniform sampler2DArray uShadowMapArray;
uniform bool uHasShadow;
uniform vec3 uFogColor; uniform float uFogDensity; uniform float uExposure;
// sky environment IBL
uniform vec3 uSkyTop; uniform vec3 uSkyHorizon; uniform vec3 uSkyBottom; uniform float uSkyExposure;

// VXGI
uniform sampler3D uVoxelGrid;
uniform bool uUseVXGI;
uniform vec3 uVoxelCenter;
uniform float uVoxelExtent;
uniform float uVXGIIntensity;

vec3 WorldToVoxel(vec3 p) {
    return (p - (uVoxelCenter - vec3(uVoxelExtent * 0.5))) / uVoxelExtent;
}

vec4 TraceCone(vec3 origin, vec3 dir, float aperture, float maxDist) {
    vec4 accum = vec4(0.0);
    float dist = 0.4;
    float voxelSize = uVoxelExtent / 64.0;

    while (dist < maxDist && accum.a < 0.95) {
        vec3 p = origin + dir * dist;
        vec3 uvw = WorldToVoxel(p);
        if (any(lessThan(uvw, vec3(0.0))) || any(greaterThan(uvw, vec3(1.0)))) break;

        float diameter = 2.0 * aperture * dist;
        float mipLevel = clamp(log2(max(diameter / voxelSize, 1.0)), 0.0, 5.0);

        vec4 voxelSample = textureLod(uVoxelGrid, uvw, mipLevel);
        float a = 1.0 - accum.a;
        accum.rgb += voxelSample.rgb * voxelSample.a * a;
        accum.a += voxelSample.a * a;

        dist += max(diameter * 0.5, voxelSize);
    }
    return accum;
}

vec3 CalculateVXGI(vec3 worldPos, vec3 N, vec3 V, float roughness, vec3 albedo) {
    if (!uUseVXGI) return vec3(0.0);

    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 coneDirs[6] = vec3[](
        N,
        normalize(N + tangent * 0.5),
        normalize(N - tangent * 0.5),
        normalize(N + bitangent * 0.5),
        normalize(N - bitangent * 0.5),
        normalize(N + (tangent + bitangent) * 0.35)
    );
    float coneWeights[6] = float[](0.35, 0.13, 0.13, 0.13, 0.13, 0.13);

    vec3 indirectDiffuse = vec3(0.0);
    float diffuseAperture = 0.577; // tan(30 deg)
    for (int i = 0; i < 6; ++i) {
        vec4 c = TraceCone(worldPos + N * 0.15, coneDirs[i], diffuseAperture, uVoxelExtent * 0.6);
        indirectDiffuse += c.rgb * coneWeights[i];
    }

    return (indirectDiffuse * albedo) * uVXGIIntensity;
}

// Light Probes SSBO (Baked Mixed Lighting)
struct GPULightProbe {
    vec4 pos;
    vec4 sh[9];
};

layout(std430, binding = 6) readonly buffer LightProbeBlock {
    vec4 uProbeMin;
    vec4 uProbeMax;
    ivec4 uProbeCounts;
    GPULightProbe uLightProbes[];
};

uniform bool uUseLightProbes;
uniform float uLightProbeIntensity;

vec3 SampleProbeIrradiance(vec3 worldPos, vec3 N) {
    if (!uUseLightProbes || uProbeCounts.w <= 0) return vec3(0.0);

    vec3 minB = uProbeMin.xyz;
    vec3 maxB = uProbeMax.xyz;
    ivec3 counts = uProbeCounts.xyz;

    vec3 local = (worldPos - minB) / (maxB - minB);
    if (any(lessThan(local, vec3(0.0))) || any(greaterThan(local, vec3(1.0)))) return vec3(0.0);

    vec3 fIndex = local * vec3(counts - ivec3(1));
    ivec3 i0 = clamp(ivec3(fIndex), ivec3(0), counts - ivec3(2));
    ivec3 i1 = i0 + ivec3(1);
    vec3 t = fract(fIndex);

    int idx000 = i0.z * counts.x * counts.y + i0.y * counts.x + i0.x;
    int idx100 = i0.z * counts.x * counts.y + i0.y * counts.x + i1.x;
    int idx010 = i0.z * counts.x * counts.y + i1.y * counts.x + i0.x;
    int idx110 = i0.z * counts.x * counts.y + i1.y * counts.x + i1.x;
    int idx001 = i1.z * counts.x * counts.y + i0.y * counts.x + i0.x;
    int idx101 = i1.z * counts.x * counts.y + i0.y * counts.x + i1.x;
    int idx011 = i1.z * counts.x * counts.y + i1.y * counts.x + i0.x;
    int idx111 = i1.z * counts.x * counts.y + i1.y * counts.x + i1.x;

    vec3 sh00 = mix(uLightProbes[idx000].sh[0].rgb, uLightProbes[idx100].sh[0].rgb, t.x);
    vec3 sh01 = mix(uLightProbes[idx010].sh[0].rgb, uLightProbes[idx110].sh[0].rgb, t.x);
    vec3 shY0 = mix(sh00, sh01, t.y);

    vec3 sh10 = mix(uLightProbes[idx001].sh[0].rgb, uLightProbes[idx101].sh[0].rgb, t.x);
    vec3 sh11 = mix(uLightProbes[idx011].sh[0].rgb, uLightProbes[idx111].sh[0].rgb, t.x);
    vec3 shY1 = mix(sh10, sh11, t.y);

    vec3 l0 = mix(shY0, shY1, t.z);
    return l0 * (0.5 + 0.5 * N.y) * uLightProbeIntensity;
}

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
float sampleCascade(int cascadeIndex, vec3 worldPos, vec3 N, vec3 L) {
    vec4 lp = uLightMatrices[cascadeIndex] * vec4(worldPos, 1.0);
    vec3 proj = lp.xyz / lp.w * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        if (cascadeIndex < 2) {
            vec4 lpNext = uLightMatrices[cascadeIndex + 1] * vec4(worldPos, 1.0);
            vec3 projNext = lpNext.xyz / lpNext.w * 0.5 + 0.5;
            if (projNext.z <= 1.0 && projNext.x >= 0.0 && projNext.x <= 1.0 && projNext.y >= 0.0 && projNext.y <= 1.0) {
                float biasN = max(0.0025 * (1.0 - dot(N, L)), 0.0008) * (1.0 + float(cascadeIndex + 1) * 1.5);
                float d = texture(uShadowMapArray, vec3(projNext.xy, float(cascadeIndex + 1))).r;
                return (projNext.z - biasN > d) ? 0.0 : 1.0;
            }
        }
        return 1.0;
    }

    float bias = max(0.0025 * (1.0 - dot(N, L)), 0.0008) * (1.0 + float(cascadeIndex) * 1.5);
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

float shadowFactor(vec3 worldPos, vec3 N, vec3 L, float viewDist){
    if(!uHasShadow) return 1.0;
    
    int cascadeIndex = 2;
    float blend = 0.0;
    int nextCascade = 2;

    if (viewDist < uCascadeSplits[1]) {
        cascadeIndex = 0;
        float d = uCascadeSplits[1] - viewDist;
        if (d < 3.0) { blend = 1.0 - (d / 3.0); nextCascade = 1; }
    } else if (viewDist < uCascadeSplits[2]) {
        cascadeIndex = 1;
        float d = uCascadeSplits[2] - viewDist;
        if (d < 5.0) { blend = 1.0 - (d / 5.0); nextCascade = 2; }
    }

    float s = sampleCascade(cascadeIndex, worldPos, N, L);
    if (blend > 0.0) {
        float sNext = sampleCascade(nextCascade, worldPos, N, L);
        s = mix(s, sNext, blend);
    }
    return s;
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
    float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyColor = mix(uSkyBottom, uSkyTop, t);
    
    // Horizon glow & atmospheric scattering band
    float horizonFactor = pow(clamp(1.0 - abs(dir.y), 0.0, 1.0), 3.0);
    skyColor = mix(skyColor, uSkyHorizon * 1.6, horizonFactor);

    // Sun / Moon specular highlight in sky reflection
    if (uHasDirLight) {
        vec3 sunDir = normalize(-uDirLight.direction);
        float sunDot = max(dot(dir, sunDir), 0.0);
        float sunExp = mix(256.0, 4.0, clamp(rough, 0.0, 1.0));
        vec3 sunSpecular = uDirLight.color * pow(sunDot, sunExp) * (1.0 - rough * 0.8) * 3.0;
        skyColor += sunSpecular;
    }

    // Roughness blur towards ambient hemispherical mean
    vec3 ambientMean = mix(uSkyBottom, (uSkyTop + uSkyHorizon) * 0.5, 0.5);
    skyColor = mix(skyColor, ambientMean, clamp(rough * 0.8, 0.0, 1.0));

    return skyColor * uSkyExposure;
}

mat3 CotangentFrame(vec3 N, vec3 p, vec2 uv) {
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)) + 1e-10);
    return mat3(T * invmax, B * invmax, N);
}

void main(){
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) N = -N;
    
    // Normal mapping with true UV-aligned Cotangent Frame & scalable strength
    if(uUseNormalMap) {
        vec3 mapN = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
        mapN.xy *= uNormalScale;
        mat3 TBN = CotangentFrame(N, vWorldPos, vUV);
        N = normalize(TBN * mapN);
    }
    
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-V, N);
    float NoV = max(dot(N, V), 1e-4);

    vec3 albedo = uAlbedo * vColor;
    if(uUseDiffuseMap) {
        vec4 diff = texture(uDiffuseMap, vUV);
        if (diff.a < 0.05) discard;
        albedo *= diff.rgb;
    }
    float metallic = clamp(uMetallic,0.0,1.0);
    float rough = clamp(uRoughness,0.04,1.0);

    // Wetness effect: rain darkens albedo and flattens roughness for mirror reflections
    if(uWetness > 0.001) {
        float w = clamp(uWetness, 0.0, 1.0);
        albedo = mix(albedo, albedo * 0.65, w);
        rough = mix(rough, max(rough * 0.08, 0.02), w);
    }

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
    vec3 vxgiBounce = CalculateVXGI(vWorldPos, N, V, rough, albedo);
    vec3 probeGI = SampleProbeIrradiance(vWorldPos, N) * albedo;
    vec3 ambient = (diffuseEnv + specularEnv + vxgiBounce + probeGI) * uAO;

    vec3 Lo = vec3(0.0);
    if(uHasDirLight){
        vec3 L = normalize(-uDirLight.direction);
        float NoL = dot(N, L);
        if(NoL > 0.0){
            float viewZ = abs((uView * vec4(vWorldPos, 1.0)).z);
            float sh = shadowFactor(vWorldPos, N, L, viewZ);
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
uniform vec2 uUVTiling;
void main(){
    vec2 tiling = (uUVTiling.x > 0.0 && uUVTiling.y > 0.0) ? uUVTiling : vec2(1.0);
    vColor=aColor; vUV=aUV * tiling;
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

uniform float uAO;
uniform float uWetness;
uniform sampler2D uDiffuseMap; uniform bool uUseDiffuseMap;
uniform sampler2D uNormalMap; uniform bool uUseNormalMap; uniform float uNormalScale;
uniform sampler2D uSpecularMap; uniform bool uUseSpecularMap;
uniform vec3 uFogColor; uniform float uFogDensity;
// cascaded shadows
uniform mat4 uLightMatrices[3];
uniform float uCascadeSplits[4];
uniform sampler2DArray uShadowMapArray;
uniform bool uHasShadow;
// sky environment IBL
uniform vec3 uSkyTop; uniform vec3 uSkyHorizon; uniform vec3 uSkyBottom; uniform float uSkyExposure;

// VXGI
uniform sampler3D uVoxelGrid;
uniform bool uUseVXGI;
uniform vec3 uVoxelCenter;
uniform float uVoxelExtent;
uniform float uVXGIIntensity;

vec3 WorldToVoxel(vec3 p) {
    return (p - (uVoxelCenter - vec3(uVoxelExtent * 0.5))) / uVoxelExtent;
}

vec4 TraceCone(vec3 origin, vec3 dir, float aperture, float maxDist) {
    vec4 accum = vec4(0.0);
    float dist = 0.4;
    float voxelSize = uVoxelExtent / 64.0;

    while (dist < maxDist && accum.a < 0.95) {
        vec3 p = origin + dir * dist;
        vec3 uvw = WorldToVoxel(p);
        if (any(lessThan(uvw, vec3(0.0))) || any(greaterThan(uvw, vec3(1.0)))) break;

        float diameter = 2.0 * aperture * dist;
        float mipLevel = clamp(log2(max(diameter / voxelSize, 1.0)), 0.0, 5.0);

        vec4 voxelSample = textureLod(uVoxelGrid, uvw, mipLevel);
        float a = 1.0 - accum.a;
        accum.rgb += voxelSample.rgb * voxelSample.a * a;
        accum.a += voxelSample.a * a;

        dist += max(diameter * 0.5, voxelSize);
    }
    return accum;
}

vec3 CalculateVXGI(vec3 worldPos, vec3 N, vec3 V, float roughness, vec3 albedo) {
    if (!uUseVXGI) return vec3(0.0);

    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 coneDirs[6] = vec3[](
        N,
        normalize(N + tangent * 0.5),
        normalize(N - tangent * 0.5),
        normalize(N + bitangent * 0.5),
        normalize(N - bitangent * 0.5),
        normalize(N + (tangent + bitangent) * 0.35)
    );
    float coneWeights[6] = float[](0.35, 0.13, 0.13, 0.13, 0.13, 0.13);

    vec3 indirectDiffuse = vec3(0.0);
    float diffuseAperture = 0.577; // tan(30 deg)
    for (int i = 0; i < 6; ++i) {
        vec4 c = TraceCone(worldPos + N * 0.15, coneDirs[i], diffuseAperture, uVoxelExtent * 0.6);
        indirectDiffuse += c.rgb * coneWeights[i];
    }

    return (indirectDiffuse * albedo) * uVXGIIntensity;
}

// Light Probes SSBO (Baked Mixed GI)
struct GPULightProbe {
    vec4 pos;
    vec4 sh[9];
};

layout(std430, binding = 6) readonly buffer LightProbeBlockInst {
    vec4 uProbeMin;
    vec4 uProbeMax;
    ivec4 uProbeCounts;
    GPULightProbe uLightProbes[];
};

uniform bool uUseLightProbes;
uniform float uLightProbeIntensity;

vec3 SampleProbeIrradiance(vec3 worldPos, vec3 N) {
    if (!uUseLightProbes || uProbeCounts.w <= 0) return vec3(0.0);

    vec3 minB = uProbeMin.xyz;
    vec3 maxB = uProbeMax.xyz;
    ivec3 counts = uProbeCounts.xyz;

    vec3 local = (worldPos - minB) / (maxB - minB);
    if (any(lessThan(local, vec3(0.0))) || any(greaterThan(local, vec3(1.0)))) return vec3(0.0);

    vec3 fIndex = local * vec3(counts - ivec3(1));
    ivec3 i0 = clamp(ivec3(fIndex), ivec3(0), counts - ivec3(2));
    ivec3 i1 = i0 + ivec3(1);
    vec3 t = fract(fIndex);

    int idx000 = i0.z * counts.x * counts.y + i0.y * counts.x + i0.x;
    int idx100 = i0.z * counts.x * counts.y + i0.y * counts.x + i1.x;
    int idx010 = i0.z * counts.x * counts.y + i1.y * counts.x + i0.x;
    int idx110 = i0.z * counts.x * counts.y + i1.y * counts.x + i1.x;
    int idx001 = i1.z * counts.x * counts.y + i0.y * counts.x + i0.x;
    int idx101 = i1.z * counts.x * counts.y + i0.y * counts.x + i1.x;
    int idx011 = i1.z * counts.x * counts.y + i1.y * counts.x + i0.x;
    int idx111 = i1.z * counts.x * counts.y + i1.y * counts.x + i1.x;

    vec3 sh00 = mix(uLightProbes[idx000].sh[0].rgb, uLightProbes[idx100].sh[0].rgb, t.x);
    vec3 sh01 = mix(uLightProbes[idx010].sh[0].rgb, uLightProbes[idx110].sh[0].rgb, t.x);
    vec3 shY0 = mix(sh00, sh01, t.y);

    vec3 sh10 = mix(uLightProbes[idx001].sh[0].rgb, uLightProbes[idx101].sh[0].rgb, t.x);
    vec3 sh11 = mix(uLightProbes[idx011].sh[0].rgb, uLightProbes[idx111].sh[0].rgb, t.x);
    vec3 shY1 = mix(sh10, sh11, t.y);

    vec3 l0 = mix(shY0, shY1, t.z);
    return l0 * (0.5 + 0.5 * N.y) * uLightProbeIntensity;
}

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
float sampleCascadeI(int cascadeIndex, vec3 worldPos, vec3 N, vec3 L) {
    vec4 lp = uLightMatrices[cascadeIndex] * vec4(worldPos, 1.0);
    vec3 proj = lp.xyz / lp.w * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        if (cascadeIndex < 2) {
            vec4 lpNext = uLightMatrices[cascadeIndex + 1] * vec4(worldPos, 1.0);
            vec3 projNext = lpNext.xyz / lpNext.w * 0.5 + 0.5;
            if (projNext.z <= 1.0 && projNext.x >= 0.0 && projNext.x <= 1.0 && projNext.y >= 0.0 && projNext.y <= 1.0) {
                float biasN = max(0.0025 * (1.0 - dot(N, L)), 0.0008) * (1.0 + float(cascadeIndex + 1) * 1.5);
                float d = texture(uShadowMapArray, vec3(projNext.xy, float(cascadeIndex + 1))).r;
                return (projNext.z - biasN > d) ? 0.0 : 1.0;
            }
        }
        return 1.0;
    }

    float bias = max(0.0025 * (1.0 - dot(N, L)), 0.0008) * (1.0 + float(cascadeIndex) * 1.5);
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

float shadowFactorI(vec3 worldPos, vec3 N, vec3 L, float viewDist){
    if(!uHasShadow) return 1.0;
    
    int cascadeIndex = 2;
    float blend = 0.0;
    int nextCascade = 2;

    if (viewDist < uCascadeSplits[1]) {
        cascadeIndex = 0;
        float d = uCascadeSplits[1] - viewDist;
        if (d < 3.0) { blend = 1.0 - (d / 3.0); nextCascade = 1; }
    } else if (viewDist < uCascadeSplits[2]) {
        cascadeIndex = 1;
        float d = uCascadeSplits[2] - viewDist;
        if (d < 5.0) { blend = 1.0 - (d / 5.0); nextCascade = 2; }
    }

    float s = sampleCascadeI(cascadeIndex, worldPos, N, L);
    if (blend > 0.0) {
        float sNext = sampleCascadeI(nextCascade, worldPos, N, L);
        s = mix(s, sNext, blend);
    }
    return s;
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

mat3 CotangentFrame(vec3 N, vec3 p, vec2 uv) {
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)) + 1e-10);
    return mat3(T * invmax, B * invmax, N);
}

void main(){
    vec3 N=normalize(vNormal);
    if (!gl_FrontFacing) N = -N;
    if(uUseNormalMap){
        vec3 mapN=texture(uNormalMap,vUV).xyz*2.0-1.0;
        mapN.xy *= (uNormalScale * 3.0);
        mat3 TBN = CotangentFrame(N, vWorldPos, vUV);
        N=normalize(TBN*mapN);
    }
    vec3 V=normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-V, N);
    float NoV = max(dot(N, V), 1e-4);

    vec3 albedo=vInstAlbedo.rgb * vColor;
    float rough = clamp(vInstAlbedo.a, 0.04, 1.0);
    float metallic = clamp(vInstEmissive.a, 0.0, 1.0);
    if(uUseDiffuseMap) {
        vec4 diff = texture(uDiffuseMap, vUV);
        if (diff.a < 0.05) discard;
        albedo *= diff.rgb;
    }
    
    // Wetness effect: rain darkens albedo and flattens roughness for mirror reflections
    if(uWetness > 0.001) {
        float w = clamp(uWetness, 0.0, 1.0);
        albedo = mix(albedo, albedo * 0.65, w);
        rough = mix(rough, max(rough * 0.08, 0.02), w);
    }

    if(uUseSpecularMap){
        vec3 sm=texture(uSpecularMap,vUV).rgb;
        rough=clamp(1.0-sm.g,0.04,1.0);
        albedo=mix(albedo,albedo*sm.r,0.3);
    }
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 F_env = F0 + (max(vec3(1.0 - rough), F0) - F0) * pow(clamp(1.0 - NoV, 0.0, 1.0), 5.0);
    vec3 kD_env = (vec3(1.0) - F_env) * (1.0 - metallic);
    vec3 diffuseEnv = kD_env * albedo * (uAmbientColor * uAmbientIntensity * (0.5 + 0.5 * N.y));
    
    vec3 skySpecular = SampleSkyI(R, rough);
    vec3 specularEnv = F_env * skySpecular;
    vec3 vxgiBounce = CalculateVXGI(vWorldPos, N, V, rough, albedo);
    vec3 probeGI = SampleProbeIrradiance(vWorldPos, N) * albedo;
    vec3 ambient = (diffuseEnv + specularEnv + vxgiBounce + probeGI) * uAO;

    vec3 Lo=vec3(0.0);
    if(uHasDirLight){
        vec3 L=normalize(-uDirLight.direction);
        float NoL = dot(N, L);
        if(NoL > 0.0){
            float viewZ = abs((uView * vec4(vWorldPos, 1.0)).z);
            float sh = shadowFactorI(vWorldPos, N, L, viewZ);
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
    vDir = aPos;
    vec4 pos = uProj * uView * vec4(aPos, 1.0);
    gl_Position = vec4(pos.xy, pos.w * 0.99999, pos.w);
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
layout(location=8) in mat4 aPrevModel; // 8,9,10,11 — per-instance prev model
out vec3 vColor; out vec2 vUV; out vec3 vNormal; out vec3 vWorldPos;
out vec3 vTangent; out vec3 vBitangent;
out vec4 vCurrClip; out vec4 vPrevClip;
uniform mat4 uView; uniform mat4 uProj;
uniform mat4 uPrevViewProj;
uniform vec2 uUVTiling;
void main(){
    vec2 tiling = (uUVTiling.x > 0.0 && uUVTiling.y > 0.0) ? uUVTiling : vec2(1.0);
    vColor=aColor; vUV=aUV * tiling;
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
    vec4 prevWp = aPrevModel * vec4(aPos, 1.0);
    vPrevClip = (uPrevViewProj[3][3] != 0.0) ? (uPrevViewProj * prevWp) : vCurrClip;
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
    float depth = texture(uDepth, vUV).r;

    // Skybox / infinite background — instant pass-through (no temporal ghosting)
    if (depth >= 0.9999) {
        FragColor = current;
        return;
    }

    // 5-Tap Cross Neighborhood (tighter bound than 3x3 square)
    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    vec3 minColor = vec3(1000.0);
    vec3 maxColor = vec3(-1000.0);

    const vec2 offsets[5] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0)
    );

    for (int i = 0; i < 5; ++i) {
        vec3 c = RGBToYCoCg(texture(uCurrentColor, vUV + offsets[i] * uTexelSize).rgb);
        m1 += c;
        m2 += c * c;
        minColor = min(minColor, c);
        maxColor = max(maxColor, c);
    }

    vec3 mu = m1 / 5.0;
    vec3 sigma = sqrt(max(m2 / 5.0 - mu * mu, vec3(0.0)));
    const float gamma = 1.25; // Standard stable variance box (Unreal / Karis model)
    vec3 boxMin = max(minColor, mu - gamma * sigma);
    vec3 boxMax = min(maxColor, mu + gamma * sigma);

    vec4 clipPos = vec4(vUV * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = uInvViewProj * clipPos;
    worldPos /= max(worldPos.w, 1e-5);

    vec4 prevClipPos = uPrevViewProj * worldPos;
    vec2 prevUV = (prevClipPos.xy / max(prevClipPos.w, 1e-5)) * 0.5 + 0.5;

    if (prevUV.x < 0.002 || prevUV.x > 0.998 || prevUV.y < 0.002 || prevUV.y > 0.998) {
        FragColor = current;
        return;
    }

    vec4 history = texture(uHistoryColor, prevUV);
    vec3 historyYCoCg = RGBToYCoCg(history.rgb);

    // Variance clipping of history sample to eliminate ghosting trails
    historyYCoCg = clamp(historyYCoCg, boxMin, boxMax);
    vec3 clampedHistory = YCoCgToRGB(historyYCoCg);

    // Temporal accumulation with stable motion continuity (Unreal Engine Karis model)
    // Variance clipping already eliminates ghosting trails without collapsing feedback to 0
    float motion = length(prevUV - vUV);
    float motionFade = clamp(motion * 8.0, 0.0, 1.0);

    float feedback = clamp(uFeedbackFactor, 0.78, 0.90);
    float blend = mix(feedback, 0.72, motionFade);
    vec3 finalColor = mix(current.rgb, clampedHistory, blend);
    FragColor = vec4(finalColor, 1.0);
}
)";

// Screen Space Reflections (SSR)
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
    if (roughness > 0.55) {
        FragColor = vec4(0.0);
        return;
    }

    // Decode view-space normal from RGBA16F buffer (already in [-1, 1])
    vec3 rawNormal = normRough.xyz;
    if (length(rawNormal) < 0.05) {
        FragColor = vec4(0.0);
        return;
    }
    vec3 normal = normalize(rawNormal);

    vec3 viewPos = getPosition(vUV);
    vec3 viewDir = normalize(viewPos);
    vec3 reflDir = normalize(reflect(viewDir, normal));
    
    // If reflection points towards the camera, reject to prevent self-reflection
    if (reflDir.z > -0.05) {
        FragColor = vec4(0.0);
        return;
    }

    float stepSize = 0.16;
    vec3 rayPos = viewPos + normal * 0.05 + reflDir * 0.10;
    vec3 rayStep = reflDir * stepSize;
    
    vec2 hitUV = vec2(0.0);
    float hitConfidence = 0.0;
    
    for (int i = 0; i < uMaxSteps; ++i) {
        vec3 prevRayPos = rayPos;
        rayPos += rayStep;
        
        vec4 proj = uProj * vec4(rayPos, 1.0);
        vec2 sampleUV = (proj.xy / max(proj.w, 1e-5)) * 0.5 + 0.5;
        
        if (sampleUV.x < 0.005 || sampleUV.x > 0.995 || sampleUV.y < 0.005 || sampleUV.y > 0.995) break;
        
        // Avoid self-hit at ray origin
        if (i < 2 && length(sampleUV - vUV) < 0.012) continue;

        float sampleDepth = texture(uDepthTex, sampleUV).r;
        if (sampleDepth >= 0.9999) {
            stepSize *= 1.05;
            rayStep = reflDir * stepSize;
            continue;
        }
        
        vec4 sampleClip = vec4(sampleUV * 2.0 - 1.0, sampleDepth * 2.0 - 1.0, 1.0);
        vec4 sampleViewPos = uInvProj * sampleClip;
        sampleViewPos /= max(sampleViewPos.w, 1e-5);
        
        float depthDiff = rayPos.z - sampleViewPos.z;
        float prevDepthDiff = prevRayPos.z - sampleViewPos.z;

        // Valid intersection condition:
        // 1. Ray penetrated from front to behind (prevRayPos in front, rayPos behind)
        // 2. Ray did not penetrate deeper than thickness
        float dynThickness = max(uThickness, abs(rayStep.z) * 1.5);
        if (prevDepthDiff >= -0.04 && depthDiff <= 0.0 && depthDiff > -dynThickness) {
            // 3. Backface rejection: Surface normal must face TOWARDS the reflection ray!
            vec4 hitNormRough = texture(uNormalRoughnessTex, sampleUV);
            vec3 hitNormal = hitNormRough.xyz;
            if (dot(hitNormal, reflDir) < -0.1) {
                // Binary search refinement
                vec3 refinePos = rayPos - rayStep;
                vec3 halfStep = rayStep * 0.5;
                for (int j = 0; j < 4; ++j) {
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
                vec2 edge = smoothstep(0.0, 0.12, hitUV) * smoothstep(1.0, 0.88, hitUV);
                float edgeFade = edge.x * edge.y;
                
                // Physically-based Fresnel (Schlick)
                float NoV = max(dot(-viewDir, normal), 0.0);
                float fresnel = mix(0.04, 1.0, pow(1.0 - NoV, 5.0));
                
                // Smooth roughness fade (only smooth surfaces reflect sharply)
                float gloss = smoothstep(0.55, 0.05, roughness);
                
                hitConfidence = edgeFade * fresnel * gloss;
                break;
            }
        }
        stepSize *= 1.04;
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
    // Softly compress specular highlights from intense emissive lights
    vec3 refl = ssr.rgb / (1.0 + ssr.rgb * 0.08);
    vec3 result = scene + refl * ssr.a * uSSRIntensity;
    FragColor = vec4(result, 1.0);
}
)";

// ---- GTAO (Ground Truth Ambient Occlusion) — half-res ----
inline const char* kGTAOFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uDepthTex;
uniform sampler2D uNormalTex;
uniform mat4 uInvProj;
uniform mat4 uView;
uniform vec2 uScreenSize;
uniform float uRadius;
uniform float uIntensity;
uniform int uDirections;
uniform int uSteps;

const float PI = 3.14159265359;

vec3 reconstructViewPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = uInvProj * clip;
    return view.xyz / max(view.w, 1e-5);
}

float interleavedGradientNoise(vec2 pixel) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(pixel, magic.xy)));
}

void main() {
    vec2 halfUV = vUV;
    float depth = texture(uDepthTex, halfUV).r;
    if (depth >= 0.9999 || depth < 0.0001) {
        FragColor = vec4(1.0);
        return;
    }

    vec3 P = reconstructViewPos(halfUV, depth);
    vec3 rawNormal = texture(uNormalTex, halfUV).xyz;
    vec3 N = (length(rawNormal) > 0.05) ? normalize(rawNormal) : vec3(0.0, 0.0, 1.0);

    float occlusion = 0.0;
    float randomAngle = interleavedGradientNoise(gl_FragCoord.xy) * 2.0 * PI;
    
    // Project world-space radius into screen-space UV coordinates
    float radiusUV = clamp(uRadius * 0.4 / max(-P.z, 0.5), 0.005, 0.2);

    for (int i = 0; i < uDirections; ++i) {
        float angle = (float(i) / float(uDirections)) * PI + randomAngle;
        vec2 direction = vec2(cos(angle), sin(angle));
        float maxHorizon = 0.0;

        for (int j = 1; j <= uSteps; ++j) {
            float t = float(j) / float(uSteps);
            vec2 sampleUV = halfUV + direction * (t * radiusUV);
            if (sampleUV.x < 0.001 || sampleUV.x > 0.999 || sampleUV.y < 0.001 || sampleUV.y > 0.999) continue;

            float sampleDepth = texture(uDepthTex, sampleUV).r;
            if (sampleDepth >= 0.9999 || sampleDepth < 0.0001) continue;

            vec3 S = reconstructViewPos(sampleUV, sampleDepth);
            vec3 horizonVec = S - P;
            float dist = length(horizonVec);
            if (dist < 0.05 || dist > uRadius * 2.0) continue;

            float cosTheta = dot(normalize(horizonVec), N);
            float falloff = clamp(1.0 - (dist / (uRadius * 2.0)), 0.0, 1.0);
            
            // Elevation threshold to reject flat surface false positives
            if (cosTheta > 0.25) {
                maxHorizon = max(maxHorizon, (cosTheta - 0.25) * falloff);
            }
        }
        occlusion += maxHorizon;
    }

    occlusion /= float(uDirections);
    float ao = clamp(1.0 - occlusion * uIntensity * 1.5, 0.0, 1.0);
    FragColor = vec4(vec3(ao), 1.0);
}
)";

inline const char* kGTAOBlurFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uAOTex;
uniform sampler2D uDepthTex;
uniform vec2 uScreenSize;

void main() {
    float centerDepth = texture(uDepthTex, vUV).r;
    if (centerDepth >= 0.9999 || centerDepth < 0.0001) {
        FragColor = vec4(1.0);
        return;
    }
    vec2 texel = 1.0 / uScreenSize;
    float centerAO = texture(uAOTex, vUV).r;
    float totalAO = centerAO;
    float totalWeight = 1.0;

    // Cross-bilateral 5x5 filter with depth & spatial gaussian weights
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            if (x == 0 && y == 0) continue;
            vec2 offset = vec2(float(x), float(y)) * texel;
            vec2 sampleUV = vUV + offset;
            if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) continue;

            float d = texture(uDepthTex, sampleUV).r;
            float spatialWeight = exp(-float(x * x + y * y) * 0.25);
            float depthWeight = exp(-abs(centerDepth - d) * 300.0);
            float weight = spatialWeight * depthWeight;

            totalAO += texture(uAOTex, sampleUV).r * weight;
            totalWeight += weight;
        }
    }
    float finalAO = totalAO / max(totalWeight, 0.0001);
    FragColor = vec4(vec3(finalAO), 1.0);
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
    ao = clamp(ao, 0.1, 1.0);
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
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = uInvProj * clip;
    view /= view.w;
    vec4 world = uInvView * view;
    return world.xyz;
}

void main() {
    float depth = texture(uDepthTex, vUV).r;
    if (depth >= 0.9999) {
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    vec3 worldPos = reconstructWorldPos(vUV, depth);
    vec3 rayDir = worldPos - uCameraPos;
    float rayLength = length(rayDir);
    rayDir /= max(rayLength, 0.001);

    float startDist = max(0.0, uFogStart);
    float endDist = min(rayLength, uFogEnd);
    if (startDist >= endDist) {
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    float stepSize = (endDist - startDist) / float(uStepCount);
    float totalDensity = 0.0;
    vec3 fogContrib = vec3(0.0);

    for (int i = 0; i < uStepCount; ++i) {
        float d = startDist + (float(i) + 0.5) * stepSize;
        vec3 p = uCameraPos + rayDir * d;
        float h = p.y - uFogHeight;
        float heightDensity = exp(-max(0.0, h) * uFogHeightFalloff);
        float n = fbm(p * 0.05 + vec3(0.0, uTime * 0.02, 0.0));
        float sampleDensity = uFogDensity * heightDensity * (0.8 + 0.4 * n);
        totalDensity += sampleDensity * stepSize;
        fogContrib += uFogColor * sampleDensity * stepSize;
    }

    vec3 inScattering = (totalDensity > 1e-4) ? (fogContrib / totalDensity) : uFogColor;
    float extinction = clamp(1.0 - exp(-totalDensity), 0.0, 1.0);
    FragColor = vec4(inScattering, extinction);
}
)";

// Volumetric fog composite — upsample + alpha blend
inline const char* kFogCompositeFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uSceneTex;
uniform sampler2D uFogTex;
void main() {
    vec3 scene = texture(uSceneTex, vUV).rgb;
    vec4 fog = texture(uFogTex, vUV);
    vec3 result = mix(scene, fog.rgb, fog.a);
    FragColor = vec4(result, 1.0);
}
)";

// ---- Light Shafts (God Rays) — pure sun occlusion mask + radial blur ----
inline const char* kSunMaskFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uDepthTex;
uniform vec2 uSunScreenPos;

void main() {
    float depth = texture(uDepthTex, vUV).r;
    if (depth < 0.9999) {
        // Any foreground geometry blocks the sun completely
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    vec2 toSun = vUV - uSunScreenPos;
    float dist = length(toSun);
    float sunDisk = clamp(exp(-dist * 16.0) * 2.5, 0.0, 2.5);
    FragColor = vec4(vec3(sunDisk), 1.0);
}
)";

inline const char* kLightShaftsFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uMaskTex;
uniform vec2 uSunScreenPos;
uniform float uDensity;
uniform float uWeight;
uniform float uDecay;
uniform float uExposure;
uniform int uSamples;

float ditherNoise(vec2 coord) {
    return fract(sin(dot(coord, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 deltaTexCoord = (vUV - uSunScreenPos) * uDensity / float(uSamples);
    vec3 illumination = vec3(0.0);
    float illuminationDecay = 1.0;

    float jitter = ditherNoise(gl_FragCoord.xy);
    vec2 sampleCoord = vUV - deltaTexCoord * jitter;

    for (int i = 0; i < uSamples; ++i) {
        sampleCoord -= deltaTexCoord;
        if (sampleCoord.x >= 0.0 && sampleCoord.x <= 1.0 && sampleCoord.y >= 0.0 && sampleCoord.y <= 1.0) {
            vec3 sampleColor = texture(uMaskTex, sampleCoord).rgb;
            illumination += sampleColor * illuminationDecay * uWeight;
        }
        illuminationDecay *= uDecay;
    }

    vec3 godRays = illumination * uExposure;
    FragColor = vec4(godRays, 1.0);
}
)";

inline const char* kLightShaftsCompositeFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uSceneTex;
uniform sampler2D uShaftsTex;
uniform vec3 uSunColor;
void main() {
    vec3 scene = texture(uSceneTex, vUV).rgb;
    vec3 shafts = texture(uShaftsTex, vUV).rgb;
    FragColor = vec4(scene + shafts * uSunColor * 1.8, 1.0);
}
)";

// Radial Speed Motion Blur & Chromatic Aberration (AAA Racing / NFS Speed Feel)
inline const char* kRadialSpeedBlurFS = R"(
#version 460 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSceneTex;
uniform vec2 uCenter;
uniform float uBlurStrength;
uniform float uChromaticAberration;

void main() {
    if (uBlurStrength <= 0.001) {
        FragColor = texture(uSceneTex, vUV);
        return;
    }

    vec2 dir = vUV - uCenter;
    float dist = length(dir);
    float blurFactor = dist * uBlurStrength * 0.07;

    const int SAMPLES = 8;
    vec3 colorAccum = vec3(0.0);

    for (int i = 0; i < SAMPLES; ++i) {
        float scale = 1.0 - blurFactor * (float(i) / float(SAMPLES - 1));
        vec2 sampleUV = uCenter + dir * scale;

        float r = texture(uSceneTex, sampleUV + dir * (uChromaticAberration * dist)).r;
        float g = texture(uSceneTex, sampleUV).g;
        float b = texture(uSceneTex, sampleUV - dir * (uChromaticAberration * dist)).b;

        colorAccum += vec3(r, g, b);
    }

    FragColor = vec4(colorAccum / float(SAMPLES), 1.0);
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

