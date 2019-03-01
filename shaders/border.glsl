
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

uniform bool horizontal;

uniform vec2 sel_id;

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
	sampler2D depth;
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
		return 1.0;
	}
	return 0.0;
}

void main(void)
{
	ivec2 tc = ivec2(int(gl_FragCoord.x), int(gl_FragCoord.y));
	const float weight[6] = float[] (0.382925, 0.24173, 0.060598, 0.005977, 0.000229, 0.000003);


	float selected = is_selected(tc);

	const vec3 sel_color = vec3(0.8, 0.7, 0.2);

	float w;
	if(sel_id.x > 0.0 || sel_id.y > 0.0)
	{
		if(horizontal)
		{
			float sel_percent = selected * weight[0] * 2.0;
			float min_d = 2.0;
			float w;
			for(int i = 1; i < 6; ++i)
			{
				ivec2 off = ivec2(i, 0);

				w = is_selected(tc + off);
				sel_percent += w * weight[i];
				if(w > 0.0)
				{
					min_d = min(texelFetch(sbuffer.depth, tc + off, 0).r, min_d);
				}

				w = is_selected(tc - off);
				sel_percent += w * weight[i];
				if(w > 0.0)
				{
					min_d = min(texelFetch(sbuffer.depth, tc - off, 0).r, min_d);
				}
			}
			FragColor = vec4(sel_percent, min_d, 0.0, 0.0);
		}
		else if(selected < 0.5)
		{

			float sel_percent = texelFetch(tmp.color, tc, 0).r * weight[0];
			float min_d = texelFetch(tmp.color, tc, 0).g;
			for(int i = 1; i < 6; ++i)
			{
				ivec2 off = ivec2(0, i);

				w = texelFetch(tmp.color, tc + off, 0).r;
				sel_percent += w * weight[i];
				if(w > 0.0)
				{
					min_d = min(texelFetch(tmp.color, tc + off, 0).g, min_d);
				}

				w = texelFetch(tmp.color, tc - off, 0).r;
				sel_percent += w * weight[i];
				if(w > 0.0)
				{
					min_d = min(texelFetch(tmp.color, tc - off, 0).g, min_d);
				}


			}
			float depth = texelFetch(sbuffer.depth, tc, 0).r;
			FragColor = vec4((depth > min_d ? sel_color: 1.0 - sel_color) * 2.0, sel_percent);
		}
	}
}

// vim: set ft=c:
