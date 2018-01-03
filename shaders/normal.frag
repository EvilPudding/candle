
#include "common.frag"

layout (location = 0) out vec4 FragColor;

void main()
{
	FragColor = vec4((texture2D(normal.texture, texcoord).rgb + 1.0) / 2.0, 1.0);
}  

// vim: set ft=c:
