
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} buf;

void main()
{
    FragColor = texelFetch(buf.color, ivec2(gl_FragCoord), 0);
}

// vim: set ft=c:
