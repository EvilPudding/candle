
#include "common.glsl"
/* uniform float far_plane; */

layout (location = 0) out vec4 FragColor;

void main()
{
	vec4 dif  = resolveProperty(mat(albedo), texcoord);
	if(dif.a < 0.7f) discard;

	float dist = length(vertex_position);
    FragColor = vec4(0, 0, 0, dist);

}  

// vim: set ft=c:
