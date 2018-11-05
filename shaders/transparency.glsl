
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} refr;

BUFFER {
	sampler2D depth;
	sampler2D albedo;
	sampler2D nmr;
} gbuffer;


void main()
{
	vec2 pp = pixel_pos();
	/* float far_depth = textureLod(gbuffer.depth, pp, 0).r; */
	/* if(gl_FragCoord.z > far_depth) discard; */

	float dif  = resolveProperty(mat(albedo), texcoord).a;
	if(dif < 0.7f) discard;

	vec4 trans = resolveProperty(mat(transparency), texcoord);
	vec4 emit = resolveProperty(mat(emissive), texcoord);

	vec3 final;
	if(trans.a > 0.0f)
	{
		/* float depth = far_depth - gl_FragCoord.z; */

		float rough = resolveProperty(mat(roughness), texcoord).r;
		vec3 nor = get_normal();

		vec2 coord = pp + nor.xy * (rough * 0.03f);

		float mip = clamp(rough * 3.0f, 0.0, 1.0);
		vec3 color = textureLod(refr.color, coord.xy, mip).rgb;

		final = color * (1.0f - trans.rgb);
	}
	else
	{
		final = textureLod(refr.color, pp, 0).rgb;
	}
	if(emit.a > 0.0f)
	{
		final = final * (1.0f - emit.a) + emit.rgb * emit.a;
	}

	FragColor = vec4(final, 1.0f);
}

// vim: set ft=c:
