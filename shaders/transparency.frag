
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;
uniform pass_t rendered;

void main()
{
	vec3 dif = get_diffuse(gbuffer);
	vec4 trans = get_transparency(gbuffer);
	vec3 nor = get_normal(gbuffer);

	vec3 c_pos = get_position(gbuffer);
	nor.z = 0;

	if(trans.a != 0.0f)
	{
		/* FragColor = vec4(nor, 1.0f); return; */
		float dist_to_eye = length(c_pos);
		/* vec3 hit = v_pos - nor / (dist_to_eye + 2.0); */
		vec3 hit = c_pos + nor * 0.08;

		/* float eta = 1.3f; */
		/* float N_dot_I = dot(nor, v_pos); */
		/* FragColor = vec4(vec3(N_dot_I), 1.0); return; */
		/* float k = 1.f - eta * eta * (1.f - N_dot_I * N_dot_I); */
		/* vec3 hit = eta * v_pos - (eta * N_dot_I + sqrt(k)) * nor; */

        vec4 projectedCoord     = camera.projection * vec4(hit, 1.0f);
        projectedCoord.xy      /= projectedCoord.w;
        projectedCoord.xy       = projectedCoord.xy * 0.5 + 0.5; 
		/* FragColor = vec4(projectedCoord.xy, 0.0, 1.0); */
		/* return; */

		/* pos.s = clamp(pos.s, 0.0, 1.0); */
		/* pos.t = clamp(pos.t, 0.0, 1.0); */

		vec3 color = pass_sample(rendered, projectedCoord.xy).rgb;

		FragColor = vec4(color * trans.rgb, 1.0);
	}
	else
	{
		FragColor = vec4(0.0);
	}
}

// vim: set ft=c:
