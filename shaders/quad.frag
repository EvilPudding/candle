
#include "common.frag"

layout (location = 0) out vec4 FragColor;

void main()
{
	/* FragColor = ssr(diffuse.texture); */
	FragColor = textureLod(diffuse.texture, texcoord, 0);
	/* FragColor = vec4(texcoord, 0.0f, 1.0f); */
}  

// vim: set ft=c:
