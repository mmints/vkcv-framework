#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "perMeshResources.inc"

layout(location = 0) in vec3 passNormal;
layout(location = 1) in vec2 passUV;
layout(location = 2) in vec3 passPos;

layout(location = 0) out vec3 outColor;

layout(set=0, binding=0) uniform sunBuffer {
    vec3 L; float padding;
    mat4 lightMatrix;
};
layout(set=0, binding=1) uniform texture2D  shadowMap;
layout(set=0, binding=2) uniform sampler    shadowMapSampler;

float shadowTest(vec3 worldPos){
    vec4 lightPos = lightMatrix * vec4(worldPos, 1);
    lightPos /= lightPos.w;
    lightPos.xy = lightPos.xy * 0.5 + 0.5;
    
    if(any(lessThan(lightPos.xy, vec2(0))) || any(greaterThan(lightPos.xy, vec2(1)))){
        return 1;
    }
    
    lightPos.z = clamp(lightPos.z, 0, 1);
    
    float shadowMapSample = texture(sampler2D(shadowMap, shadowMapSampler), lightPos.xy).r;
    float bias = 0.01f;
    shadowMapSample += bias;
    return shadowMapSample < lightPos.z ? 0 : 1;
}

void main()	{
    vec3 N = normalize(passNormal);
    vec3 sunColor = vec3(1);
    vec3 sun = sunColor * clamp(dot(N, L), 0, 1);
    sun *= shadowTest(passPos);
    vec3 ambient = vec3(0.1);
    vec3 albedo = texture(sampler2D(albedoTexture, textureSampler), passUV).rgb;
	outColor = albedo * (sun + ambient);
}