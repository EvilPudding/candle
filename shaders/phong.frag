
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D depth;
	sampler2D albedo;
	sampler2D nrm;
} gbuffer;

void main()
{
	vec3 dif = textureLod(gbuffer.albedo, pixel_pos(), 0).rgb;
	vec3 c_pos = get_position(gbuffer.depth);
	vec3 w_pos = (camera.model * vec4(c_pos, 1.0f)).xyz;

	/* vec3 c_pos2 = get_position2(gbuffer); */

	vec4 normal_roughness_metalness = textureLod(gbuffer.nrm, pixel_pos(), 0);
	vec3 c_nor = decode_normal(normal_roughness_metalness.rg);
	float roughness = normal_roughness_metalness.b;

	float dist_to_eye = length(c_pos);

	vec3 color = vec3(0.0f);
	/* FragColor = vec4(texcoord - gl_FragCoord.xy / screen_size, 0, 1); return; */

	if(light_radius < 0.0f)
	{
		color = light_color.rgb * light_color.a * dif;
	}
	else if(light_color.a > 0.01)
	{
		if(dist_to_eye > length(vertex_position)) discard;

		vec3 c_light = (camera.view * vec4(light_pos, 1.0f)).xyz;

		vec3 c_light_dir = c_light - c_pos;
		vec3 w_light_dir = light_pos - w_pos;

		float point_to_light = length(w_light_dir);
		/* FragColor = vec4(texture(shadow_map, -vec).rgb, 1.0f); return; */

		/* FragColor = vec4(vec3(nor) * 0.5 + 0.5, 1.0); return; */

		/* FragColor = vec4(w_light_dir, 1.0f); return; */
		float sd = get_shadow(w_light_dir, point_to_light, dist_to_eye);

		/* FragColor = vec4(vec3(sd), 1.0); return; */
		/* FragColor = vec4(vec3(lookup_single(-vec)) / 100, 1.0); return; */

		if(sd < 0.95)
		{
			vec3 lcolor = light_color.rgb * light_color.a;


			float lightAtt = 0.01;


			float diffuseCoefficient = max(0.0, dot(c_nor, normalize(c_light_dir)));
			/* FragColor = vec4(vec3(diffuseCoefficient / 10), 1.0f); return; */

			vec3 frag_diffuse = diffuseCoefficient * dif * lcolor;

			float l = point_to_light / light_radius;
			float attenuation = clamp(1.0f - pow(l, 2), 0.0f, 1.0f);
			/* float attenuation = 1.0 / (1.0 + lightAtt * pow(point_to_light, 2)); */

			vec3 color_lit = attenuation * frag_diffuse;


			if(diffuseCoefficient > 0.005 && attenuation > 0.01)
			{
				/* vec4 spe = textureLod(gbuffer.specular, pixel_pos(), 0); */

				/* vec3 eye_dir = normalize(-c_pos); */

				/* vec3 reflect_dir = normalize(reflect(c_light_dir, c_nor)); */


				/* float specularCoefficient = 0.0; */
				/* vec3 specularColor = spe.rgb * lcolor; */
				/* float power = clamp(roughnessToSpecularPower(spe.a), 0.0f, 1.0f) * 10; */

				/* specularCoefficient = pow(clamp(-dot(eye_dir, reflect_dir), 0.0f, 1.0f), power); */

				/* vec3 frag_specular = clamp(specularCoefficient * specularColor, 0.0f, 1.0f); */
				/* color_lit += attenuation * frag_specular; */
			}
			color += color_lit * (1.0 - sd);
		}
	}

	FragColor = vec4(color, 1.0);
}  

// vim: set ft=c:
