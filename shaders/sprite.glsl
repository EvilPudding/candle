
#include "common.glsl"

layout (location = 0) out vec4 FragColor;

void main()
{
	/* FragColor = ssr(albedo.texture); */
	FragColor = resolveProperty(mat(albedo), texcoord, true);
}  

// vim: set ft=c:
