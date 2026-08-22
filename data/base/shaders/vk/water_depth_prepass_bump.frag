#version 450

layout(std140, set = 0, binding = 0) uniform cbuffer {
	mat4 ModelViewProjectionMatrix;
	mat4 ViewMatrix;
	float timeSec;
	float mipLoadBias;
	float pad0;
	float pad1;
};

layout(set = 1, binding = 0) uniform sampler2DArray tex_nm;

layout(location = 0) in vec4 uv1_uv2;
layout(location = 0) out vec4 FragColor;

#include "water_normals.glsl"

void main()
{
	vec3 modelN = wzWaterModelNormal(tex_nm, uv1_uv2.xy, uv1_uv2.zw, timeSec, mipLoadBias);
	vec3 viewN = normalize(mat3(ViewMatrix) * modelN);
	FragColor = vec4(viewN * 0.5 + 0.5, 0.0);
}
