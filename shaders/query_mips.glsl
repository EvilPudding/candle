#include "common.glsl"

layout (location = 0) out vec3 Coords;
layout (location = 1) out vec4 Mips;

void main()
{
	Mips.r = mip_map_level(texcoord * mat(albedo).scale * mat(albedo).size * 0.05);
	Mips.g = mip_map_level(texcoord * mat(normal).scale * mat(normal).size * 0.05);
	Mips.b = mip_map_level(texcoord * mat(metalness).scale * mat(metalness).size * 0.05);
	Mips.a = mip_map_level(texcoord * mat(roughness).scale * mat(roughness).size * 0.05);
	Coords = vec3(texcoord, matid);
}

// vim: set ft=c:
