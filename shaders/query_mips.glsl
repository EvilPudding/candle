#include "common.glsl"

layout (location = 0) out vec4 Coords;
layout (location = 1) out vec4 Mips;

uniform bool transparent;
void main(void)
{
	if (transparent && (((int(gl_FragCoord.x) + int(gl_FragCoord.y)) & 1) == 0))
		discard;
	Mips.r = mip_map_level(texcoord * mat(albedo).scale * vec2(mat(albedo).size) * 0.05) / 8.0f;
	Mips.g = mip_map_level(texcoord * mat(normal).scale * vec2(mat(normal).size) * 0.05) / 8.0f;
	Mips.b = mip_map_level(texcoord * mat(metalness).scale * vec2(mat(metalness).size) * 0.05) / 8.0f;
	Mips.a = mip_map_level(texcoord * mat(roughness).scale * vec2(mat(roughness).size) * 0.05) / 8.0f;
	Coords = vec4(texcoord / 16.0 + 0.5, float(matid) / 255.0, 0);
}

// vim: set ft=c:
