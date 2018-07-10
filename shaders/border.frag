
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

uniform bool horizontal;

uniform float weight[6] = float[] (0.382925, 0.24173, 0.060598, 0.005977, 0.000229, 0.000003);

uniform vec2 sel_id;

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

BUFFER {
	sampler2D color;
} tmp;

float is_selected(ivec2 coord)
{
	vec2 t = texelFetch(sbuffer.id, coord, 0).rg;
	float selected = filtered(t, sel_id);
	if(is_equal(sel_id, t))
	{
		return 1.0f;
	}
	return 0.0f;
}

void main()
{
	ivec2 tc = ivec2(int(gl_FragCoord.x), int(gl_FragCoord.y));

	float selected = is_selected(tc);

	const vec3 sel_color = vec3(0.8f, 0.7f, 0.2f);

	if(sel_id.x > 0.0f || sel_id.y > 0.0f)
	{
		if(horizontal)
		{
			float sel_percent = selected * weight[0] * 2;
			for(int i = 1; i < 6; ++i)
			{
				ivec2 off = ivec2(i, 0);

				float w = is_selected(tc + off);
				/* sel_percent += w; */
				sel_percent += w * weight[i] * 2;

				w = is_selected(tc - off);
				/* sel_percent += w; */
				sel_percent += w * weight[i] * 2;
			}
			FragColor = vec4(sel_percent, vec3(0.0f));
		}
		else if(selected < 0.5)
		{
			float sel_percent = texelFetch(tmp.color, tc, 0).r * weight[0] * 2;
			for(int i = 1; i < 6; ++i)
			{
				ivec2 off = ivec2(0, i);
				sel_percent += texelFetch(tmp.color, tc + off, 0).r * weight[i] * 2;
				sel_percent += texelFetch(tmp.color, tc - off, 0).r * weight[i] * 2;
			}
			FragColor = vec4(sel_color, sel_percent * 0.5);
		}
	}
}

// vim: set ft=c:
