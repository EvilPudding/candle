
#include "common.glsl"
/* uniform float far_plane; */

layout (location = 0) out vec4 FragColor;

void main()
{
	float dist = length(vertex_position);
    FragColor = vec4(0, 0, 0, dist);

}  

// vim: set ft=c:
