
layout (location = 0) out vec4 DiffuseColor;
layout (location = 1) out vec4 SpecularColor;
layout (location = 2) out vec2 Normal;

#include "common.frag"

void main()
{

	/* vec2 pos = pixel_pos(); */
	/* vec3 pos3 = textureLod(gbuffer.wposition, pos, 0).rgb ; */
	vec4 w_pos = (camera.model*vec4(get_position(gbuffer), 1.0f));
	vec3 m_pos = (inverse(model) * w_pos).xyz;


	vec3 diff = abs(m_pos);
	if(diff.x > 1) discard;
	if(diff.y > 1) discard;
	if(diff.z > 1) discard;
	vec2 tc = m_pos.xy - 0.5;

	vec3 vnorm = resolveProperty(normal, tc).rgb * 2.0f - 1.0f;
	/* vnorm = vec3(0.0f, 0.0f, 1.0f); */

	vec3 norm = ((camera.view * model) * vec4(vnorm, 0.0f)).xyz;
	if(dot(norm, get_normal(gbuffer)) < 0.5) discard;

	DiffuseColor = resolveProperty(diffuse, tc);
	SpecularColor = resolveProperty(specular, tc);
	Normal = encode_normal(norm);

}

// vim: set ft=c:
