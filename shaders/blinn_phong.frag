
#include "common.frag"

layout (location = 0) out vec4 FragColor;

void main()
{
	vec3 dif  = resolveProperty(diffuse, texcoord).rgb;

	float noi = (texture(perlin_map, (worldspace_position + 1) / 14).r);

	dif = dif * (1 - (noi / 10.0));
	vec3 color = ambient * dif;

	if(light_intensity > 0.01)
	{
		vec3 vec = light_pos - worldspace_position;
		float point_to_light = length(vec);
		float sd = get_shadow(vec, point_to_light);
		if(sd < 0.95)
		{
			/* float specul_smudge = clamp(1.0 - pow(noi, 5), 0.5, 1.0); */
			float specul_smudge = 1.0;

			float lightAtt = 0.01;

			vec3 light_dir = get_light_dir();
			vec3 normalColor = get_normal();

			float diffuseCoefficient = max(0.0, dot(normalColor, light_dir));
			vec3 frag_diffuse = diffuseCoefficient * dif;

			float attenuation = 1.0 / (1.0 + lightAtt * pow(point_to_light, 2));

			vec3 color_lit = attenuation * frag_diffuse;

			if(specul_smudge > 0.0 && diffuseCoefficient > 0.005 && attenuation > 0.01)
			{
				vec3 half_dir = get_half_dir();

				float specularCoefficient = 0.0;
				vec4 tempSpec = resolveProperty(specular, texcoord);
				vec3 specularColor = tempSpec.rgb * light_intensity * specul_smudge;
				float specWidth = tempSpec.a;

				specularCoefficient = pow(max(dot(half_dir, normalColor), 0.0), specWidth * 40);
				vec3 frag_specular = specularCoefficient * specularColor;
				color_lit += attenuation * frag_specular;
				/* FragColor = vec4(vec3(specularCoefficient), 1.0); return; */
			}
			color += color_lit * (1.0 - sd);

		}
	}
	vec3 gamma = vec3(1.2);

	FragColor = vec4(pow(color, gamma), 1.0);

}

// vim: set ft=c:
