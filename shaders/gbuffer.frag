
layout (location = 0) out vec4 DiffuseColor;
layout (location = 1) out vec4 SpecularColor;
layout (location = 2) out vec2 Normal;

#include "common.frag"

void main()
{
	vec4 dif  = resolveProperty(diffuse, texcoord);
	if(dif.a == 0.0f) discard;

	DiffuseColor = dif;

	SpecularColor = resolveProperty(specular, texcoord);

	Normal = encode_normal(get_normal());
}

// vim: set ft=c:
