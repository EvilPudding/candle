
#include "common.glsl"

layout (location = 0) out vec4 FragColor;

void main()
{
	/* FragColor = ssr(albedo.texture); */
	FragColor = texture2D(mat(albedo).texture, texcoord);
}  

// vim: set ft=c:
