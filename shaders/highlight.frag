
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

uniform vec2 over_id;
uniform vec2 sel_id;
uniform vec2 over_poly_id;
uniform int mode;

#define EDIT_VERT	0
#define EDIT_EDGE	1
#define EDIT_FACE	2
#define EDIT_OBJECT	3


float filtered(vec2 c, vec2 fil)
{
	fil = round(fil * 255); 
	c = round(c * 255); 

	if(c.x != fil.x || c.y != fil.y)
	{
		return 0;
	}
	return 1;
}

uniform pass_t final;

void main()
{
	/* vec2 tex_offset = 1.0f / textureSize(gbuffer.id, 0); */
	vec2 c;
	vec2 c2;

	c = get_id(gbuffer);
	c2 = get_geomid(gbuffer);

	float over = filtered(c, over_id);
	float selected = filtered(c, sel_id);

	float overp = filtered(c2, over_poly_id);

	const vec3 sel_color = vec3(0.03f, 0.05f, 0.1f);
	const vec3 sel_color2 = vec3(0.1f, 0.03f, 0.05f);
	const vec3 over_color = vec3(0.08);

	vec4 color = pass_sample(final, texcoord);

	vec3 final = color.rgb + (sel_color * selected + over_color * over);
	final += (sel_color2 * overp);

	FragColor = vec4(final, 1.0f);
	/* FragColor = vec4(c * 30, 0.0f, 1.0f); */
}

// vim: set ft=c:
