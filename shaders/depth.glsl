
#include "common.glsl"
/* uniform float far_plane; */

layout (location = 0) out vec4 FragColor;

void main(void)
{
	vec4 dif  = resolveProperty(mat(albedo), texcoord, true);
	if(dif.a < 0.7) discard;

	FragColor = encode_float_rgba(sqrt(dot(vertex_position, vertex_position)));
	/* float dist = length(vertex_position); */
	/* gl_FragDepth = dist / 90.0f; */
}  

// vim: set ft=c:
