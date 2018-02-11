
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

uniform bool horizontal;

uniform float weight[3] = float[] (0.38774, 0.24477, 0.06136);

uniform vec3 id_filter;

vec3 get_unfiltered(vec2 offset)
{
	return textureLod(gbuffer.id, offset, 0).rgb;
}

vec3 get_filtered(vec2 offset)
{
	vec3 c = textureLod(gbuffer.id, offset, 0).rgb;
	if(id_filter == c)
	{
		return c;
	}
	return vec3(0.0f);
}

void main()
{
	vec2 tex_offset = 1.0f / textureSize(gbuffer.id, 0);
	vec3 c;

	if(horizontal)
	{
		c = get_filtered(texcoord).rgb * weight[0];
		for(int i = 1; i < 3; ++i)
		{
			vec2 off = vec2(tex_offset.x * i, 0.0f);
			c += get_filtered(texcoord + off) * weight[i];
			c += get_filtered(texcoord - off) * weight[i];
		}
	}
	else
	{
		c = get_unfiltered(texcoord).rgb * weight[0];
		for(int i = 1; i < 3; ++i)
		{
			vec2 off = vec2(0.0f, tex_offset.y * i);
			c += get_unfiltered(texcoord + off) * weight[i];
			c += get_unfiltered(texcoord - off) * weight[i];
		}
	}
	FragColor = vec4(c, 0.4);
}

// vim: set ft=c:
