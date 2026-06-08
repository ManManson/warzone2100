// Version directive is set by Warzone when loading the shader
// (This shader supports GLSL 1.20 - 1.50 core.)

uniform vec2 blurDirection;
uniform float depthThreshold;
uniform sampler2D occlusionTexture;
uniform sampler2D depthTexture;

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

const float SKY_DEPTH_THRESHOLD = 0.9999;

void main()
{
	float centerDepth = texture(depthTexture, texCoords).r;
	if (centerDepth >= SKY_DEPTH_THRESHOLD)
	{
		#ifdef NEWGL
		FragColor = vec4(1.0);
		#else
		gl_FragColor = vec4(1.0);
		#endif
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

	#ifdef NEWGL
	FragColor = vec4(vec3(result), 1.0);
	#else
	gl_FragColor = vec4(vec3(result), 1.0);
	#endif
}
