#version 450

layout(std140, set = 0, binding = 0) uniform cbuffer {
	mat4 invProjectionMatrix;
	vec4 sceneUvScaleClamp;
	vec4 ssrUvScaleClamp;
	vec4 normalsUvScaleClamp;
	vec4 depthUvScaleClamp;
	float intensity;
	float F0;
	float padding0;
	float padding1;
};

layout(set = 1, binding = 0) uniform sampler2D sceneTexture;
layout(set = 1, binding = 1) uniform sampler2D ssrTexture;
layout(set = 1, binding = 2) uniform sampler2D prepassNormals;
layout(set = 1, binding = 3) uniform sampler2D prepassDepth;

layout(location = 0) in vec2 texCoords;
layout(location = 0) out vec4 FragColor;

#include "view_position.glsl"

void main()
{
	vec2 sceneUv = clamp(texCoords * sceneUvScaleClamp.xy, vec2(0.0), sceneUvScaleClamp.zw);
	vec2 ssrUv = clamp(texCoords * ssrUvScaleClamp.xy, vec2(0.0), ssrUvScaleClamp.zw);
	vec2 nUv = clamp(texCoords * normalsUvScaleClamp.xy, vec2(0.0), normalsUvScaleClamp.zw);
	vec2 dUv = clamp(texCoords * depthUvScaleClamp.xy, vec2(0.0), depthUvScaleClamp.zw);

	vec3 scene = texture(sceneTexture, sceneUv).rgb;
	vec4 ssr = texture(ssrTexture, ssrUv);
	float ssrWeight = 1.0 - texture(prepassNormals, nUv).a;
	if (ssrWeight < 1e-3 || ssr.a < 1e-3)
	{
		FragColor = vec4(scene, 1.0);
		return;
	}

	float depth = texture(prepassDepth, dUv).r;
	vec3 viewPos = wzGetViewPosition(dUv, depth, invProjectionMatrix);
	vec3 N = texture(prepassNormals, nUv).xyz * 2.0 - 1.0;
	float nLen = length(N);
	N = (nLen < 1e-5) ? vec3(0.0, 0.0, 1.0) : (N / nLen);
	vec3 V = normalize(-viewPos);
	float ndotv = clamp(dot(N, V), 0.0, 1.0);
	float F = F0 + (1.0 - F0) * pow(1.0 - ndotv, 5.0);
	float mixAmt = clamp(ssrWeight * max(F, 0.18) * ssr.a * intensity, 0.0, 1.0);
	FragColor = vec4(mix(scene, ssr.rgb, mixAmt), 1.0);
}
