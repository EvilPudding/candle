
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

uniform int distance;
uniform int level;

BUFFER {
	sampler2D color;
} buf;

vec3 fs(ivec2 coord)
{
	if(coord.x < 0) coord.x = 0;
	if(coord.y < 0) coord.y = 0;
	ivec2 s = textureSize(buf.color, level);
	if(coord.x >= s.x) coord.x = s.x - 1;
	if(coord.y >= s.y) coord.y = s.y - 1;
	return texelFetch(buf.color, coord, level).rgb;
}

vec3 fetch(ivec2 coord)
{
	return fs(coord).rgb +
		   fs(coord + ivec2(1, 0)) +
		   fs(coord + ivec2(1, 1)) +
		   fs(coord + ivec2(0, 1));
}

void main(void)
{
	ivec2 tc = ivec2(gl_FragCoord.xy);
	vec3 c = vec3(0.0);

	c += fetch(tc + ivec2(distance, distance));
	c += fetch(tc + ivec2(-distance - 1, -distance - 1));
	c += fetch(tc + ivec2(distance, -distance - 1));
	c += fetch(tc + ivec2(-distance - 1, distance));

	FragColor = vec4(c / 16.0, 1.0);
}

// vim: set ft=c:
