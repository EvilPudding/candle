
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


void main()
{
	ivec2 fc = ivec2(gl_FragCoord.xy);
	vec4 cc = texelFetch(light.color, fc, 0);

	vec4 normal_metalic_roughness = texelFetch(gbuffer.nmr, fc, 0);
	vec4 albedo = texelFetch(gbuffer.albedo, fc, 0);
	vec3 nor = decode_normal(normal_metalic_roughness.rg);

	vec4 ssred = ssr2(gbuffer.depth, light.color, albedo,
			normal_metalic_roughness.ba, nor);

	/* FragColor = ssred; return; */

    /* FragColor = vec4(vec3(texelFetch(portal.depth, fc, 0).r), 1.0f); return; */

    cc.rgb *= texelFetch(ssao.occlusion, fc, 0).r;

	vec3 final = cc.rgb + ssred.rgb * ssred.a;

	/* FragColor = vec4(cc.xyz, 1.0f); return; */
	/* FragColor = vec4(ssred.rgb, 1.0f); return; */

	/* final = clamp(final * 1.6f - 0.10f, 0.0, 3.0); */
	final = final * pow(2.0f, camera(exposure));
	float dist = length(get_position(gbuffer.depth));

	final.b += (clamp(dist - 5, 0, 1)) / 70;

	FragColor = vec4(final, 1.0f);
}

// vim: set ft=c:
