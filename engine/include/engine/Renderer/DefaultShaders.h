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
struct PointLight{ vec3 position; vec3 color; float intensity; float range; float constant; float linear; float quadratic; };
#define MAX_POINT_LIGHTS 8
uniform PointLight uPointLights[MAX_POINT_LIGHTS]; uniform int uPointLightCount;
// PBR
uniform vec3 uAlbedo; uniform float uMetallic; uniform float uRoughness; uniform float uAO;
uniform vec3 uEmissive;
uniform sampler2D uDiffuseMap; uniform bool uUseDiffuseMap;
// shadows
uniform mat4 uLightMatrix; uniform sampler2D uShadowMap; uniform bool uHasShadow;
uniform vec3 uFogColor; uniform float uFogDensity; uniform float uExposure;

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
    vec4 lp = uLightMatrix * vec4(worldPos,1.0);
    vec3 proj = lp.xyz/lp.w * 0.5 + 0.5;
    if(proj.z>1.0) return 1.0;
    float bias = max(0.0015*(1.0-dot(N,L)), 0.0008);
    // радиус растёт с дистанцией — тени мягче вдали и растворяются к краю карты
    float fade = clamp(1.0 - viewDist/60.0, 0.0, 1.0);
    if(fade<=0.0) return 1.0;
    float radius = (1.0 + viewDist*0.05) / vec2(textureSize(uShadowMap,0)).x * 2.0;
    vec2 poisson[12] = vec2[](
        vec2(-0.326,-0.406),vec2(-0.840,-0.074),vec2(-0.696, 0.457),vec2(-0.203, 0.621),
        vec2( 0.962,-0.195),vec2( 0.473,-0.480),vec2( 0.519, 0.767),vec2( 0.185,-0.893),
        vec2( 0.507, 0.064),vec2( 0.896, 0.412),vec2(-0.322,-0.933),vec2(-0.792,-0.598));
    float s=0.0;
    for(int i=0;i<12;++i){
        float d=texture(uShadowMap, proj.xy+poisson[i]*radius).r;
        s += (proj.z-bias > d) ? 0.0 : 1.0;
    }
    return mix(1.0, s/12.0, fade);
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

void main(){
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 albedo = uAlbedo * vColor;
    if(uUseDiffuseMap) albedo *= texture(uDiffuseMap,vUV).rgb;
    float metallic = clamp(uMetallic,0.0,1.0);
    float rough = clamp(uRoughness,0.04,1.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ambient IBL-lite: hemisphere + AO
    float hemi = N.y*0.5+0.5;
    vec3 ambient = (uAmbientColor*uAmbientIntensity*mix(albedo,vec3(0.5),metallic*0.5))*(0.6+0.6*hemi)*uAO;

    vec3 Lo = vec3(0.0);
    if(uHasDirLight){
        vec3 L = normalize(-uDirLight.direction);
        float sh = shadowFactor(vWorldPos,N,L, length(vWorldPos-uViewPos));
        vec3 radiance = uDirLight.color*uDirLight.intensity;
        Lo += ApplyLight(N,V,L,radiance,F0,albedo,rough,metallic)*sh;
    }
    for(int i=0;i<uPointLightCount && i<MAX_POINT_LIGHTS;++i){
        PointLight pl=uPointLights[i];
        vec3 toL = pl.position - vWorldPos;
        float dist = length(toL);
        if(dist>pl.range) continue;
        vec3 L = toL/dist;
        float atten = 1.0/(pl.constant+pl.linear*dist+pl.quadratic*dist*dist);
        float rangeFade = 1.0-clamp((dist-pl.range*0.7)/(pl.range*0.3),0.0,1.0);
        Lo += ApplyLight(N,V,L,pl.color*pl.intensity*atten*rangeFade,F0,albedo,rough,metallic);
    }

    vec3 color = ambient*uAO*0.35 + Lo + uEmissive;
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
struct PointLight{ vec3 position; vec3 color; float intensity; float range; float constant; float linear; float quadratic; };
#define MAX_POINT_LIGHTS 8
uniform PointLight uPointLights[MAX_POINT_LIGHTS]; uniform int uPointLightCount;
uniform sampler2D uDiffuseMap; uniform bool uUseDiffuseMap;
uniform sampler2D uSpecularMap; uniform bool uUseSpecularMap;
uniform vec3 uFogColor; uniform float uFogDensity;
uniform mat4 uLightMatrix; uniform sampler2D uShadowMap; uniform bool uHasShadow;

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
float shadowFactorI(vec3 worldPos, vec3 N, vec3 L, float viewDist){
    if(!uHasShadow) return 1.0;
    vec4 lp = uLightMatrix * vec4(worldPos,1.0);
    vec3 proj = lp.xyz/lp.w * 0.5 + 0.5;
    if(proj.z>1.0) return 1.0;
    float bias = max(0.0015*(1.0-dot(N,L)), 0.0008);
    float fade = clamp(1.0 - viewDist/60.0, 0.0, 1.0);
    if(fade<=0.0) return 1.0;
    float radius = (1.0 + viewDist*0.05) / vec2(textureSize(uShadowMap,0)).x * 2.0;
    vec2 poisson[12] = vec2[](
        vec2(-0.326,-0.406),vec2(-0.840,-0.074),vec2(-0.696, 0.457),vec2(-0.203, 0.621),
        vec2( 0.962,-0.195),vec2( 0.473,-0.480),vec2( 0.519, 0.767),vec2( 0.185,-0.893),
        vec2( 0.507, 0.064),vec2( 0.896, 0.412),vec2(-0.322,-0.933),vec2(-0.792,-0.598));
    float s=0.0;
    for(int i=0;i<12;++i){
        float d=texture(uShadowMap, proj.xy+poisson[i]*radius).r;
        s += (proj.z-bias > d) ? 0.0 : 1.0;
    }
    return mix(1.0, s/12.0, fade);
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
    vec3 albedo=vInstAlbedo.rgb * vColor;
    float rough = clamp(vInstAlbedo.a, 0.04, 1.0);
    float metallic = clamp(vInstEmissive.a, 0.0, 1.0);
    if(uUseDiffuseMap) albedo*=texture(uDiffuseMap,vUV).rgb;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float hemi=N.y*0.5+0.5;
    vec3 ambient=(uAmbientColor*uAmbientIntensity*mix(albedo,vec3(0.5),metallic*0.5))*(0.6+0.6*hemi);

    vec3 Lo=vec3(0.0);
    if(uHasDirLight){
        vec3 L=normalize(-uDirLight.direction);
        float sh=shadowFactorI(vWorldPos,N,L, length(vWorldPos-uViewPos));
        Lo += ApplyLight(N,V,L,uDirLight.color*uDirLight.intensity,F0,albedo,rough,metallic)*sh;
    }
    for(int i=0;i<uPointLightCount && i<MAX_POINT_LIGHTS;++i){
        PointLight pl=uPointLights[i]; 
        vec3 toL=pl.position-vWorldPos; float dist=length(toL); if(dist>pl.range) continue;
        vec3 L=toL/dist;
        float atten=1.0/(pl.constant+pl.linear*dist+pl.quadratic*dist*dist);
        float rangeFade=1.0-clamp((dist-pl.range*0.7)/(pl.range*0.3),0.0,1.0);
        Lo += ApplyLight(N,V,L,pl.color*pl.intensity*atten*rangeFade,F0,albedo,rough,metallic);
    }

    vec3 color = ambient*0.35 + Lo + vInstEmissive.rgb;
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

} // namespace DefaultShaders
