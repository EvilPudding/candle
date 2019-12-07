#include "common.glsl"

layout (location = 0) out vec4 Alb;
layout (location = 1) out vec4 NN;
layout (location = 2) out vec4 MR;
layout (location = 3) out vec3 Emi;


void main()
{
	vec4 dif = textureLod(g_framebuffer, texcoord, 0.0f);
	Alb = dif;
	if(dif.a < 0.7) discard;

	NN = vec4(encode_normal(get_normal(vec3(0.5f, 0.5f, 1.0f))), 0.0f, 1.0f);
	MR = vec4(0.5, 0.5, 0.0f, 1.0f);
	Emi = vec3(0.0f, 0.0f, 0.0f);
	Emi = dif.xyz;
}

// vim: set ft=c:
