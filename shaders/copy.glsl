
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} buf;

void main()
{
    FragColor = texture(buf.color, pixel_pos(), 0);
}

// vim: set ft=c:
