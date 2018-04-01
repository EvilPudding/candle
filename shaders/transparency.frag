
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;
uniform pass_t rendered;

void main()
{
	vec3 dif = get_diffuse(gbuffer);
	vec4 trans = get_transparency(gbuffer);
	vec4 spec = get_specular(gbuffer);
	vec3 nor = get_normal(gbuffer);

	vec3 c_pos = get_position(gbuffer);

	if(trans.a != 0.0f)
	{
		/* FragColor = vec4(nor, 1.0f); return; */
		float dist_to_eye = length(c_pos);
		/* vec3 hit = v_pos - nor / (dist_to_eye + 2.0); */
		vec3 hit = c_pos + nor * 0.08;

		vec2 coord = pixel_pos() + nor.xy * 0.03;

		vec3 color = //pass_sample(rendered, coord).rgb;
		textureLod(rendered.texture, coord.xy, (1.0f - spec.a) * 3).rgb;
		/* vec3 color = vec3(c_pos.xy, 0.0f); */

		FragColor = vec4(color * trans.rgb, 1.0);
	}
	else
	{
		FragColor = vec4(0.0);
	}
}

// vim: set ft=c:
