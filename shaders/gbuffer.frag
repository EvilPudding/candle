
layout (location = 0) out vec4 AlbedoColor;
layout (location = 1) out vec4 NMR; // normal_roughness_metalness

#include "common.frag"

void main()
{
	vec4 dif  = resolveProperty(albedo, texcoord);
	if(dif.a < 0.7f) discard;

	dif.rgb += poly_color;
	/* dif.rgb = TM[2] / 2.0f + 0.5f; */
	AlbedoColor = dif;

	NMR.rg = encode_normal(get_normal());

	NMR.b = resolveProperty(metalness, texcoord).r;
	NMR.a = resolveProperty(roughness, texcoord).r;

}

// vim: set ft=c:
