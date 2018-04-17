
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} buf;

void main()
{
    FragColor = textureLod(buf.color, texcoord, 0);
}

// vim: set ft=c:
