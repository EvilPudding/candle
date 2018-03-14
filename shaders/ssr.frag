
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;
uniform pass_t rendered;
uniform pass_t transp;

void main()
{
	vec4 col = pass_sample(rendered, texcoord);
	vec4 trans = pass_sample(transp, texcoord);
	if(trans.a > 0)
		col = trans;
	/* col = mix(col, trans, trans.a); */

	/* vec4 col = vec4(0.0f); */
	vec4 refl = textureLod(gbuffer.specular, texcoord, 0);
	vec4 ssred = ssr(rendered.texture);

	/* vec3 final = mix(color.rgb, ssred.rgb, ssred.a); */
	vec3 final = (col.rgb) + ssred.rgb * ssred.a * refl.rgb;

	/* FragColor = vec4(trans.xyz, 1.0f); return; */
	/* FragColor = vec4(ssred.rgb, 1.0f); return; */
	/* FragColor = vec4(col.rgb, 1.0f); return; */
	/* FragColor = vec4(textureLod(gbuffer.transparency, texcoord, 0).rgb, 1.0f); return; */

	/* final = clamp(final * 1.6f - 0.10f, 0.0, 3.0); */
	final = final * pow(2.0f, camera.exposure);
	float dist = length(textureLod(gbuffer.position, texcoord, 0).xyz);
	final.b += (clamp(dist - 5, 0, 1)) / 70;

	FragColor = vec4(final, 1.0f);
}

// vim: set ft=c:
