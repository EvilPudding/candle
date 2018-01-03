
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

uniform bool horizontal;

uniform float weight[6] = float[] (0.382925, 0.24173, 0.060598, 0.005977, 0.000229, 0.000003);



void main()
{
	vec2 tex_offset = 1.0f / textureSize(diffuse.texture, 0);
	vec3 result = textureLod(diffuse.texture, texcoord, 0).rgb * weight[0];

	if(horizontal)
	{
		for(int i = 1; i < 6; ++i)
		{
			vec2 off = vec2(tex_offset.x * i, 0.0f);
			result += textureLod(diffuse.texture, texcoord + off, 0).rgb * weight[i];
			result += textureLod(diffuse.texture, texcoord - off, 0).rgb * weight[i];
		}
	}
	else
	{
		for(int i = 1; i < 6; ++i)
		{
			vec2 off = vec2(0.0f, tex_offset.y * i);
			result += textureLod(diffuse.texture, texcoord + off, 0).rgb * weight[i];
			result += textureLod(diffuse.texture, texcoord - off, 0).rgb * weight[i];
		}
	}
	FragColor = vec4(result, 0.4);
}

// vim: set ft=c:
