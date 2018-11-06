#include "common.glsl"

layout (location = 0) out vec4 AlbedoColor;
layout (location = 1) out vec4 NMR; // normal_roughness_metalness


void main()
{
	vec4 dif  = resolveProperty(mat(albedo), texcoord);
	if(dif.a < 0.7) discard;

	dif.rgb += poly_color;
	/* dif.rgb = TM[2] / 2.0 + 0.5; */
	AlbedoColor = dif;
	AlbedoColor.a = receive_shadows > 0 ? 1.0 : 0.5;

	if(has_normals)
	{
		NMR.rg = encode_normal(get_normal());
	}
	else
	{
		NMR.rg = encode_normal(vec3(0, 1, 0));
	}

	NMR.b = resolveProperty(mat(metalness), texcoord).r;
	NMR.a = resolveProperty(mat(roughness), texcoord).r;

	/* AlbedoColor = scene.test_color; */
}

// vim: set ft=c:
