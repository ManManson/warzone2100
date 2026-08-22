#version 450

layout(std140, set = 0, binding = 0) uniform cbuffer {
	mat4 invProjectionMatrix;
	mat4 projectionMatrix;
	vec4 params;              // x=maxRayLength, y=thickness, z=minRayStart, w unused
	vec4 prepassUvScaleClamp; // xy scale, zw clamp
	vec4 sceneUvScaleClamp;
	float stepCount;
	float padding0;
	float padding1;
	float padding2;
};

layout(set = 1, binding = 0) uniform sampler2D depthTexture;
layout(set = 1, binding = 1) uniform sampler2D normalsTexture;
layout(set = 1, binding = 2) uniform sampler2D sceneTexture;

layout(location = 0) in vec2 texCoords;
layout(location = 0) out vec4 FragColor;

#include "view_position.glsl"

const float SKY_DEPTH_THRESHOLD = 0.9999;
const int MAX_STEPS = 64;

vec2 clipToUV(vec4 clip)
{
	return vec2(clip.x, -clip.y) * 0.5 + 0.5;
}

vec3 getViewNormal(vec2 uv)
{
	vec3 n = texture(normalsTexture, uv).xyz * 2.0 - 1.0;
	float len = length(n);
	// Empty or invalid prepass normals must not inject NaNs into the ray direction.
	if (len < 1e-5)
	{
		return vec3(0.0, 0.0, 1.0);
	}
	return n / len;
}

float edgeFade(vec2 uv, vec2 clampZW)
{
	// Screen-space rays cannot recover data beyond the rendered prepass extent.
	// Fade hits near that boundary instead of exposing a hard reflection cutoff.
	vec2 n = uv / max(clampZW, vec2(1e-6));
	float fadeX = smoothstep(0.0, 0.05, uv.x) * smoothstep(1.0, 0.95, n.x);
	float fadeY = smoothstep(0.0, 0.05, uv.y) * smoothstep(1.0, 0.95, n.y);
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
	if (ssrWeight < 1e-3)
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
	minStart = clamp(minStart, 1.0, maxDist * 0.15);
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
		FragColor = vec4(0.0);
		return;
	}

	// Refine the coarse first crossing without increasing the primary step count.
	for (int b = 0; b < 4; ++b)
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

	// Confidence combines material eligibility, ray length, screen-edge validity,
	// and grazing angle. The compose pass uses it as reflection opacity.
	float confidence = ssrWeight
		* (0.25 + 0.75 * (1.0 - clamp(hitT / max(maxDist, 1e-6), 0.0, 1.0)))
		* edgeFade(hitUV, prepassUvScaleClamp.zw)
		* (0.2 + 0.8 * clamp(dot(N, -V), 0.0, 1.0));

	// Convert from prepass allocation coordinates back through logical screen UV
	// into the populated extent of the opaque scene-color texture.
	vec2 sceneUv = clamp(hitUV / max(prepassUvScaleClamp.xy, vec2(1e-6)) * sceneUvScaleClamp.xy,
		vec2(0.0), sceneUvScaleClamp.zw);
	vec3 color = texture(sceneTexture, sceneUv).rgb;
	FragColor = vec4(color, confidence);
}
