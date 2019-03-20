#include "common.glsl"

layout (location = 0) out vec4 Coords;
layout (location = 1) out vec4 Mips;
layout (location = 2) out vec4 Mips2;

#define TC_MAX ( 64.0)
#define TC_MIN (-64.0)
#define NUM_COORDS 65536.0

vec2 tc_remap(vec2 tc)
{
	return floor((tc - TC_MIN) * (NUM_COORDS / (TC_MAX - TC_MIN)));
}

uniform bool transparent;
void main(void)
{
	if (transparent && (((int(gl_FragCoord.x) + int(gl_FragCoord.y)) & 1) == 0))
		discard;
	Mips.r = mip_map_level(texcoord * mat(albedo).scale * vec2(mat(albedo).size) * 0.05) / 8.0f;
	Mips.g = mip_map_level(texcoord * mat(roughness).scale * vec2(mat(roughness).size) * 0.05) / 8.0f;
	Mips.b = mip_map_level(texcoord * mat(metalness).scale * vec2(mat(metalness).size) * 0.05) / 8.0f;
	Mips.a = mip_map_level(texcoord * mat(transparency).scale * vec2(mat(transparency).size) * 0.05) / 8.0f;
	Mips2.r = mip_map_level(texcoord * mat(normal).scale * vec2(mat(normal).size) * 0.05) / 8.0f;
	Mips2.g = mip_map_level(texcoord * mat(emissive).scale * vec2(mat(emissive).size) * 0.05) / 8.0f;
	Mips2.b = float(matid) / 255.0;

	vec2 int_coords = tc_remap(texcoord);
	Coords = vec4(
			floor(int_coords.x / 256.0), mod(int_coords.x, 256.0),
			floor(int_coords.y / 256.0), mod(int_coords.y, 256.0)) / 255.0;
}

// vim: set ft=c:
