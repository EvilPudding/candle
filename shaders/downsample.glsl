#include "common.glsl"

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} buf;

uniform int level;

void main(void)
{
	/* FragColor = vec4(texcoord, 0, 1); */
	/* return; */
	/* vec2 pp = vec2(gl_FragCoord.xy) * (vec2(textureSize(buf.color, level)) / screen_size); */
	vec2 pp = vec2(gl_FragCoord.xy) * 2.0;
	vec4 tex = vec4(0.0);
	ivec2 iv;
	for(iv.x = 0; iv.x < 2; iv.x++)
		for(iv.y = 0; iv.y < 2; iv.y++)
			tex += texelFetch(buf.color, ivec2(pp) + iv, level);
	tex /= 4.0;
	/* if(tex.a == 0) discard; */

	FragColor = tex;
}

// vim: set ft=c:
