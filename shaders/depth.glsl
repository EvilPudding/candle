
#include "common.glsl"
/* uniform float far_plane; */

layout (location = 0) out vec4 FragColor;

void main(void)
{
	vec4 dif  = resolveProperty(mat(albedo), texcoord, true);
	if(dif.a < 0.7) discard;

	float dist = length(vertex_position);
    /* FragColor = vec4(texcoord, 0.0f, 1.0f); */
	/* glDepth */
	gl_FragDepth = dist / 90.0f;
}  

// vim: set ft=c:
