
#include "common.frag"

layout (location = 0) out vec4 FragColor;

void main()
{
	FragColor = textureLod(albedo.texture, texcoord, 0);
}  

// vim: set ft=c:
