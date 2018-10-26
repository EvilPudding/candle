
layout (location = 0) out vec2 ID;
layout (location = 1) out vec2 GeomID;

#include "common.glsl"

void main()
{
	vec4 dif  = resolveProperty(mat(albedo), texcoord);
	if(dif.a == 0.0f) discard;

	ID = vec2(float(id.x) / 255.0f, float(id.y) / 255.0f);

	GeomID = poly_id;
}

// vim: set ft=c:
