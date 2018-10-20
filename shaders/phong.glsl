
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D depth;
	sampler2D albedo;
	sampler2D nmr;
} gbuffer;

void main()
{
	ivec2 fc = ivec2(gl_FragCoord.xy);
	vec4 dif = texelFetch(gbuffer.albedo, fc, 0);

	vec3 c_pos = get_position(gbuffer.depth);
	vec3 w_pos = (model * vec4(c_pos, 1.0f)).xyz;

	/* vec3 c_pos2 = get_position2(gbuffer); */

	vec4 normal_metalic_roughness = texelFetch(gbuffer.nmr, fc, 0);
	vec3 c_nor = decode_normal(normal_metalic_roughness.rg);

	float dist_to_eye = length(c_pos);

	vec3 color = vec3(0.0f);
	/* FragColor = vec4(texcoord - gl_FragCoord.xy / screen_size, 0, 1); return; */

	if(light(radius) < 0.0f)
	{
		color = light(color.rgb) * light(color.a) * dif.xyz;
	}
	else if(light(color.a) > 0.01)
	{
		/* if(dist_to_eye > length(vertex_position)) discard; */

		vec3 c_light = (camera(view) * vec4(obj_pos, 1.0f)).xyz;

		vec3 c_light_dir = c_light - c_pos;
		vec3 w_light_dir = obj_pos - w_pos;

		float point_to_light = length(c_light_dir);
		/* FragColor = vec4(texture(shadow_map, -vec).rgb, 1.0f); return; */

		/* FragColor = vec4(vec3(nor) * 0.5 + 0.5, 1.0); return; */

		/* FragColor = vec4(w_light_dir, 1.0f); return; */
		float sd = get_shadow(w_light_dir, point_to_light,
				dist_to_eye);
		sd = 0;
		/* FragColor = vec4(vec3(texture(light(shadow_map), -w_light_dir).a)/100.0f, 1.0); return; */

		if(sd < 0.95)
		{
			vec3 eye_dir = normalize(-c_pos);
			vec3 lcolor = light(color.rgb) * light(color.a);

			float l = point_to_light / light(radius);
			/* float attenuation = clamp(1.0f - pow(l, 0.3), 0.0f, 1.0f); */
			/* float attenuation = 1.0f/(1.0f+1.3f*pow(l * 3.0f, 2)); */
			/* attenuation *= clamp(1.0f - l, 0.0f, 1.0f); */
			float attenuation = ((3.0f - l*3.0f) / (1.0f+1.3f*(pow(l*3.0f-0.1f, 2))))/3.0f;
			attenuation = clamp(attenuation, 0, 1);

			vec4 color_lit = pbr(dif, normal_metalic_roughness.ba,
					light(color) * attenuation, c_light_dir, c_pos, c_nor);
			color += color_lit.xyz * (1.0 - sd);
		}
	}

	FragColor = vec4(color, 1.0);
}  

// vim: set ft=c:
