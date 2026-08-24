#version 450

layout(std140, set = 0, binding = 0) uniform cbuffer {
	mat4 invProjectionMatrix;
	mat4 projectionMatrix;
	mat4 viewToSkyLocal;
	vec4 params;              // x=maxRayLength, y=thickness, z=minRayStart, w unused
	vec4 prepassUvScaleClamp; // xy scale, zw clamp
	vec4 sceneUvScaleClamp;
	vec4 skyFogColor;         // rgb, a=fog enabled
	float stepCount;
	float skyboxAvailable;
	float padding1;
	float padding2;
};

layout(set = 1, binding = 0) uniform sampler2D depthTexture;
layout(set = 1, binding = 1) uniform sampler2D normalsTexture;
layout(set = 1, binding = 2) uniform sampler2D sceneTexture;
layout(set = 1, binding = 3) uniform sampler2D skyboxTexture;

layout(location = 0) in vec2 texCoords;
layout(location = 0) out vec4 FragColor;

#include "view_position.glsl"
#include "sky_radiance.glsl"

const float SKY_DEPTH_THRESHOLD = 0.9999;
const int MAX_STEPS = 64;
const float NORMAL_LENGTH_EPSILON = 1e-5;
const float SSR_WEIGHT_EPSILON = 1e-3;
const float UV_EPSILON = 1e-6;
const float EDGE_FADE_WIDTH = 0.05;
const float MIN_RAY_START_ABS = 1.0;
const float MIN_RAY_START_MAX_FRACTION = 0.15;
const float MISS_CONFIDENCE_MIN = 0.3;
const float MISS_CONFIDENCE_MAX = 0.65;
const float HIT_CONFIDENCE_MIN = 0.75;
const float HIT_CONFIDENCE_MAX = 1.0;
const int BINARY_SEARCH_STEPS = 4;

vec2 clipToUV(vec4 clip)
{
	return vec2(clip.x, -clip.y) * 0.5 + 0.5;
}

vec3 getViewNormal(vec2 uv)
{
	vec3 n = texture(normalsTexture, uv).xyz * 2.0 - 1.0;
	float len = length(n);
	// Empty or invalid prepass normals must not inject NaNs into the ray direction.
	if (len < NORMAL_LENGTH_EPSILON)
	{
		return vec3(0.0, 0.0, 1.0);
	}
	return n / len;
}

float edgeFade(vec2 uv, vec2 clampZW)
{
	// Screen-space rays cannot recover data beyond the rendered prepass extent.
	// Fade hits near that boundary instead of exposing a hard reflection cutoff.
	vec2 n = uv / max(clampZW, vec2(UV_EPSILON));
	float fadeX = smoothstep(0.0, EDGE_FADE_WIDTH, uv.x) * smoothstep(1.0, 1.0 - EDGE_FADE_WIDTH, n.x);
	float fadeY = smoothstep(0.0, EDGE_FADE_WIDTH, uv.y) * smoothstep(1.0, 1.0 - EDGE_FADE_WIDTH, n.y);
	return fadeX * fadeY;
}

