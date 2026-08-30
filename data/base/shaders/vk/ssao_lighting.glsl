#ifndef WZ_SSAO_LIGHTING_GLSL
#define WZ_SSAO_LIGHTING_GLSL

// Sample blurred AO in framebuffer UV (gl_FragCoord / viewport).
// Do not flip FragCoord.y -- same convention as point-light clipSpaceCoord.
vec2 wzSsaoUv()
{
	vec2 uv = gl_FragCoord.xy / vec2(float(viewportWidth), float(viewportHeight));
	return clamp(uv * ssaoUvScaleClamp.xy, vec2(0.0), ssaoUvScaleClamp.zw);
}

float wzSampleSsao()
{
	return texture(ssaoTexture, wzSsaoUv()).r;
}

vec3 wzApplySsaoToAmbient(vec3 ambientRgb)
{
	return ambientRgb * mix(1.0, wzSampleSsao(), ssaoIntensity);
}

#endif
