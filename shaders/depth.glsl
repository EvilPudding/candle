
#include "common.glsl"
/* uniform float far_plane; */

layout (location = 0) out vec4 FragColor;

void main(void)
{
	vec4 dif  = resolveProperty(mat(albedo), texcoord, true);
	if(dif.a < 0.7) discard;

	/* FragColor = encode_float_rgba(dot(vertex_position, vertex_position)); */
	FragColor = encode_float_rgba(length(vertex_position));
}  

// vim: set ft=c:
