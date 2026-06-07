#version 450

layout(std140, set = 0, binding = 0) uniform cbuffer {
	vec2 blurDirection;
	vec2 padding;
};

layout(set = 1, binding = 0) uniform sampler2D occlusionTexture;

layout(location = 0) in vec2 texCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
	float result = 0.0;
	result += texture(occlusionTexture, texCoords + blurDirection * 1.0).r;
	result += texture(occlusionTexture, texCoords + blurDirection * 2.0).r;
	result += texture(occlusionTexture, texCoords + blurDirection * 3.0).r;
	result += texture(occlusionTexture, texCoords + blurDirection * 4.0).r;
	result *= 0.25;
	FragColor = vec4(vec3(result), 1.0);
}
