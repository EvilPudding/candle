
layout (location = 0) out vec4 FragColor;
#include "common.frag"

void main()
{
	vec4 dif = resolveProperty(albedo, texcoord);
	if(dif.a == 0.0f) discard;

	FragColor = dif;
}

// vim: set ft=c:
