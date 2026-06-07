#version 450

layout(std140, set = 0, binding = 0) uniform cbuffer {
	float ssaoIntensity;
	vec3 padding;
};

layout(set = 1, binding = 0) uniform sampler2D sceneTexture;
layout(set = 1, binding = 1) uniform sampler2D ssaoTexture;

layout(location = 0) in vec2 texCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
	vec3 scene = texture(sceneTexture, texCoords).rgb;
	float ao = texture(ssaoTexture, texCoords).r;
	vec3 result = scene * mix(1.0, ao, ssaoIntensity);
	FragColor = vec4(result, 1.0);
}
