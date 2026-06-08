// Version directive is set by Warzone when loading the shader
// (This shader supports GLSL 1.20 - 1.50 core.)

uniform mat4 invProjectionMatrix;
uniform mat4 projectionMatrix;
uniform vec4 params;
uniform vec2 noiseScale;
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
const float SKY_DEPTH_THRESHOLD = 0.9999;

vec3 getViewPosition(vec2 uv, float depth)
{
	// OpenGL depth buffer stores NDC Z mapped from [-1, 1] to [0, 1].
	float clipZ = depth * 2.0 - 1.0;
	vec4 clipSpace = vec4(uv * 2.0 - 1.0, clipZ, 1.0);
	vec4 viewSpace = invProjectionMatrix * clipSpace;
	return viewSpace.xyz / viewSpace.w;
}

vec3 faceCamera(vec3 normal, vec3 viewPos)
{
	if (dot(normal, viewPos) > 0.0)
	{
		normal = -normal;
	}
	return normal;
}

vec3 getViewNormal(vec3 viewPos)
{
	#ifdef NEWGL
	vec3 dx = dFdx(viewPos);
	vec3 dy = dFdy(viewPos);
	vec3 depthNormal = cross(dx, dy);
	float derivativeLen = length(depthNormal);
	if (derivativeLen < 1e-8)
	{
		// Large coplanar surfaces (terrain): sample away from the camera to avoid
		// false self-occlusion when the hemisphere points toward the surface.
		return normalize(viewPos);
	}
	return faceCamera(depthNormal / derivativeLen, viewPos);
	#else
	return normalize(viewPos);
	#endif
}

void main()
{
	float depth = texture(depthTexture, texCoords).r;
	if (depth >= SKY_DEPTH_THRESHOLD)
	{
		#ifdef NEWGL
		FragColor = vec4(1.0);
		#else
		gl_FragColor = vec4(1.0);
		#endif
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
		// pie_PerspectiveGet uses +Z into the scene; LearnOpenGL's >= test assumes -Z forward.
		occlusion += (sampleViewPos.z <= samplePos.z - bias ? 1.0 : 0.0) * rangeCheck;
	}

	occlusion = 1.0 - (occlusion / float(KERNEL_SIZE));

	#ifdef NEWGL
	FragColor = vec4(vec3(occlusion), 1.0);
	#else
	gl_FragColor = vec4(vec3(occlusion), 1.0);
	#endif
}
