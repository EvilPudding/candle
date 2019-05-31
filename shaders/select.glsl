#include "common.glsl"

layout (location = 0) out vec4 IDS;
layout (location = 1) out vec2 GeomID;


void main(void)
{
	/* vec4 dif  = resolveProperty(mat(albedo), texcoord, true); */
	/* if(dif.a == 0.0) discard; */

	IDS.xy = vec2(float(id.x) / 255.0, float(id.y) / 255.0);
	IDS.zw = vec2(poly_id);
	/* GeomID = gl_PrimitiveID; */
}

// vim: set ft=c:
