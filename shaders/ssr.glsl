
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D depth;
	sampler2D albedo;
	sampler2D nn;
	sampler2D mr;
	sampler2D emissive;
} gbuffer;

BUFFER {
	sampler2D occlusion;
} ssao;

BUFFER {
	sampler2D color;
} light;

BUFFER {
	sampler2D color;
} refr;

BUFFER {
	sampler2D color;
} volum;

uniform float ssr_power;
uniform float ssao_power;

vec4 upsample()
{
	ivec2 fc = ivec2(gl_FragCoord.xy);

	vec3 volumetric = vec3(0.0);
	float ssao_factor = 0.0;
	float totalWeight = 0.0;

	// Select the closest downscaled pixels.

	int xOffset = fc.x % 2 == 0 ? -1 : 1;
	int yOffset = fc.y % 2 == 0 ? -1 : 1;

	ivec2 offsets[] = ivec2[](
			ivec2(0, 0),
			ivec2(0, yOffset),
			ivec2(xOffset, 0),
			ivec2(xOffset, yOffset));

	float frag_depth = linearize(texelFetch(gbuffer.depth, fc, 0).r);
	ivec2 dfc = fc / 2;

	for (int i = 0; i < 4; i ++)
	{
		ivec2 coord = dfc + offsets[i];
		vec3 downscaledVolum = texelFetch(volum.color, coord, 0).rgb;
		float downscaledDepth = linearize(texelFetch(gbuffer.depth, coord * 2, 0).r);

		float currentWeight = max(0.0, 1.0f - 0.5 * abs(downscaledDepth - frag_depth));
		if (ssao_power > 0.05)
		{
			vec2 sampled = texelFetch(ssao.occlusion, coord, 0).rg;
			float downscaledSsao = sampled.r;
			ssao_factor += (1.0f - downscaledSsao) * currentWeight;
		}
		/* float currentWeight = abs(downscaledDepth - frag_depth) > 0.3 ? 0.0 : 1.0; */

		volumetric += downscaledVolum * currentWeight;
		totalWeight += currentWeight;

	}

	const float epsilon = 0.0001f;
	float factor = totalWeight + epsilon;
	return vec4(volumetric, factor - ssao_factor * ssao_power) / factor;
}

void main(void)
{
	ivec2 fc = ivec2(gl_FragCoord.xy);
	vec4 cc = texelFetch(light.color, fc, 0);

	vec2 normal = texelFetch(gbuffer.nn, fc, 0).rg;
	vec2 metalic_roughness = texelFetch(gbuffer.mr, fc, 0).rg;
	vec4 albedo = texelFetch(gbuffer.albedo, fc, 0);
	vec3 emissive = texelFetch(gbuffer.emissive, fc, 0).rgb;
	vec3 nor = decode_normal(normal);

	vec4 ssred = ssr_power > 0.05 ? ssr2(gbuffer.depth, refr.color, albedo,
			metalic_roughness, nor) * 1.5 : vec4(0.0);

	/* FragColor = ssred; return; */

    /* FragColor = vec4(vec3(texelFetch(portal.depth, fc, 0).r), 1.0); return; */

	float frag_depth = linearize(texelFetch(gbuffer.depth, fc, 0).r);

	/* vec2 pos = fract(gl_FragCoord.xy / 4.); */
	/* ivec2 sc =  fc / 4; */
	/* vec2 p00 = texelFetch(ssao.occlusion, (sc + ivec2(0, 0)), 0).rg; */
	/* vec2 p10 = texelFetch(ssao.occlusion, (sc + ivec2(1, 0)), 0).rg; */
	/* vec2 p01 = texelFetch(ssao.occlusion, (sc + ivec2(0, 1)), 0).rg; */
	/* vec2 p11 = texelFetch(ssao.occlusion, (sc + ivec2(1, 1)), 0).rg; */
	/* float err00 = abs(linearize(p00.g) - frag_depth); */
	/* float err10 = abs(linearize(p10.g) - frag_depth); */
	/* float err01 = abs(linearize(p01.g) - frag_depth); */
	/* float err11 = abs(linearize(p11.g) - frag_depth); */

	/* const float max_err = 0.01; */
	/* if (err00 > max_err) */
	/* { */
	/* 	if (err01 < max_err) */
	/* 		p00.r = p01.r; */
	/* 	else if (err10 < max_err) */
	/* 		p00.r = p10.r; */
	/* } */
	/* if (err11 > max_err) */
	/* { */
	/* 	if (err01 < max_err) */
	/* 		p11.r = p01.r; */
	/* 	else if (err10 < max_err) */
	/* 		p11.r = p10.r; */
	/* } */
	/* if (err01 > max_err) */
	/* { */
	/* 	if (err11 < max_err) */
	/* 		p01.r = p01.r; */
	/* 	else if (err00 < max_err) */
	/* 		p01.r = p00.r; */
	/* } */
	/* if (err10 > max_err) */
	/* { */
	/* 	if (err11 < max_err) */
	/* 		p10.r = p01.r; */
	/* 	else if (err00 < max_err) */
	/* 		p10.r = p00.r; */
	/* } */

	/* float h0 = mix(p00.r, p10.r, pos.x); */
	/* float h1 = mix(p01.r, p11.r, pos.x); */
	/* float ssao_interpol = mix(h0, h1, pos.y); */
	vec4 volum_ssao = upsample();
	/* FragColor = vec4(vec3(ssao_interpol), 1.0); */
	/* return; */

    cc.rgb *= volum_ssao.w;

	vec3 final = cc.rgb + ssred.rgb * (ssred.a * ssr_power) + emissive + volum_ssao.xyz;


	/* FragColor = vec4(cc.xyz, 1.0); return; */
	/* FragColor = vec4(ssred.rgb, 1.0); return; */

	/* final = clamp(final * 1.6 - 0.10, 0.0, 3.0); */
	final = final * pow(2.0, camera(exposure));
	float dist = length(get_position(gbuffer.depth));

	/* final.b += (clamp(dist - 5.0, 0.0, 1.0)) / 70.0; */

	FragColor = vec4(final, 1.0);
    /* FragColor.xyz = vec3(texelFetch(ssao.occlusion, fc, 0).r); */
}

// vim: set ft=c:
