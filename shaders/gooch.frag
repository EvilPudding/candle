
#include "common.frag"

layout (location = 0) out vec4 FragColor;

void main()
{
	vec3 WarmColor = vec3(0.6, 0.6, 0.0);
	vec3 CoolColor = vec3(0.0, 0.0, 0.6);
	float DiffuseWarm = 0.45;
	float DiffuseCool = 0.45; 

	vec3 normalColor = get_normal();

	vec3 light_dir = get_light_dir();

	vec3 reflect_dir = reflect(-light_dir, normalColor);

	vec3 eye_dir = get_eye_dir();

	float diffuseCoefficient = max(0.0, dot(normalColor, light_dir));
	vec3 dif  = resolveProperty(diffuse, texcoord).rgb;
	vec4 tempSpec = resolveProperty(specular, texcoord);
	vec3 specularColor = tempSpec.rgb * light_intensity;

	float specWidth = tempSpec.a * 32;

	vec3 kcool = min(CoolColor + DiffuseCool * dif, 1.0);
	vec3 kwarm = min(WarmColor + DiffuseWarm * dif, 1.0);
	vec3 kfinal = mix(kcool, kwarm, diffuseCoefficient);

	float spec = pow(max(dot(eye_dir, reflect_dir), 0.0), specWidth);

	/* FragColor = vec4(vec3(light_dir), 1.0); return; */
	FragColor = vec4(clamp(kfinal + spec, 0.0, 1.0), 1.0);
	/* FragColor = vec4(vec3(spec), 1.0); */
}

// vim: set ft=c:
