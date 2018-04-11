
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

	vec3 final = vec3(0.0f);
	if(trans.r > 0.0f || trans.g > 0.0f || trans.b > 0.0f)
	{
		vec2 coord = pixel_pos() + nor.xy * (trans.a * 0.03);

		vec3 color = //pass_sample(refr, coord).rgb;
		textureLod(refr.diffuse, coord.xy, trans.a * 3).rgb;

		final += vec3(color * trans.rgb);
	}
	else if(emit.a > 0.0f)
	{
		final += emit.rgb * emit.a;
	}
	FragColor = vec4(final, 1.0f);
}

// vim: set ft=c:
