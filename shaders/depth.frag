
#include "common.frag"
/* uniform float far_plane; */
uniform vec3 probe_pos;


layout (location = 0) out vec4 FragColor;

void main()
{
	vec3 pp = (camera.view * vec4(probe_pos, 1.0f)).xyz;
	float dist = length(pp - vertex_position);
    /* FragColor = vec4(vec3(0.0, 1.0, 0.0f), dist); */
    FragColor = vec4(vec3(0.0), dist);
}  

// vim: set ft=c:
