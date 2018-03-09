
#include "common.frag"

layout (location = 0) out vec4 FragColor;

void main()
{
	/* FragColor = ssr(diffuse.texture); */
	FragColor = texture2D(diffuse.texture, texcoord);
}  

// vim: set ft=c:
