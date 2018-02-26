
layout (location = 0) out vec4 FragColor;

#include "common.frag"

void main()
{
	vec3 w_pos = texture2D(gbuffer.wposition, texcoord).rgb;
	vec3 nor = texture2D(gbuffer.normal, texcoord).rgb;

	vec3 c_pos = camera.pos - w_pos;
	float dist_to_eye = length(c_pos);
	FragColor = vec4(vec3(ambientOcclusion(w_pos, nor, dist_to_eye)), 1.0);

}

// vim: set ft=c:
