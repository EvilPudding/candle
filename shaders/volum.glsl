
#include "common.glsl"
#line 2

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D depth;
	sampler2D albedo;
} gbuffer;

void main(void)
{
	if (light(volumetric) == 0.0) discard;
	float ditherValue = ditherPattern[(int(gl_FragCoord.x) % 4) * 4 + (int(gl_FragCoord.y) % 4)];
	float depth = textureLod(gbuffer.depth, pixel_pos(), 0.0).r;
	if(gl_FragCoord.z < depth)
	{
		depth = gl_FragCoord.z;
	}
	float intensity = abs(light(volumetric));
	bool inverted = light(volumetric) < 0.0;
	/* FragColor = vec4(texcoord - gl_FragCoord.xy / screen_size, 0, 1); return; */
	vec3 c_pos = get_position(depth,  pixel_pos());
	vec3 w_cam = camera(pos);
	vec3 w_pos = (camera(model) * vec4(c_pos, 1.0)).xyz;
	float color = 0.0;
	int iterations = 32;

	vec3 norm_dir = normalize(w_cam - w_pos);
	vec3 dir = norm_dir * ((2.0 * light(radius)) / float(iterations));
	w_pos += dir * ditherValue;
	bool passed = false;
	for (int i = 0; i < iterations; i++)
	{
		w_pos += dir;
		vec3 dif = w_pos - obj_pos;
		float len = length(dif);
		float l = len / light(radius);
		float attenuation = ((3.0 - l*3.0) / (1.0+1.3*(pow(l*3.0-0.1, 2.0))))/3.0;
		attenuation = clamp(attenuation, 0.0, 1.0);
		if (attenuation > 0.0) {
			passed = true;
			float z;
			float Z = lookup_single(dif, z);
			bool in_light = Z >= z;
			in_light = inverted ? !in_light : in_light;

			/* color += (in_light ? attenuation : 0.0) * (1.0f - dot(norm_dir, dif / len)); */
			color += in_light ? attenuation * (1.0f - dot(norm_dir, dif / len)) : 0.0;
			/* if (!in_light) break; */
		}
		else if (passed)
		{
			break;
		}
		/* if (len > light(radius)) break; */
	}
	FragColor = vec4(light(color).rgb * (color / float(iterations)) * intensity, 1.);
	/* FragColor = vec4((pos + 10.0f) / 10.0f, 1); return; */
}  

// vim: set ft=c:
