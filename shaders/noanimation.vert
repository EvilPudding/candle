
#ifdef MESH4
layout (location = 0) in vec4 P;
#else
layout (location = 0) in vec3 P;
#endif
layout (location = 1) in vec3 N;
layout (location = 2) in vec2 UV;
layout (location = 3) in vec3 TG;
layout (location = 4) in vec3 BT;
/* layout (location = 5) in vec4 COL; */
layout (location = 5) in vec2 ID;

/* TODO make this included, not inline */
struct camera_t
{
	vec3 pos;
	float exposure;
	mat4 projection;
	mat4 view;
#ifdef MESH4
	float angle4;
#endif
};
/* ----------------------------------- */


uniform mat4 MVP;
uniform mat4 M;
uniform camera_t camera;
uniform vec2 id;
uniform float cameraspace_normals;

out mat4 inv_projection;

out vec3 tgspace_light_dir;
out vec3 tgspace_eye_dir;
out vec3 worldspace_position;
out vec3 cameraspace_vertex_pos;
out vec3 cameraspace_light_dir;

out vec3 cam_normal;
out vec3 cam_cnormal;
out vec3 cam_tangent;
out vec3 cam_bitangent;

out vec2 poly_id;
out vec2 object_id;

out vec3 vertex_normal;
out vec3 vertex_cnormal;
out vec3 vertex_tangent;
out vec3 vertex_bitangent;

out vec2 texcoord;
/* out vec4 vertex_color; */

out mat3 TM;
out mat3 C_TM;
/* out vec3 lightDir; */

uniform float ratio;
uniform float has_tex;
uniform vec3 light_pos;

void main()
{
	/* mat3 MV3x3 = mat3(MV); */
#ifdef MESH4
	float Y = cos(camera.angle4);
	float W = sin(camera.angle4);
	vec4 pos = vec4(vec3(P.x, P.y * Y + P.w * W, P.z), 1.0);
#else
	vec4 pos = vec4(P, 1.0);
#endif


	vec4 mpos = M * pos;
	worldspace_position = mpos.xyz;

	cameraspace_vertex_pos = (camera.view * mpos).xyz;

	/* vec3 cameraspace_light_pos = ( V * vec4(light_pos, 1.0)).xyz; */
	/* cameraspace_light_dir = cameraspace_light_pos - cameraspace_vertex_pos; */

	vec4 rotated_N = M * vec4(N, 0.0f);
	vec4 rotated_CN = camera.view * rotated_N;

	/* vertex_color = COL; */
	texcoord = vec2(-UV.y, UV.x);
	vertex_tangent = TG;
	vertex_bitangent = BT;
	vertex_normal = rotated_N.xyz;
	vertex_cnormal = rotated_CN.xyz;

	object_id = id;
	poly_id = ID;

	TM = mat3(TG, BT, rotated_N.xyz);

	C_TM = mat3(TG, BT, rotated_CN.xyz);

	{
		cam_normal = rotated_N.xyz;
		cam_cnormal = rotated_CN.xyz;
		/* cam_tangent = (M * vec4(TG, 0.0)).xyz; */
		/* cam_bitangent = (M * vec4(BT, 0.0)).xyz; */
	}

	/* inv_projection = inverse(projection); */

	gl_Position = MVP * pos;
	/* gl_Position = projection * vec4(cameraspace_vertex_pos, 1.0); */
}

// vim: set ft=c:
