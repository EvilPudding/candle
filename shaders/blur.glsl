
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

uniform bool horizontal;

uniform float weight[6] = float[] (0.382925, 0.24173, 0.060598, 0.005977, 0.000229, 0.000003);

BUFFER {
	sampler2D color;
} buf;

void main()
{
	ivec2 tc = ivec2(gl_FragCoord);
	vec3 c = texelFetch(buf.color, pp, 0).rgb * weight[0];

	if(horizontal)
	{
		for(int i = 1; i < 6; ++i)
		{
			ivec2 off = ivec2(i, 0.0);
			c += texelFetch(buf.color, pp + off, 0).rgb * weight[i];
			c += texelFetch(buf.color, pp - off, 0).rgb * weight[i];
		}
	}
	else
	{
		for(int i = 1; i < 6; ++i)
		{
			ivec2 off = ivec2(0.0, * i);
			c += texelFetch(buf.color, pp + off, 0).rgb * weight[i];
			c += texelFetch(buf.color, pp - off, 0).rgb * weight[i];
		}
	}
	FragColor = vec4(c, 0.4);
}

// vim: set ft=c:
