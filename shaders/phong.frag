
#include "common.frag"
#line 5

layout (location = 0) out vec4 FragColor;
uniform pass_t ssao;

void main()
{
	vec3 dif = textureLod(gbuffer.diffuse, texcoord, 0).rgb;
	vec3 w_pos = textureLod(gbuffer.wposition, texcoord, 0).rgb;

	vec3 c_pos = camera_pos - w_pos;

	vec3 nor = textureLod(gbuffer.normal, texcoord, 0).rgb;

	float dist_to_eye = length(c_pos);
	/* FragColor = clamp(vec4((textureLod(gbuffer.normal, texcoord, 0).rgb+1.0f)/2, 1.0),0,1); return; */

	vec3 color = ambient * dif;
	if(light_intensity > 0.01)
	{
		vec3 vec = light_pos - w_pos;

		float point_to_light = length(vec);

		/* FragColor = vec4(vec3(nor) * 0.5 + 0.5, 1.0); return; */


		float sd = get_shadow(vec, point_to_light, dist_to_eye);

		/* FragColor = vec4(vec3(sd), 1.0); return; */
		/* FragColor = vec4(vec3(lookup_single(-vec)) / 100, 1.0); return; */
		if(sd < 0.95)
		{


			/* float specul_smudge = min(1.0, 1.0 - pow(noi, 5)); */
			float specul_smudge = 1.0;

			float lightAtt = 0.01;

			vec3 light_dir = normalize(-w_pos + light_pos);

			float diffuseCoefficient = max(0.0, dot(nor, light_dir));
			vec3 frag_diffuse = light_intensity * diffuseCoefficient * dif;

			float attenuation = 1.0 / (1.0 + lightAtt * pow(point_to_light, 2));

			vec3 color_lit = attenuation * frag_diffuse;


			/* if(specul_smudge > 0.0 && diffuseCoefficient > 0.005 && attenuation > 0.01) */
			/* { */
			/* 	vec4 spe = textureLod(gbuffer.specular, texcoord, 0); */

			/* 	vec3 eye_dir = normalize(-c_pos); */

			/* 	vec3 reflect_dir = reflect(light_dir, nor); */


			/* 	float specularCoefficient = 0.0; */
			/* 	vec3 specularColor = spe.rgb * light_intensity * specul_smudge; */
			/* 	float power = clamp(roughnessToSpecularPower(spe.a), 0.0f, 1.0f) * 10; */

			/* 	specularCoefficient = pow(max(dot(eye_dir, reflect_dir), 0.0), power); */
			/* 	if(power >= 1.0f) */
			/* 	{ */
			/* 		specularCoefficient = 0.0f; */
			/* 	} */

			/* 	vec3 frag_specular = clamp(specularCoefficient * specularColor, 0.0f, 1.0f); */
			/* 	color_lit += attenuation * frag_specular; */
				/* FragColor = vec4(vec3(specularCoefficient), 1.0); return; */
				/* FragColor = vec4(vec3(specularColor), 1.0); return; */
			/* } */
			color += color_lit * (1.0 - sd);
		}
	}

    color *= pass_sample(ssao, texcoord).rgb;

	FragColor = vec4(color, 1.0);
}  

// vim: set ft=c:
