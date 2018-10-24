
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} buf;

void main()
{
	vec4 tex = texture(buf.color, pixel_pos(), 0);
	if(tex.a == 0) discard;

	FragColor = tex;
}

// vim: set ft=c:
