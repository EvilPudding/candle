#ifdef MESH4
layout (location = 0) in vec4 P;
#else
layout (location = 0) in vec3 P;
#endif
layout (location = 1) in vec3 N;
layout (location = 2) in vec2 UV;
layout (location = 3) in vec3 TG;
layout (location = 4) in vec3 BT;

uniform mat4 MVP;
uniform mat4 M;
uniform mat4 V;
uniform mat4 projection;
/* uniform mat3 MV3x3; */

out vec2 texcoord;
/* out vec3 lightDir; */

uniform float ratio;
uniform float started;
uniform float has_tex;
uniform vec3 light_pos;

uniform float screen_scale_x;
uniform float screen_scale_y;

#ifdef MESH4
uniform float angle4;
#endif

out vec3 tgspace_light_dir;
out vec3 tgspace_eye_dir;
out vec3 worldspace_position;
out vec3 cameraspace_vertex_pos;
out vec3 cameraspace_light_dir;

out vec3 cam_normal;
out vec3 cam_tangent;
out vec3 cam_bitangent;

out vec3 vertex_normal;
out vec3 vertex_tangent;
out vec3 vertex_bitangent;

out mat3 TM;


void main()
{
	/* texcoord = vec2(UV.x * (screen_width / texture_width), */
			/* UV.y * (screen_height / texture_height)); */
	texcoord = vec2(
			UV.x * screen_scale_x,
			UV.y * screen_scale_y);
	/* texcoord = vec2(UV.x, UV.y); */
	/* gl_Position = vec4(P * 0.25, 1.0); */
	gl_Position = vec4(P.xyz, 1.0);
}  

// vim: set ft=c:
