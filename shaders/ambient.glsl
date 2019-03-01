
#include "common.glsl"
/* uniform float far_plane; */
in vec3 Position_worldspace;

in vec3 LightDirection_tangentspace;
layout (location = 0) out vec4 FragColor;

void main(void)
{
	vec3 dif = resolveProperty(albedo, texcoord, true).rgb;

	/* vec3 normalColor = vec3(0.0, 0.0, 1.0); */
	/* vec3 l = normalize(LightDirection_tangentspace); */
	/* vec3 R = reflect(-l, normalColor); */
	/* float mul = max(ambient, light_intensity * dot(normalColor, l)); */
	/* vec3 color_lit = dif * mul; */
    FragColor = vec4(dif, 1.0);
}  

// vim: set ft=c:
