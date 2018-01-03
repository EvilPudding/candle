
layout (location = 0) out vec4 FragColor;

#include "common.frag"

void main()
{
	if(light_intensity > 0.01)
	{
		vec3 w_pos = texture2D(g_wposition, texcoord).rgb;
		vec3 nor = texture2D(g_normal, texcoord).rgb;
		vec3 vec = light_pos - w_pos;
		float point_to_light = length(vec);

		vec3 c_pos = camera_pos - w_pos;
		float dist_to_eye = length(c_pos);
		float sd = get_shadow(vec, point_to_light, dist_to_eye);
		FragColor = vec4(vec3(1.0 - sd) * ambientOcclusion(w_pos, nor, dist_to_eye), 1.0);
		return;
	}
	FragColor = vec4(0.0, 0.0, 0.0, 1.0);

}

// vim: set ft=c:
