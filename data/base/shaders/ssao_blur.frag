// Version directive is set by Warzone when loading the shader
// (This shader supports GLSL 1.20 - 1.50 core.)

uniform vec2 blurDirection;
uniform sampler2D occlusionTexture;

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

void main()
{
	float result = 0.0;
	result += texture(occlusionTexture, texCoords + blurDirection * 1.0).r;
	result += texture(occlusionTexture, texCoords + blurDirection * 2.0).r;
	result += texture(occlusionTexture, texCoords + blurDirection * 3.0).r;
	result += texture(occlusionTexture, texCoords + blurDirection * 4.0).r;
	result *= 0.25;

	#ifdef NEWGL
	FragColor = vec4(vec3(result), 1.0);
	#else
	gl_FragColor = vec4(vec3(result), 1.0);
	#endif
}
