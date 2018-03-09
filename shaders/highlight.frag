
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

uniform bool horizontal;

/* uniform float weight[3] = float[] (0.38774, 0.24477, 0.06136); */

uniform vec2 over_id;
uniform vec2 sel_id;
uniform bool mode;

vec4 get_unfiltered(vec2 offset)
{
	return textureLod(gbuffer.id, offset, 0);
}

float filtered(vec2 c, vec2 filter)
{

	c = abs(c.xy - filter);

	if(c.x > 0.0001 || c.y > 0.0001)
	{
		return 0;
	}
	return 1;
}

uniform pass_t final;

void main()
{
	/* vec2 tex_offset = 1.0f / textureSize(gbuffer.id, 0); */
	vec4 c = textureLod(gbuffer.id, texcoord, 0);
	vec2 mode_c = mode?c.zw:c.xy;

	float over = filtered(mode_c, over_id);
	float selected = filtered(mode_c, sel_id);

	const vec3 sel_color = vec3(0.03f, 0.05f, 0.1f);
	const vec3 over_color = vec3(0.08);

	vec4 color = pass_sample(final, texcoord);

	vec3 final = color.rgb + (sel_color * selected + over_color * over);


	FragColor = vec4(final, 1.0f);
	/* FragColor = vec4(mode_c * 30, 0.0f, 1.0f); */
}

// vim: set ft=c:
