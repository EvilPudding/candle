
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

uniform vec2 over_id;
uniform vec2 sel_id;
uniform vec2 over_poly_id;
uniform vec2 context_id;
uniform int mode;
uniform vec3 context_pos;
uniform float context_phase;

#define EDIT_VERT	0
#define EDIT_EDGE	1
#define EDIT_FACE	2
#define EDIT_OBJECT	3

const vec2 zero = vec2(0.003922, 0.000000);

bool is_equal(vec2 a, vec2 b)
{
	a = round(a * 255.0); 
	b = round(b * 255.0); 

	return (b.x == a.x && b.y == a.y);
}

BUFFER {
	sampler2D ids;
} sbuffer;

BUFFER {
	sampler2D occlusion;
} ssao;

void main(void)
{
	vec2 c;
	vec2 c2;

	float occlusion = textureLod(ssao.occlusion, pixel_pos(), 0.0).r;
	vec4 both = textureLod(sbuffer.ids, pixel_pos(), 0.0);
	c = both.rg;
	c2 = both.ba;

	const vec3 over_poly_color = vec3(0.2, 0.02, 0.5);
	const vec3 over_color = vec3(0.05);

	vec3 final = vec3(0.0);
	/* if(sel_id.x > 0.0 || sel_id.y > 0.0) */
	/* { */
	/* 	final -= sel_color * (1.0 - selected); */
	/* } */
	if(is_equal(over_id, c))
	{
		if(mode != EDIT_OBJECT)
		{
			final += over_poly_color;
		}
		else
		{
			final += over_color;
		}
	}
	else
	{
	}

	FragColor = vec4(final, 1.0);
	/* FragColor = vec4(c * 30, 0.0, 1.0); */
}

// vim: set ft=c:
