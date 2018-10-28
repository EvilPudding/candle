
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} buf;

uniform int level;

void main()
{
	vec4 tex = texelFetch(buf.color, ivec2(gl_FragCoord.xy), level);
	if(tex.a == 0) discard;

	FragColor = tex;
}

// vim: set ft=c:
