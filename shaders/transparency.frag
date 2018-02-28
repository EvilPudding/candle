
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;
uniform pass_t phong;

void main()
{
	float len = texture2D(gbuffer.depth, texcoord).r;

	if(gl_FragDepth < len)
		FragColor = vec4(1.0, 0.0, 1.0, 1.0);
	else
		FragColor = vec4(0.0, 0.0, 0.0, 0.0);
}

// vim: set ft=c:
