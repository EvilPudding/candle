
#include "common.glsl"

layout (location = 0) out vec4 FragColor;

uniform sampler2D tex;

void main(void)
{
	FragColor = textureLod(tex, texcoord, 0.0);
}  

// vim: set ft=c:
