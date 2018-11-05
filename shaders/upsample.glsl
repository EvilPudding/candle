
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} buf;

uniform int level;
uniform float alpha;

void main()
{
	/* FragColor = vec4(texcoord, 0, 1); */
	/* return; */
	vec4 tex;
	tex = texture(buf.color, pixel_pos(), level);
	/* if(tex.a == 0) discard; */
	tex.a = alpha;

	FragColor = tex;
}

// vim: set ft=c:
