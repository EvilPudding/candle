
layout (location = 0) out vec4 FragColor;

#include "common.frag"

void main()
{
	vec3 c_pos_o = texture2D(gbuffer.position, texcoord).rgb;
	vec3 c_nor = decode_normal(texture2D(gbuffer.normal, texcoord).rg);

	float dist_to_eye = length(c_pos_o);
	FragColor = vec4(vec3(ambientOcclusion(c_pos_o, c_nor, dist_to_eye)), 1.0);

}

// vim: set ft=c:
