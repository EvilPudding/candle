
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} refr;

void main()
{
	float dif  = resolveProperty(mat(albedo), texcoord).a;
	if(dif < 0.7f) discard;

	vec4 trans = resolveProperty(mat(transparency), texcoord);
	vec4 emit = resolveProperty(mat(emissive), texcoord);

	vec3 final;
	if(trans.a > 0.0f)
	{
		float rough = resolveProperty(mat(roughness), texcoord).r;
		vec3 nor = get_normal();

		vec2 coord = pixel_pos() + nor.xy * (rough * 0.03f);

		vec3 color = textureLod(refr.color, coord.xy, rough * 3.0f).rgb;

		final = color * (1.0f - trans.rgb);
	}
	else
	{
		final = textureLod(refr.color, pixel_pos(), 0).rgb;
	}
	if(emit.a > 0.0f)
	{
		final = final * (1.0f - emit.a) + emit.rgb * emit.a;
	}

	FragColor = vec4(final, 1.0f);
}

// vim: set ft=c:
