
#include "common.glsl"

layout (location = 0) out vec4 FragColor;

uniform vec2 sel_id;
uniform vec2 context_id;
uniform vec3 context_pos;
uniform float context_phase;

#define EDIT_VERT	0
#define EDIT_EDGE	1
#define EDIT_FACE	2
#define EDIT_OBJECT	3

const vec2 zero = vec2(0.003922, 0.000000);

float filtered(vec2 c, vec2 fil)
{
	fil = round(fil * 255.0); 
	c = round(c * 255.0); 

	if(c.x != fil.x || c.y != fil.y)
	{
		return 0.0;
	}
	return 1.0;
}

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

BUFFER {
	sampler2D depth;
} gbuffer;

void main(void)
{
	vec2 c;

	float occlusion = textureLod(ssao.occlusion, pixel_pos(), 0.0).r;
	vec4 both = textureLod(sbuffer.ids, pixel_pos(), 0.0);
	vec3 c_pos = get_position(gbuffer.depth);
	vec3 w_pos = (camera(model) * vec4(c_pos, 1.0)).xyz;
	w_pos += 0.0001;
	c = both.rg;
	float ditherValue = ditherPattern[(int(gl_FragCoord.x) % 4) * 4 + (int(gl_FragCoord.y) % 4)];

	if (length(w_pos - context_pos) + ditherValue * 4.0 > pow(context_phase, 4.0) * 1000.0)
		discard;
	if (c.x == 0.0 && c.y == 0.0)
	{
		/* vec2 grid = abs(fract(w_pos.xz - 0.5) - 0.5) / fwidth(w_pos.xz); */
		/* float line = min(grid.x, grid.y); */
		vec3 grid = abs(fract(w_pos - 0.5) - 0.5) / fwidth(w_pos);
		float line = 1.0 - min(min(grid.x, grid.y), grid.z);

		float checker;
		if ((int(floor(w_pos.x) + floor(w_pos.y) + floor(w_pos.z)) & 1) == 0)
			checker = 1.0;
		else
			checker = 0.8;
		FragColor = vec4(vec3(occlusion) * checker * clamp(1.0 - line, 0.8, 1.0), 1.0);
		return;
	}
	discard;
}

// vim: set ft=c:
