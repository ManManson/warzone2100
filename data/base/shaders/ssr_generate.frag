// Version directive is set by Warzone when loading the shader
// (This shader supports GLSL 1.40 - 1.50 core.)

layout(std140) uniform cbuffer {
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

uniform sampler2D depthTexture;
uniform sampler2D normalsTexture;
uniform sampler2D sceneTexture;

#if (!defined(GL_ES) && (__VERSION__ >= 130)) || (defined(GL_ES) && (__VERSION__ >= 300))
#define NEWGL
#else
#define texture(tex, uv) texture2D(tex, uv)
#endif

#ifdef NEWGL
in vec2 texCoords;
out vec4 FragColor;
#else
varying vec2 texCoords;
#endif

#include "view_position.glsl"

const float SKY_DEPTH_THRESHOLD = 0.9999;
const int MAX_STEPS = 64;

vec2 clipToUV(vec4 clip)
{
	return clip.xy * 0.5 + 0.5;
}

vec3 getViewNormal(vec2 uv)
{
	vec3 n = texture(normalsTexture, uv).xyz * 2.0 - 1.0;
	float len = length(n);
	if (len < 1e-5)
	{
		return vec3(0.0, 0.0, 1.0);
	}
	return n / len;
}

float edgeFade(vec2 uv, vec2 clampZW)
{
	vec2 n = uv / max(clampZW, vec2(1e-6));
	float fadeX = smoothstep(0.0, 0.05, uv.x) * smoothstep(1.0, 0.95, n.x);
	float fadeY = smoothstep(0.0, 0.05, uv.y) * smoothstep(1.0, 0.95, n.y);
	return fadeX * fadeY;
}

void writeColor(vec4 color)
{
	#ifdef NEWGL
	FragColor = color;
	#else
	gl_FragColor = color;
	#endif
}

void main()
{
	vec2 uv = clamp(texCoords * prepassUvScaleClamp.xy, vec2(0.0), prepassUvScaleClamp.zw);
	float depth = texture(depthTexture, uv).r;
	if (depth >= SKY_DEPTH_THRESHOLD)
	{
		writeColor(vec4(0.0));
		return;
	}

	float ssrWeight = 1.0 - texture(normalsTexture, uv).a;
	if (ssrWeight < 1e-3)
	{
		writeColor(vec4(0.0));
		return;
	}

	vec3 origin = wzGetViewPosition(uv, depth, invProjectionMatrix);
	vec3 N = getViewNormal(uv);
	vec3 V = normalize(origin);
	if (dot(N, -V) < 0.0)
	{
		writeColor(vec4(0.0));
		return;
	}

	vec3 R = reflect(V, N);
	float maxDist = max(params.x, 1.0);
	int steps = int(stepCount + 0.5);
	if (steps < 1)
	{
		steps = 1;
	}
	if (steps > MAX_STEPS)
	{
		steps = MAX_STEPS;
	}
	float minStart = params.z * abs(origin.z);
	minStart = clamp(minStart, 1.0, maxDist * 0.15);
	float thickness = max(params.y, maxDist / float(steps));

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
		writeColor(vec4(0.0));
		return;
	}

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

	float confidence = ssrWeight
		* (0.25 + 0.75 * (1.0 - clamp(hitT / max(maxDist, 1e-6), 0.0, 1.0)))
		* edgeFade(hitUV, prepassUvScaleClamp.zw)
		* (0.2 + 0.8 * clamp(dot(N, -V), 0.0, 1.0));

	vec2 sceneUv = clamp(hitUV / max(prepassUvScaleClamp.xy, vec2(1e-6)) * sceneUvScaleClamp.xy,
		vec2(0.0), sceneUvScaleClamp.zw);
	vec3 color = texture(sceneTexture, sceneUv).rgb;
	writeColor(vec4(color, confidence));
}
