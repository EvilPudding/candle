
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D depth;
	sampler2D albedo;
	sampler2D nmr;
} gbuffer;

const vec3 Fdielectric = vec3(0.04f);
void main()
{
	vec4 dif = textureLod(gbuffer.albedo, pixel_pos(), 0);
	vec3 c_pos = get_position(gbuffer.depth);
	vec3 w_pos = (camera.model * vec4(c_pos, 1.0f)).xyz;

	/* vec3 c_pos2 = get_position2(gbuffer); */

	vec4 normal_metalic_roughness = textureLod(gbuffer.nmr, pixel_pos(), 0);
	vec3 c_nor = decode_normal(normal_metalic_roughness.rg);

	float dist_to_eye = length(c_pos);

	vec3 color = vec3(0.0f);
	/* FragColor = vec4(texcoord - gl_FragCoord.xy / screen_size, 0, 1); return; */

	if(light_radius < 0.0f)
	{
		color = light_color.rgb * light_color.a * dif.xyz;
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
			vec3 eye_dir = normalize(-c_pos);
			vec3 lcolor = light_color.rgb * light_color.a;

			float l = point_to_light / light_radius;
			float attenuation = clamp(1.0f - pow(l, 2), 0.0f, 1.0f);

			vec4 color_lit = pbr(dif, normal_metalic_roughness.ba,
					light_color * attenuation, c_light_dir, c_pos, c_nor);
			color += color_lit.xyz * (1.0 - sd);
		}
	}

	FragColor = vec4(color, 1.0);
}  

// vim: set ft=c:
