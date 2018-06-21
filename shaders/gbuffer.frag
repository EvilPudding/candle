
layout (location = 0) out vec4 AlbedoColor;
layout (location = 1) out vec4 NRM; // normal_roughness_metalness

#include "common.frag"

void main()
{
	vec4 dif  = resolveProperty(albedo, texcoord);
	if(dif.a == 0.0f) discard;

	dif.rgb += poly_color;
	AlbedoColor = dif;

	NRM.b = resolveProperty(roughness, texcoord).r;
	NRM.a = resolveProperty(metalness, texcoord).r;

	NRM.rg = encode_normal(get_normal());
}

// vim: set ft=c:
