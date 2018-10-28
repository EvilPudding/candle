
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} buf;

uniform int level;

void main()
{
	/* FragColor = vec4(texcoord, 0, 1); */
	/* return; */
	vec2 pp = gl_FragCoord.xy * (vec2(textureSize(buf.color, 0)) / screen_size);
	vec4 tex = texelFetch(buf.color, ivec2(pp), 0);
	/* if(tex.a == 0) discard; */

	FragColor = tex;
}

// vim: set ft=c:
