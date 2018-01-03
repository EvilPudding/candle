
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

void main()
{
	float brightPassThreshold = 1.0f;

	vec3 luminanceVector = vec3(0.2125, 0.7154, 0.0721);
    vec4 c = textureLod(diffuse.texture, texcoord, 0);

    float luminance = dot(luminanceVector, c.xyz);
    luminance = max(0.0, luminance - brightPassThreshold);
    c.xyz *= sign(luminance);
    c.a = 1.0;

    FragColor = c;
}

// vim: set ft=c:
