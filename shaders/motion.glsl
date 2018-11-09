
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
	ivec2 fc = ivec2(gl_FragCoord.xy);

	w_pos.a = 1;
	vec4 previousPos = (camera(projection) * camera(previous_view)) * w_pos;
	vec3 pp = previousPos.xyz / previousPos.w;
	/* FragColor = vec4(pp.xyz / 3, 1.0); return; */
	/* FragColor = vec4(pp, 1); */
	const int num_samples = 8;
	pp = (pp + 1.0) / 2.0f;
	vec2 velocity = (p.xy - pp.xy) / num_samples;

	// Get the initial color at this pixel.
	vec4 color = texture(buf.color, p);
	p += velocity;
	for(int i = 1; i < num_samples; ++i, p += velocity)
	{
		color += texture(buf.color, p);
	}
	FragColor = color / num_samples;
	/* FragColor = vec4(abs(velocity), 0.0, 1.0); */
}

// vim: set ft=c:
