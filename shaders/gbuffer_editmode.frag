layout (location = 0) out vec4 DiffuseColor;
layout (location = 1) out vec4 SpecularColor;
layout (location = 2) out vec4 Transparency;
layout (location = 3) out vec3 PositionColor;
layout (location = 4) out vec2 ID;
layout (location = 5) out vec2 Normal;

#include "common.frag"

uniform vec2 over_id;
uniform vec2 sel_id;
uniform int mode;

#define EDIT_OBJECT 0
#define EDIT_MESH 1

void main()
{
	vec4 dif  = resolveProperty(diffuse, texcoord);
	if(dif.a == 0.0f) discard;

	/* float noi = (textureLod(perlin_map, (worldspace_position + 1) / 14, 0).r) * 4 - 3.5f; */

	/* dif = vec4(dif.rgb * (1 - (noi / 10.0)), dif.a); */

	DiffuseColor = dif;
	SpecularColor = resolveProperty(specular, texcoord) * 2;
	/* SpecularColor.a *= 1.0 - clamp(abs(noi * n.y), 0.0f, 1.0f); */
	/* DiffuseColor = vec4(vec3(SpecularColor.a), 1.0f); */

	Normal = encode_normal(get_normal());

	PositionColor = c_position;

	ID = vec2(1000000.0f);
	/* ID = vec4(object_id.x, object_id.y, poly_id.x, poly_id.y); */
	if(mode == EDIT_MESH)
	{
		vec2 c = abs(sel_id - object_id);
		if(c.x < 0.000001 || c.y < 0.000001)
		{
			ID = poly_id;
		}
	}
	else
	{
		ID = object_id;
	}

	/* float up = max(n.y, 0.0); */
	/* DiffuseColor = vec4(vec3(up), 1.0); */
	Transparency = resolveProperty(transparency, texcoord);

	/* float mipmapLevel = textureQueryLod(diffuse.texture, texcoord).x; */
	/* Transparency = vec4(vec3(mipmapLevel/10), 1.0f); return; */

}

// vim: set ft=c:
