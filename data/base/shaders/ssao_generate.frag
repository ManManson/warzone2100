// Version directive is set by Warzone when loading the shader
// (This shader supports GLSL 1.20 - 1.50 core.)

uniform mat4 invProjectionMatrix;
uniform mat4 projectionMatrix;
uniform vec4 params;
uniform vec4 kernel[16];

uniform sampler2D depthTexture;
uniform sampler2D noiseTexture;

#if (!defined(GL_ES) && (__VERSION__ >= 130)) || (defined(GL_ES) && (__VERSION__ >= 300))
#define NEWGL
#else
#define texture(tex, uv) texture2D(tex, uv)
#endif

#ifdef NEWGL
in vec2 texCoords;
#else
varying vec2 texCoords;
#endif

#ifdef NEWGL
out vec4 FragColor;
#else
// Uses gl_FragColor
#endif

const int KERNEL_SIZE = 16;

vec3 getViewPosition(vec2 uv, float depth)
{
	// OpenGL depth buffer stores NDC Z mapped from [-1, 1] to [0, 1].
	float clipZ = depth * 2.0 - 1.0;
	vec4 clipSpace = vec4(uv * 2.0 - 1.0, clipZ, 1.0);
	vec4 viewSpace = invProjectionMatrix * clipSpace;
	return viewSpace.xyz / viewSpace.w;
}

void main()
{
	float depth = texture(depthTexture, texCoords).r;
	vec3 origin = getViewPosition(texCoords, depth);
	vec3 normal = normalize(origin);

	vec2 noiseScale = params.zw;
	vec2 noiseUV = texCoords * noiseScale;
	vec3 randomVec = normalize(texture(noiseTexture, noiseUV).xyz * 2.0 - 1.0);

	vec3 tangent = normalize(randomVec - normal * dot(normal, randomVec));
	vec3 bitangent = cross(normal, tangent);
	mat3 tbn = mat3(tangent, bitangent, normal);

	float radius = params.x * abs(origin.z);
	float bias = params.y * abs(origin.z);
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
		vec3 sampleViewPos = getViewPosition(sampleUV, sampleDepth);
		float rangeCheck = smoothstep(0.0, 1.0, radius / abs(origin.z - sampleViewPos.z));
		occlusion += (sampleViewPos.z >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
	}

	occlusion = 1.0 - (occlusion / float(KERNEL_SIZE));

	#ifdef NEWGL
	FragColor = vec4(vec3(occlusion), 1.0);
	#else
	gl_FragColor = vec4(vec3(occlusion), 1.0);
	#endif
}
