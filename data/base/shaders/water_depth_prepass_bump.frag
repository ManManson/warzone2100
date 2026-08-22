// Version directive is set by Warzone when loading the shader
// (This shader supports GLSL 1.40 - 1.50 core.)

layout(std140) uniform cbuffer {
	mat4 ModelViewProjectionMatrix;
	mat4 ViewMatrix;
	float timeSec;
	float mipLoadBias;
	float pad0;
	float pad1;
};

uniform sampler2DArray tex_nm;

#if (!defined(GL_ES) && (__VERSION__ >= 130)) || (defined(GL_ES) && (__VERSION__ >= 300))
#define NEWGL
#define texture2DArray(tex,coord,bias) texture(tex,coord,bias)
in vec4 uv1_uv2;
out vec4 FragColor;
#else
#define texture(tex,uv,bias) texture2D(tex,uv,bias)
varying vec4 uv1_uv2;
#endif

#include "water_normals.glsl"

void main()
{
	vec3 modelN = wzWaterModelNormal(tex_nm, uv1_uv2.xy, uv1_uv2.zw, timeSec, mipLoadBias);
	vec3 viewN = normalize(mat3(ViewMatrix) * modelN);
	#ifdef NEWGL
	FragColor = vec4(viewN * 0.5 + 0.5, 0.0);
	#else
	gl_FragColor = vec4(viewN * 0.5 + 0.5, 0.0);
	#endif
}
