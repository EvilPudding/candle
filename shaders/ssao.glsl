
layout (location = 0) out vec4 FragColor;

#include "common.glsl"
#line 6

BUFFER {
	sampler2D depth;
	sampler2D nmr;
} gbuffer;

void main()
{
	vec3 c_pos_o = get_position(scene.camera, gbuffer.depth);
	vec3 c_nor = get_normal(gbuffer.nmr);

	float dist_to_eye = length(c_pos_o);
	FragColor.r = ambientOcclusion(scene.camera, gbuffer.depth,
			c_pos_o, c_nor, dist_to_eye);
}

// vim: set ft=c:
