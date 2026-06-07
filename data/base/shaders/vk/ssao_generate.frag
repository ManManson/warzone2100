#version 450

layout(std140, set = 0, binding = 0) uniform cbuffer {
	mat4 invProjectionMatrix;
	vec4 params;
	vec4 kernel[16];
};

layout(set = 1, binding = 0) uniform sampler2D depthTexture;
layout(set = 1, binding = 1) uniform sampler2D noiseTexture;

layout(location = 0) in vec2 texCoords;
layout(location = 0) out vec4 FragColor;

const int KERNEL_SIZE = 16;

vec3 getViewPosition(vec2 uv, float depth)
{
	vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpace = invProjectionMatrix * clipSpace;
	return viewSpace.xyz / viewSpace.w;
}

void main()
{
	float depth = texture(depthTexture, texCoords).r;
	vec3 origin = getViewPosition(texCoords, depth);

	vec2 noiseScale = params.zw;
	vec2 noiseUV = texCoords * noiseScale;
	vec3 randomVec = normalize(texture(noiseTexture, noiseUV).xyz * 2.0 - 1.0);

	vec3 tangent = normalize(randomVec - origin * dot(origin, randomVec));
	vec3 bitangent = cross(origin, tangent);
	mat3 tbn = mat3(tangent, bitangent, origin);

	float occlusion = 0.0;
	for (int i = 0; i < KERNEL_SIZE; ++i)
	{
		vec3 samplePos = tbn * kernel[i].xyz;
		samplePos = origin + samplePos * params.x;

		vec4 offset = invProjectionMatrix * vec4(samplePos, 1.0);
		offset.xyz /= offset.w;
		vec2 sampleUV = offset.xy * 0.5 + 0.5;

		if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
		{
			continue;
		}

		float sampleDepth = texture(depthTexture, sampleUV).r;
		vec3 sampleViewPos = getViewPosition(sampleUV, sampleDepth);
		float rangeCheck = smoothstep(0.0, 1.0, params.x / abs(origin.z - sampleViewPos.z));
		occlusion += (sampleViewPos.z >= samplePos.z + params.y ? 1.0 : 0.0) * rangeCheck;
	}

	occlusion = 1.0 - (occlusion / float(KERNEL_SIZE));
	FragColor = vec4(vec3(occlusion), 1.0);
}
