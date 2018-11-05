
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} buf;

void main()
{
	float brightPassThreshold = 1.0;

	vec3 luminanceVector = vec3(0.2125, 0.7154, 0.0721);
	vec3 c = textureLod(buf.color, texcoord, 0).rgb;

    float luminance = dot(luminanceVector, c);
    luminance = max(0.0, luminance - brightPassThreshold);
    c *= sign(luminance);

    FragColor = vec4(c, 1.0);
}

// vim: set ft=c:
