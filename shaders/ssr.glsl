
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D depth;
	sampler2D albedo;
	sampler2D nmr;
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

void main()
{
	ivec2 fc = ivec2(gl_FragCoord.xy);
	vec4 cc = texelFetch(light.color, fc, 0);

	vec4 normal_metalic_roughness = texelFetch(gbuffer.nmr, fc, 0);
	vec4 albedo = texelFetch(gbuffer.albedo, fc, 0);
	vec3 nor = decode_normal(normal_metalic_roughness.rg);

	vec4 ssred = ssr2(gbuffer.depth, refr.color, albedo,
			normal_metalic_roughness.ba, nor);

	/* FragColor = ssred; return; */

    /* FragColor = vec4(vec3(texelFetch(portal.depth, fc, 0).r), 1.0); return; */

    /* cc.rgb *= texelFetch(ssao.occlusion, fc, 0).r; */
	cc.rgb *= texelFetch(ssao.occlusion, fc, 0).r;

	vec3 final = cc.rgb + ssred.rgb * ssred.a;

	/* FragColor = vec4(cc.xyz, 1.0); return; */
	/* FragColor = vec4(ssred.rgb, 1.0); return; */

	/* final = clamp(final * 1.6 - 0.10, 0.0, 3.0); */
	final = final * pow(2.0, camera(exposure));
	float dist = length(get_position(gbuffer.depth));

	final.b += (clamp(dist - 5.0, 0.0, 1.0)) / 70.0;

	FragColor = vec4(final, 1.0);
}

// vim: set ft=c:
