
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

uniform bool horizontal;

uniform float weight[6] = float[] (0.382925, 0.24173, 0.060598, 0.005977, 0.000229, 0.000003);

BUFFER {
	sampler2D color;
} buf;

void main()
{
	vec2 pp = pixel_pos();
	vec2 tex_offset = 1.0f / textureSize(buf.color, 0);
	vec3 c = textureLod(buf.color, pp, 0).rgb * weight[0];

	if(horizontal)
	{
		for(int i = 1; i < 6; ++i)
		{
			vec2 off = vec2(tex_offset.x * i, 0.0f);
			c += textureLod(buf.color, pp + off, 0).rgb * weight[i];
			c += textureLod(buf.color, pp - off, 0).rgb * weight[i];
		}
	}
	else
	{
		for(int i = 1; i < 6; ++i)
		{
			vec2 off = vec2(0.0f, tex_offset.y * i);
			c += textureLod(buf.color, pp + off, 0).rgb * weight[i];
			c += textureLod(buf.color, pp - off, 0).rgb * weight[i];
		}
	}
	FragColor = vec4(c, 0.4f);
}

// vim: set ft=c:
