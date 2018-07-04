
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D depth;
	sampler2D nmr;
} gbuffer;

BUFFER {
	sampler2D occlusion;
} ssao;

BUFFER {
	sampler2D color;
} rendered;

void main()
{
	vec4 cc = textureLod(rendered.color, pixel_pos(), 0);

	vec4 normal_metalic_roughness = textureLod(gbuffer.nmr, pixel_pos(), 0);
	vec3 nor = decode_normal(normal_metalic_roughness.rg);
	float roughness = normal_metalic_roughness.a;

	vec4 ssred = ssr2(gbuffer.depth, rendered.color, roughness, nor);
	/* FragColor = ssred; return; */

    cc.rgb *= textureLod(ssao.occlusion, pixel_pos(), 0).r;

	vec3 final = cc.rgb + ssred.rgb * ssred.a;

	/* FragColor = vec4(cc.xyz, 1.0f); return; */
	/* FragColor = vec4(ssred.rgb, 1.0f); return; */

	/* final = clamp(final * 1.6f - 0.10f, 0.0, 3.0); */
	final = final * pow(2.0f, camera.exposure);
	float dist = length(get_position(gbuffer.depth));
	final.b += (clamp(dist - 5, 0, 1)) / 70;

	FragColor = vec4(final, 1.0f);
}

// vim: set ft=c:
