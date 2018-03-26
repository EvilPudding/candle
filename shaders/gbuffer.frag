
layout (location = 0) out vec4 DiffuseColor;
layout (location = 1) out vec4 SpecularColor;
layout (location = 2) out vec4 Transparency;
layout (location = 3) out vec3 PositionColor;
layout (location = 4) out vec2 Normal;

#include "common.frag"

void main()
{
	vec4 dif  = resolveProperty(diffuse, texcoord);
	if(dif.a == 0.0f) discard;

	DiffuseColor = dif;

	SpecularColor = resolveProperty(specular, texcoord) * 2;

	Transparency = resolveProperty(transparency, texcoord);

	PositionColor = vertex_position;

	Normal = encode_normal(get_normal());
}

// vim: set ft=c:
