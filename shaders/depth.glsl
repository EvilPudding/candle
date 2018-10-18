
#include "common.glsl"
/* uniform float far_plane; */

layout (location = 0) out vec4 FragColor;

void main()
{
	/* vec3 pp = (camera.view * vec4(probe_pos, 1.0f)).xyz; */
	/* float dist = length(pp - vertex_position); */
    FragColor = vec4(vec3(0.0), gl_FragDepth);
}  

// vim: set ft=c:
