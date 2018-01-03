
#include "common.frag"

layout (location = 0) out vec4 FragColor;

void main()
{
	FragColor = vec4(vec3(texture2D(wposition.texture, texcoord) / 12.0), 1.0);
}  

// vim: set ft=c:
