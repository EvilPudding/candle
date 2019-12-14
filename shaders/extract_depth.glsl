#include "common.glsl"

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D depth;
} depthbuffer;

uniform vec2 position;

void main(void)
{
	float d = texelFetch(depthbuffer.depth, ivec2(position), 0).r * 256.;
	FragColor = vec4(fract(d) * 256., floor(d), 0.0, 0.0) / 255.0;
}

// vim: set ft=c:
