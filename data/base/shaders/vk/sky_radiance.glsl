// Direction-indexed lookup of the 2D skybox texpage.
// Uses viewToSkyLocal, skyFogColor, and skyboxTexture from the including shader.
// Matches pie_DrawSkybox: four walls share the same strip (u 0..2 per 90 deg),
// v follows the mesh bands (top=0.01, middle=0.85, baseline=0.99).

vec3 wzSampleSkyRadiance(vec3 viewDir)
{
	vec3 d = mat3(viewToSkyLocal) * viewDir;
	float len = length(d);
	if (len < 1e-5)
	{
		return skyFogColor.rgb;
	}
	d /= len;

	const float PI = 3.14159265;
	float u = fract((atan(d.x, d.z) + PI) / (0.5 * PI)) * 2.0;

	vec3 ad = abs(d);
	vec3 p = d / max(max(ad.x, max(ad.y, ad.z)), 1e-6);
	float y = p.y;
	float v;
	if (y > 0.15)
	{
		v = mix(0.85, 0.01, clamp((y - 0.15) / 0.85, 0.0, 1.0));
	}
	else
	{
		v = mix(0.99, 0.85, clamp((y + 0.45) / 0.60, 0.0, 1.0));
	}

	vec3 color = texture(skyboxTexture, vec2(u, v)).rgb;
	if (skyFogColor.a > 0.5 && y < 0.5)
	{
		color = skyFogColor.rgb;
	}
	return color;
}
