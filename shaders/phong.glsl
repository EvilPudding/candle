
#include "common.glsl"
#line 2

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D depth;
	sampler2D albedo;
	sampler2D nn;
	sampler2D mr;
} gbuffer;

void main(void)
{
	ivec2 fc = ivec2(gl_FragCoord.xy);
	vec4 dif = texelFetch(gbuffer.albedo, fc, 0);
	if(dif.a == 0.0) discard;

	vec3 c_pos = get_position(gbuffer.depth);
	vec3 w_pos = (camera(model) * vec4(c_pos, 1.0)).xyz;

	vec2 normal = texelFetch(gbuffer.nn, fc, 0).rg;
	vec2 metalic_roughness = texelFetch(gbuffer.mr, fc, 0).rg;
	vec3 c_nor = decode_normal(normal);

	float dist_to_eye = length(c_pos);

	vec3 color = vec3(0.0);

	if(light(radius) < 0.0)
	{
		color = light(color.rgb) * dif.xyz;
	}
	else if(light(color).r > 0.01 || light(color).g > 0.01 || light(color).b > 0.01)
	{

		vec3 c_light = (camera(view) * vec4(obj_pos, 1.0)).xyz;

		vec3 c_light_dir = c_light - c_pos;
		vec3 w_light_dir = obj_pos - w_pos;

		float point_to_light = length(c_light_dir);


		float depth = textureLod(gbuffer.depth, pixel_pos(), 0.0).r;
		float sd = 0.0;
		if(dif.a >= 1.0)
		{
			sd = get_shadow(w_light_dir, point_to_light, dist_to_eye, depth);
		}

		if(sd < 0.95)
		{
			vec3 eye_dir = normalize(-c_pos);

			float l = point_to_light / light(radius);
			/* float attenuation = clamp(1.0 - pow(l, 0.3), 0.0, 1.0); */
			/* float attenuation = 1.0/(1.0+1.3*pow(l * 3.0, 2)); */
			/* attenuation *= clamp(1.0 - l, 0.0, 1.0); */
			float attenuation = ((3.0 - l*3.0) / (1.0+1.3*(pow(l*3.0-0.1, 2.0))))/3.0;
			attenuation = clamp(attenuation, 0.0, 1.0);

			vec4 color_lit = pbr(dif, metalic_roughness,
					light(color) * attenuation, c_light_dir, c_pos, c_nor);
			color += color_lit.xyz * (1.0 - sd);
		}
	}

	FragColor = vec4(color, 1.0);
}  

// vim: set ft=c:
