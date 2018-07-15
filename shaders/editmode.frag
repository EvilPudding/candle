
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

uniform int mode;

#define EDIT_VERT	0
#define EDIT_EDGE	1
#define EDIT_FACE	2
#define EDIT_OBJECT	3

uniform vec2 mouse_pos;
uniform vec3 selected_pos;
uniform float start_radius;
uniform float tool_fade;

vec3 real_pos(float depth, vec2 coord)
{
    float z = depth * 2.0 - 1.0;
	coord = (coord * 2.0f) - 1.0;

    vec4 clipSpacePosition = vec4(coord, z, 1.0);
    vec4 viewSpacePosition = inverse(camera.projection) * clipSpacePosition;

    // Perspective division
    viewSpacePosition = viewSpacePosition / viewSpacePosition.w;

    vec4 worldSpacePosition = (inverse(camera.view) * viewSpacePosition);

    return worldSpacePosition.xyz;
}


float range(float v, float min, float max)
{
	return clamp((v - min) / (max - min), 0, 1);
}
void main()
{
	float intensity = 0.0f;
	vec2 p = pixel_pos();

	float dist = -(camera.view * vec4(selected_pos, 1.0f)).z;
	dist = unlinearize(dist);

	vec3 proj_pixel = real_pos(dist, p);

	float d = length(proj_pixel - selected_pos);
	d = 1.0f - abs((d-start_radius)/0.4f);
	d = clamp(d, 0, 1);
	intensity += pow(d, 1);

	vec3 proj_mouse = real_pos(dist, mouse_pos);
	/* FragColor = vec4(proj_mouse - proj_pixel, 1);return; */

	float dist_to_mouse = length(proj_mouse - proj_pixel);
	float md = clamp((0.4 - dist_to_mouse)/0.4 - 0.1, 0.0f, 1.0f);
	intensity += pow(md, 2);
	intensity -= max(dist_to_mouse - (1 - tool_fade) * start_radius * 2.1, 0) * tool_fade;

	if(intensity < 0.9) intensity = 0;
	if(intensity > 1.0) intensity = 1;

	float rad = abs(length(proj_mouse - selected_pos) - start_radius);

	vec3 color;
	if(rad > 0.3) color = vec3(0.1, 0.1, 0.4);
	else color = vec3(0.2, 0.2, 0.7);

	FragColor = vec4(color * intensity, 1.0f);
}

// vim: set ft=c:
