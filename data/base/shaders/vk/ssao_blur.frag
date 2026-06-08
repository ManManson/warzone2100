#version 450

layout(std140, set = 0, binding = 0) uniform cbuffer {
	vec2 blurDirection;
	float depthThreshold;
	float padding;
};

layout(set = 1, binding = 0) uniform sampler2D occlusionTexture;
layout(set = 1, binding = 1) uniform sampler2D depthTexture;

layout(location = 0) in vec2 texCoords;
layout(location = 0) out vec4 FragColor;

const float SKY_DEPTH_THRESHOLD = 0.9999;

void main()
{
	float centerDepth = texture(depthTexture, texCoords).r;
	if (centerDepth >= SKY_DEPTH_THRESHOLD)
	{
		FragColor = vec4(1.0);
		return;
	}

	float result = texture(occlusionTexture, texCoords).r;
	float weightSum = 1.0;

	for (int i = 1; i <= 4; ++i)
	{
		vec2 sampleUV = texCoords + blurDirection * float(i);
		float sampleDepth = texture(depthTexture, sampleUV).r;
		if (sampleDepth >= SKY_DEPTH_THRESHOLD)
		{
			continue;
		}

		float depthDiff = abs(centerDepth - sampleDepth);
		float weight = 1.0 - smoothstep(0.0, depthThreshold, depthDiff);
		if (weight <= 0.0)
		{
			continue;
		}

		result += texture(occlusionTexture, sampleUV).r * weight;
		weightSum += weight;
	}

	result /= weightSum;
	FragColor = vec4(vec3(result), 1.0);
}
