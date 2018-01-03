
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
uniform mat4 MV;
/* uniform mat3 MV3x3; */

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

out vec2 texcoord;

#ifdef MESH4
uniform float angle4;
#endif

out mat3 TM;
/* out vec3 lightDir; */

uniform float ratio;
uniform float started;
uniform float has_tex;
uniform vec3 light_pos;


out vec3 Position_worldspace;

void main()
{
	mat3 MV3x3 = mat3(MV);
#ifdef MESH4
	float Y = cos(angle4);
	float W = sin(angle4);
	vec4 pos = vec4(vec3(P.x, P.y * Y + P.w * W, P.z), 1.0);
#else
	vec4 pos = vec4(P, 1.0);
#endif

	Position_worldspace = (M * pos).xyz;

	texcoord = vec2(-UV.y, UV.x);

	mat3 TM = transpose(mat3(
		normalize(MV3x3 * TG),
		normalize(MV3x3 * BT),
		normalize(MV3x3 * N)
	));


	gl_Position = MVP * pos;
}  

// vim: set ft=c:
