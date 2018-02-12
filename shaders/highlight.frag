
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

uniform bool horizontal;

/* uniform float weight[3] = float[] (0.38774, 0.24477, 0.06136); */

uniform vec3 id_color;

vec3 get_unfiltered(vec2 offset)
{
	return textureLod(gbuffer.id, offset, 0).rgb;
}

float get_filtered(vec2 offset)
{
	vec3 c = textureLod(gbuffer.id, offset, 0).rgb;
	c = abs(c - id_color);
	if(c.x > 0.0001 || c.y > 0.0001 || c.z > 0.0001)
	{
		return 0;
	}
	return 0.1;
}

uniform pass_t refl;

void main()
{
	/* vec2 tex_offset = 1.0f / textureSize(gbuffer.id, 0); */

	float value = get_filtered(texcoord);

	vec4 color = pass_sample(refl, texcoord);
	vec3 final = color.rgb + vec3(1) * value;
	FragColor = vec4(final, 1.0f);

	/* if(horizontal) */
	/* { */
	/* 	c = get_filtered(texcoord).rgb * weight[0]; */
	/* 	for(int i = 1; i < 3; ++i) */
	/* 	{ */
	/* 		vec2 off = vec2(tex_offset.x * i, 0.0f); */
	/* 		c += get_filtered(texcoord + off) * weight[i]; */
	/* 		c += get_filtered(texcoord - off) * weight[i]; */
	/* 	} */
	/* } */
	/* else */
	/* { */
	/* 	c = get_unfiltered(texcoord).rgb * weight[0]; */
	/* 	for(int i = 1; i < 3; ++i) */
	/* 	{ */
	/* 		vec2 off = vec2(0.0f, tex_offset.y * i); */
	/* 		c += get_unfiltered(texcoord + off) * weight[i]; */
	/* 		c += get_unfiltered(texcoord - off) * weight[i]; */
	/* 	} */
	/* } */
	/* FragColor = vec4(c, 0.4); */
}

// vim: set ft=c:
