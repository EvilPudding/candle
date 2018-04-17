
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

bool is_equal(vec2 a, vec2 b)
{
	a = round(a * 255); 
	b = round(b * 255); 

	return (b.x == a.x && b.y == a.y);
}

BUFFER {
	sampler2D id;
	sampler2D geomid;
} sbuffer;

void main()
{
	vec2 c;
	vec2 c2;

	c = textureLod(sbuffer.id, pixel_pos(), 0).rg;
	c2 = textureLod(sbuffer.geomid, pixel_pos(), 0).rg;

	float over = filtered(c, over_id);
	float selected = filtered(c, sel_id);

	float overp = filtered(c2, over_poly_id);

	const vec3 sel_color = vec3(0.03f, 0.05f, 0.1f);
	const vec3 sel_color2 = vec3(0.1f, 0.03f, 0.05f);
	const vec3 over_color = vec3(0.08);

	vec3 final = (sel_color * selected + over_color * over);
	if(is_equal(sel_id, c))
	{
		final += (sel_color2 * overp);
	}

	FragColor = vec4(final, 1.0f);
	/* FragColor = vec4(c * 30, 0.0f, 1.0f); */
}

// vim: set ft=c:
