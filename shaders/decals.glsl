
layout (location = 0) out vec4 AlbedoColor;
layout (location = 1) out vec4 NRM; // normal_roughness_metalness

#include "common.glsl"
#line 8

BUFFER {
	sampler2D depth;
	/* sampler2D nmr; */
} gbuffer;

void main()
{

	/* vec2 pos = pixel_pos(); */
	/* vec3 pos3 = textureLod(gbuffer.wposition, pos, 0).rgb ; */
	vec4 w_pos = (camera(model)*vec4(get_position(gbuffer.depth), 1.0f));
	vec3 m_pos = (inverse(model) * w_pos).xyz;


	vec3 diff = abs(m_pos);
	if(diff.x > 0.5) discard;
	if(diff.y > 0.5) discard;
	if(diff.z > 0.5) discard;
	vec2 tc = m_pos.xy - 0.5;

	vec3 vnorm = resolveProperty(mat(normal), tc).rgb * 2.0f - 1.0f;
	/* vnorm = vec3(0.0f, 0.0f, 1.0f); */

	vec3 norm = ((camera(view) * model) * vec4(vnorm, 0.0f)).xyz;
	/* if(dot(norm, get_normal(gbuffer.nmr)) < 0.5) discard; */

	/* AlbedoColor = vec4(get_normal(gbuffer.nmr), 1); */
	AlbedoColor = resolveProperty(mat(albedo), tc);

	NRM.b = resolveProperty(mat(roughness), tc).r;
	NRM.a = resolveProperty(mat(metalness), tc).r;
	NRM.rg = encode_normal(norm);

}

// vim: set ft=c:
