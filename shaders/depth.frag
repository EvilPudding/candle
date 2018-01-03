
/* uniform float far_plane; */
in vec3 Position_worldspace;
uniform vec3 probe_pos;
uniform vec3 camera_pos;

struct property_t
{
	float texture_blend;
	sampler2D texture;
	float texture_scale;
	vec4 color;
};

in vec3 LightDirection_tangentspace;

uniform vec3 ambient_light;
uniform property_t diffuse;
uniform property_t normal;
uniform property_t specular;
uniform float light_intensity;

float ambient = 0.20;
layout (location = 0) out vec4 FragColor;

void main()
{
	/* vec3 dif = diffuse.color.rgb; */

	/* vec3 normalColor = vec3(0.0, 0.0, 1.0); */
	/* vec3 l = normalize(LightDirection_tangentspace); */
	/* vec3 R = reflect(-l, normalColor); */
	/* float mul = max(ambient, light_intensity * dot(normalColor, l)); */
	/* vec3 color_lit = dif * mul; */
	float dist = length(probe_pos - Position_worldspace);
    FragColor = vec4(vec3(0.0), dist);
}  

// vim: set ft=c:
