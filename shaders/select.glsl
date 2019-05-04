#include "common.glsl"

layout (location = 0) out vec2 ID;
layout (location = 1) out vec2 GeomID;


void main(void)
{
	/* vec4 dif  = resolveProperty(mat(albedo), texcoord, true); */
	/* if(dif.a == 0.0) discard; */

	ID = vec2(float(id.x) / 255.0, float(id.y) / 255.0);

	GeomID = vec2(poly_id);
	/* GeomID = gl_PrimitiveID; */
}

// vim: set ft=c:
