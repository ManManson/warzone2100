#version 450

layout(std140, set = 0, binding = 0) uniform cbuffer {
	mat4 invProjectionMatrix;
	mat4 projectionMatrix;
	vec4 params;
	vec2 noiseScale;
	vec2 padding;
	vec4 kernel[16];
};

layout(set = 1, binding = 0) uniform sampler2D depthTexture;
layout(set = 1, binding = 1) uniform sampler2D noiseTexture;

layout(location = 0) in vec2 texCoords;
layout(location = 0) out vec4 FragColor;

const int KERNEL_SIZE = 16;
const float SKY_DEPTH_THRESHOLD = 0.9999;

vec3 getViewPosition(vec2 uv, float depth)
{
	// Vulkan NDC Z is in [0, 1].
	vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpace = invProjectionMatrix * clipSpace;
	return viewSpace.xyz / viewSpace.w;
}

vec3 getViewNormal(vec3 viewPos)
{
	vec3 dx = dFdx(viewPos);
	vec3 dy = dFdy(viewPos);
	vec3 normal = normalize(cross(dx, dy));
	if (dot(normal, normal) < 1e-8)
	{
		normal = normalize(viewPos);
	}
	return normal;
}

void main()
{
	float depth = texture(depthTexture, texCoords).r;
	if (depth >= SKY_DEPTH_THRESHOLD)
	{
		FragColor = vec4(1.0);
		return;
	}

	vec3 origin = getViewPosition(texCoords, depth);
	vec3 normal = getViewNormal(origin);

	vec2 noiseUV = texCoords * noiseScale;
	vec3 randomVec = normalize(texture(noiseTexture, noiseUV).xyz * 2.0 - 1.0);

	vec3 tangent = normalize(randomVec - normal * dot(normal, randomVec));
	vec3 bitangent = cross(normal, tangent);
	mat3 tbn = mat3(tangent, bitangent, normal);

	float radius = params.x * abs(origin.z);
	float bias = max(params.y * abs(origin.z), params.z);
	float maxDepthDiff = radius * params.w;
	float occlusion = 0.0;
	for (int i = 0; i < KERNEL_SIZE; ++i)
	{
		vec3 samplePos = origin + (tbn * kernel[i].xyz) * radius;

		vec4 offset = projectionMatrix * vec4(samplePos, 1.0);
		offset.xyz /= offset.w;
		vec2 sampleUV = offset.xy * 0.5 + 0.5;

		if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
		{
			continue;
		}

		float sampleDepth = texture(depthTexture, sampleUV).r;
		if (sampleDepth >= SKY_DEPTH_THRESHOLD)
		{
			continue;
		}

		vec3 sampleViewPos = getViewPosition(sampleUV, sampleDepth);
		float depthDiff = abs(origin.z - sampleViewPos.z);
		float rangeCheck = 1.0 - smoothstep(0.0, maxDepthDiff, depthDiff);
		occlusion += (sampleViewPos.z >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
	}

	occlusion = 1.0 - (occlusion / float(KERNEL_SIZE));
	FragColor = vec4(vec3(occlusion), 1.0);
}
