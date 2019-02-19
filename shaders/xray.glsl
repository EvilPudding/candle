#include "common.frag"

layout (location = 0) out vec4 FragColor;

void main()
{
	vec4 dif = resolveProperty(albedo, texcoord, true);
	if(dif.a == 0.0) discard;

	FragColor = dif;
}

// vim: set ft=c:
