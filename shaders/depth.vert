
out vec3 LightDirection_tangentspace;

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
uniform vec3 probe_pos;
uniform vec3 camera_pos;

#ifdef MESH4
uniform float angle4;
#endif


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


	vec3 LightPosition_cameraspace = ( V * vec4(probe_pos, 1.0)).xyz;
	vec3 EyeDirection_cameraspace = - ( MV * pos).xyz;
	vec3 LightDirection_cameraspace = LightPosition_cameraspace + EyeDirection_cameraspace;

	Position_worldspace = (M * pos).xyz;

	/* texcoord = vec2(-UV.y, UV.x); */

	mat3 TM = transpose(mat3(
		normalize(MV3x3 * TG),
		normalize(MV3x3 * BT),
		normalize(MV3x3 * N)
	));

	LightDirection_tangentspace = TM * LightDirection_cameraspace;

	gl_Position = MVP * pos;
}  

// vim: set ft=c:
