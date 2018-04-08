
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

	if(trans.r != 0.0f || trans.g != 0.0f || trans.b != 0.0f)
	{
		/* FragColor = vec4(nor, 1.0f); return; */
		float dist_to_eye = length(c_pos);

		vec2 coord = pixel_pos() + nor.xy * (trans.a * 0.03);

		vec3 color = //pass_sample(rendered, coord).rgb;
		textureLod(rendered.texture, coord.xy, trans.a * 3).rgb;
		/* vec3 color = vec3(c_pos.xy, 0.0f); */

		FragColor = vec4(color * trans.rgb, 1.0);
	}
	else
	{
		FragColor = pass_sample(rendered, pixel_pos());
	}
}

// vim: set ft=c:
