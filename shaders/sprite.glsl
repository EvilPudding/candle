
#include "common.glsl"

layout (location = 0) out vec4 FragColor;

void main(void)
{
	/* FragColor = ssr(albedo.texture); */
	FragColor = resolveProperty(mat(albedo), texcoord, true);
}  

// vim: set ft=c:
