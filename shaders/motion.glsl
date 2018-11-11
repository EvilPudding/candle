
#include "common.glsl"
#line 4

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} buf;

BUFFER {
	sampler2D depth;
	sampler2D albedo;
	sampler2D nmr;
} gbuffer;

void main()
{
	vec2 p = pixel_pos();
	vec3 c_pos = get_position(gbuffer.depth, p);
	vec4 w_pos = (camera(model) * vec4(c_pos, 1.0));

	vec4 previousPos = (camera(projection) * camera(previous_view)) * w_pos;
	vec2 pp = previousPos.xy / previousPos.w;

	const int num_samples = 8;
	pp = (pp + 1.0) / 2.0;
	vec2 velocity = (p - pp) / float(num_samples);

	vec3 color = textureLod(buf.color, p, 0).rgb;
	for(int i = 1; i < num_samples; ++i)
	{
		p += velocity;
		color += textureLod(buf.color, p, 0).rgb;
	}
	FragColor = vec4(color / num_samples, 1.0);
	/* FragColor = vec4(textureLod(buf.color, pp, 0).rgb, 1.0); */
	/* FragColor = vec4(pp, 0.0, 1.0); */
}

// vim: set ft=c:
