#ifdef DEBUG_STEERING

// Optional extra feather distance in world units; keep small for sharper edges.
const float STEERING_RING_FEATHER = 0.5;

// Controls how steep the ring falloff is. Larger values => crisper ring.
const float STEERING_RING_SHARPNESS = 4.0;

// Signed-distance-like evaluation of a thin ring band at (center, radius).
// Returns 1.0 at the ring centerline (dist == radius) and smoothly fades
// to 0.0 both inward and outward over a small band around the radius.
float evalRingBand(vec2 p, vec2 center, float radius, float halfWidth)
{
	float dist = length(p - center);
	float d = abs(dist - radius);
	// Keep the effective fade band tight for a sharper look.
	float fadeRange = max(halfWidth, 0.5) + STEERING_RING_FEATHER;
	float t = clamp(d / fadeRange, 0.0, 1.0);
	// Use a powered falloff for steeper edges: alpha = (1 - t)^sharpness.
	float alpha = pow(1.0 - t, STEERING_RING_SHARPNESS);
	return clamp(alpha, 0.0, 1.0);
}

// Evaluate all steering rings at world position and return blended color.
vec4 evalSteeringRings(vec3 worldPos, out float coverageOut)
{
	coverageOut = 0.0;
	vec4 accum = vec4(0.0);
	if (steeringDebugRingCount <= 0)
	{
		return accum;
	}
	// worldPos.xz = (vertex.x, vertex.z) in world units; center/radius from C++ in same units
	vec2 p = worldPos.xz;
	for (int i = 0; i < steeringDebugRingCount && i < 128; ++i)
	{
		// Read ivec4 explicitly and convert to float to avoid driver-dependent ivec4->vec4 interpretation
		ivec4 idata = steeringDebugRingsData[i];
		vec2 center = vec2(float(idata.x), float(idata.y));
		float radius = float(idata.z);
		float halfWidth = float(idata.w) * 0.5;

		float alpha = evalRingBand(p, center, radius, halfWidth);
		if (alpha <= 0.0)
		{
			continue;
		}

		// Interpret steeringDebugRingsColor as straight (non-premultiplied) RGBA in 0–1.
		// Convert to premultiplied RGBA after modulating with the SDF alpha.
		vec4 baseColor = steeringDebugRingsColor[i];
		float colAlpha = baseColor.a * alpha;
		vec3 colRGB = baseColor.rgb * colAlpha;

		// Premultiplied-alpha "over" compositing
		accum.rgb = accum.rgb + colRGB * (1.0 - accum.a);
		accum.a = clamp(accum.a + colAlpha * (1.0 - accum.a), 0.0, 1.0);
		coverageOut = max(coverageOut, alpha);
	}
	return accum;
}
#endif // DEBUG_STEERING