void main()
{
	// Scale into the populated part of a potentially padded prepass texture.
	vec2 uv = clamp(texCoords * prepassUvScaleClamp.xy, vec2(0.0), prepassUvScaleClamp.zw);
	float depth = texture(depthTexture, uv).r;
	if (depth >= SKY_DEPTH_THRESHOLD)
	{
		FragColor = vec4(0.0);
		return;
	}

	// Prepass normal alpha stores SSAO weight; its inverse identifies SSR-eligible water.
	float ssrWeight = 1.0 - texture(normalsTexture, uv).a;
	if (ssrWeight < SSR_WEIGHT_EPSILON)
	{
		FragColor = vec4(0.0);
		return;
	}

	vec3 origin = wzGetViewPosition(uv, depth, invProjectionMatrix);
	vec3 N = getViewNormal(uv);
	// In view space the camera is at the origin, so V points camera -> surface.
	vec3 V = normalize(origin);
	// Reject back-facing or malformed normals before reflecting V about them.
	if (dot(N, -V) < 0.0)
	{
		FragColor = vec4(0.0);
		return;
	}

	vec3 R = reflect(V, N);
	float maxDist = max(params.x, 1.0);
	int steps = int(stepCount + 0.5);
	steps = clamp(steps, 1, MAX_STEPS);
	// Move the first sample away from the reflector to avoid immediate self-hits.
	// The offset scales with view depth but remains bounded for near/far surfaces.
	float minStart = params.z * abs(origin.z);
	minStart = clamp(minStart, MIN_RAY_START_ABS, maxDist * MIN_RAY_START_MAX_FRACTION);
	// Never use a depth tolerance narrower than one coarse march interval.
	float thickness = max(params.y, maxDist / float(steps));

	// lastMiss and hitP form the bracket later refined by binary search.
	vec3 lastMiss = origin + R * minStart;
	vec3 hitP = lastMiss;
	vec2 hitUV = uv;
	float hitT = minStart;
	bool hit = false;

	for (int i = 1; i <= MAX_STEPS; ++i)
	{
		if (i > steps)
		{
			break;
		}
		float t = mix(minStart, maxDist, float(i) / float(steps));
		vec3 p = origin + R * t;
		vec4 clip = projectionMatrix * vec4(p, 1.0);
		clip.xyz /= clip.w;
		vec2 suv = clipToUV(clip);
		if (suv.x < 0.0 || suv.y < 0.0 || suv.x > prepassUvScaleClamp.z || suv.y > prepassUvScaleClamp.w)
		{
			break;
		}
		suv = clamp(suv, vec2(0.0), prepassUvScaleClamp.zw);
		float sd = texture(depthTexture, suv).r;
		if (sd >= SKY_DEPTH_THRESHOLD)
		{
			lastMiss = p;
			continue;
		}
		vec3 hitPos = wzGetViewPosition(suv, sd, invProjectionMatrix);
		float rayCamDist = length(p);
		float surfCamDist = length(hitPos);
		// A hit occurs when the marched ray has just crossed behind scene depth,
		// but remains close enough to reject unrelated geometry behind it.
		if (surfCamDist < rayCamDist && (rayCamDist - surfCamDist) < thickness)
		{
			hit = true;
			hitP = p;
			hitUV = suv;
			hitT = t;
			break;
		}
		lastMiss = p;
	}

	if (!hit)
	{
		// Screen-space color cannot supply sky that is behind the camera or off
		// the framebuffer. A miss looks up the same 2D skybox the ScenePass uses.
		// Keep miss confidence below nearby geometry hits so the blur does not
		// wash units into the sky-colored ripples.
		float ndotv = clamp(dot(N, -V), 0.0, 1.0);
		float confidence = ssrWeight * mix(MISS_CONFIDENCE_MIN, MISS_CONFIDENCE_MAX, ndotv);
		if (skyboxAvailable < 0.5)
		{
			FragColor = vec4(0.0);
			return;
		}
		FragColor = vec4(wzSampleSkyRadiance(R), confidence);
		return;
	}

	// Refine the coarse first crossing without increasing the primary step count.
	for (int b = 0; b < BINARY_SEARCH_STEPS; ++b)
	{
		vec3 mid = mix(lastMiss, hitP, 0.5);
		vec4 clip = projectionMatrix * vec4(mid, 1.0);
		clip.xyz /= clip.w;
		vec2 suv = clamp(clipToUV(clip), vec2(0.0), prepassUvScaleClamp.zw);
		float sd = texture(depthTexture, suv).r;
		if (sd >= SKY_DEPTH_THRESHOLD)
		{
			lastMiss = mid;
			continue;
		}
		vec3 hitPos = wzGetViewPosition(suv, sd, invProjectionMatrix);
		float rayCamDist = length(mid);
		float surfCamDist = length(hitPos);
		if (surfCamDist < rayCamDist && (rayCamDist - surfCamDist) < thickness)
		{
			hitP = mid;
			hitUV = suv;
			hitT = length(mid - origin);
		}
		else
		{
			lastMiss = mid;
		}
	}

	// Geometry hits need to outrank the water's own ripple albedo. Distance still
	// fades far hits; facing weight stays in compose so this alpha can stay high.
	float confidence = ssrWeight
		* mix(HIT_CONFIDENCE_MIN, HIT_CONFIDENCE_MAX, 1.0 - clamp(hitT / max(maxDist, UV_EPSILON), 0.0, 1.0))
		* edgeFade(hitUV, prepassUvScaleClamp.zw);

	vec2 sceneUv = clamp(hitUV / max(prepassUvScaleClamp.xy, vec2(UV_EPSILON)) * sceneUvScaleClamp.xy,
		vec2(0.0), sceneUvScaleClamp.zw);
	vec3 color = texture(sceneTexture, sceneUv).rgb;
	FragColor = vec4(color, confidence);
}
