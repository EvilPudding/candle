
#include "common.glsl"
/* uniform float far_plane; */

layout (location = 0) out vec4 FragColor;

void main()
{
	float dist = length(vertex_position);
    /* FragColor = vec4(vec3(0.0), unlinearize(gl_FragDepth)); */
    FragColor = vec4(0, 0, 0, dist);
}  

// vim: set ft=c:
