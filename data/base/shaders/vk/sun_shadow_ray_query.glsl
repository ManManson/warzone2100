// Sun shadow visibility via inline ray query (VK only, WZ_ENABLE_SUN_SHADOW_RAY_QUERY)

#ifndef WZ_SUN_SHADOW_TLAS_DESCRIPTOR_SET
#define WZ_SUN_SHADOW_TLAS_DESCRIPTOR_SET 2
#endif

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_query : require

layout(set = WZ_SUN_SHADOW_TLAS_DESCRIPTOR_SET, binding = 0) uniform accelerationStructureEXT topLevelAS;

float sunShadowBias(float NdotL, float offset)
{
	return (sqrt(max(1.0 - NdotL * NdotL, 0.0)) / max(NdotL, 1e-4)) * offset;
}

float traceSunVisibility(vec3 origin, vec3 worldNormal, vec3 sunDir, float bias)
{
	const vec3 rayOrigin = origin + worldNormal * bias;
	const vec3 rayDir = normalize(sunDir);

	rayQueryEXT rq;
	rayQueryInitializeEXT(rq, topLevelAS,
		gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
		0xFF, rayOrigin, 0.001, rayDir, 1e5);

	while (rayQueryProceedEXT(rq)) { }

	return (rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionNoneEXT)
		? 1.0 : 0.0;
}
