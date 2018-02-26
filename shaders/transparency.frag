
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;
uniform pass_t phong;

void main()
{
	vec3 dif = texture2D(gbuffer.diffuse, texcoord).rgb;
	vec3 trans = texture2D(gbuffer.transparency, texcoord).rgb;
	vec4 n = texture2D(gbuffer.normal, texcoord);
	vec3 nor = n.xyz;

	vec3 w_pos = texture2D(gbuffer.wposition, texcoord).rgb;
	vec3 v_pos = texture2D(gbuffer.cposition, texcoord).rgb;
	vec3 c_pos = camera.pos - w_pos;
	vec4 refl = texture2D(gbuffer.reflection, texcoord);

	if(n.a != 0.0f)
	{
		vec3 eye_dir = normalize(-c_pos);
		float dist_to_eye = length(c_pos);
		/* vec3 hit = v_pos - nor / (dist_to_eye + 2.0); */
		vec3 hit = v_pos + refract(eye_dir, nor, 1.3) / 4.0;

		/* float eta = 1.3f; */
		/* float N_dot_I = dot(nor, v_pos); */
		/* FragColor = vec4(vec3(N_dot_I), 1.0); return; */
		/* float k = 1.f - eta * eta * (1.f - N_dot_I * N_dot_I); */
		/* vec3 hit = eta * v_pos - (eta * N_dot_I + sqrt(k)) * nor; */

        vec4 projectedCoord     = camera.projection * vec4(hit, 1.0f);
        projectedCoord.xy      /= projectedCoord.w;
        projectedCoord.xy       = projectedCoord.xy * 0.5 + 0.5; 
		/* FragColor = vec4(projectedCoord.xy, 0.0, 1.0); */
		/* return; */

		/* pos.s = clamp(pos.s, 0.0, 1.0); */
		/* pos.t = clamp(pos.t, 0.0, 1.0); */

		vec3 color = pass_sample(phong, projectedCoord.xy).rgb;

		if(refl.r > 0.01)
		{
			vec3 reflect_dir = reflect(eye_dir, nor);

			vec3 reflect_color = textureLod(ambient_map, reflect_dir, 0).rgb;
			float coef = clamp(1.5 * pow(1.0 + dot(eye_dir, nor), 1), 0.0, 1.0);
			color = mix(color, dif * reflect_color, coef * refl.r);
			/* color = reflect_color * coef; */
			/* color = vec3(coef); */
		}
		FragColor = vec4(color, 1.0);
	}
	else
	{
		vec3 cc = pass_sample(phong, texcoord).rgb;
		FragColor = vec4(cc, 1.0);
	}
}

// vim: set ft=c:
