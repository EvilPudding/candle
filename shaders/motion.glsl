
#include "common.glsl"

layout (location = 0) out vec4 FragColor;

BUFFER {
	sampler2D color;
} buf;

BUFFER {
	sampler2D depth;
	sampler2D albedo;
} gbuffer;

uniform float power;

void main(void)
{
	vec2 p = pixel_pos();
	vec3 c_pos = get_position(gbuffer.depth, p);
	vec4 w_pos = (camera(model) * vec4(c_pos, 1.0));

	vec4 previousPos = (camera(projection) * camera(previous_view)) * w_pos;
	vec2 pp = previousPos.xy / previousPos.w;
	float ditherValue = ditherPattern[(int(gl_FragCoord.x) % 4) * 4 + (int(gl_FragCoord.y) % 4)];

	const int num_samples = 4;
	pp = (pp + 1.0) / 2.0;
	vec2 velocity = ((p - pp) / float(num_samples)) * power;

	float samples = 0.0;
	p += velocity * ditherValue;
	vec3 color = vec3(0.0);
	for(int i = 0; i < num_samples; ++i)
	{
		samples += 1.0;
		color += textureLod(buf.color, p, 0.0).rgb;
		p += velocity;
		if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0)
		{
			break;
		}
	}
	FragColor = vec4(color / samples, 1.0);
}

// vim: set ft=c:
