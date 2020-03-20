#include "common.glsl"

layout (location = 0) out vec4 Alb;
layout (location = 1) out vec4 NN;
layout (location = 2) out vec4 MR;
layout (location = 3) out vec3 Emi;


void main()
{
	vec4 dif = textureLod(g_framebuffer, texcoord, 0.0);
	Alb = dif;
	if(dif.a < 0.7) discard;

	NN = vec4(encode_normal(get_normal(vec3(0.5, 0.5, 1.0))), 0.0, 1.0);
	MR = vec4(0.5, 0.5, 0.0, 1.0);
	Emi = vec3(0.0, 0.0, 0.0);
	/* Alb = vec4(0.0, 0.0, 0.0, 1.0); */
	/* Emi = dif.xyz; */
}

// vim: set ft=c:
