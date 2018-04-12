
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;
uniform pass_t refr;

void main()
{

	vec4 trans = get_transparency();
	vec4 emit = get_emissive();
	vec4 spec = get_specular();
	vec3 nor = get_normal();

	vec4 final = vec4(0.0f);
	if(trans.r > 0.0f || trans.g > 0.0f || trans.b > 0.0f)
	{
		vec2 coord = pixel_pos() + nor.xy * (trans.a * 0.03);

		vec3 color = //pass_sample(refr, coord).rgb;
		textureLod(refr.diffuse, coord.xy, trans.a * 3).rgb;

		final += vec4(color * trans.rgb, 1.0f);
	}
	else if(emit.a > 0.0f)
	{
		final += emit;
	}
	final.a = clamp(final.a, 0.0f, 1.0f);
	FragColor = final;
}

// vim: set ft=c:
