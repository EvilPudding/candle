#include "common.glsl"

layout (location = 0) out vec4 FragColor;

#line 6

BUFFER {
	sampler2D depth;
	sampler2D nmr;
} gbuffer;

void main()
{
	vec3 c_pos_o = get_position(gbuffer.depth);
	vec3 c_nor = get_normal(gbuffer.nmr);

	float dist_to_eye = length(c_pos_o);
	FragColor.r = ambientOcclusion(gbuffer.depth,
			c_pos_o, c_nor, dist_to_eye);
}

// vim: set ft=c:
