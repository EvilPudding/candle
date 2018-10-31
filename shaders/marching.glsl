#extension GL_EXT_geometry_shader4 : enable
#extension GL_EXT_gpu_shader4 : enable

//Volume data field texture
uniform sampler3D dataFieldTex;
//Edge table texture
uniform isampler2D edgeTableTex;
//Triangles table texture
uniform isampler2D triTableTex;
//Global iso level
uniform float isolevel;
//Marching cubes vertices decal
uniform vec3 vertDecals[8];
//Vertices position for fragment shader
varying vec4 position;
//Get vertex i position within current marching cube
vec3 cubePos(int i){
	return gl_in[0].gl_Position.xyz + vertDecals[i];
}
//Get vertex i value within current marching cube
float cubeVal(vec3 p, int i){
	return texture3D(dataFieldTex, (cubePos(i)+1.0f)/2.0f).a;
}
//Get triangle table value
int triTableValue(int i, int j){
	return texelFetch2D(triTableTex, ivec2(j, i), 0).a;
}
//Compute interpolated vertex along an edge
vec3 vertexInterp(float isolevel, vec3 v0, float l0, vec3 v1, float l1){
	return mix(v0, v1, (isolevel-l0)/(l1-l0));
}
//Geometry Shader entry point
void main(void) {
	int cubeindex=0;
	vec3 cubesPos[8];
	cubesPos[0] = cubePos(0);
	cubesPos[1] = cubePos(1);
	cubesPos[2] = cubePos(2);
	cubesPos[3] = cubePos(3);
	cubesPos[4] = cubePos(4);
	cubesPos[5] = cubePos(5);
	cubesPos[6] = cubePos(6);
	cubesPos[7] = cubePos(7);

	float cubeVal0 = cubeVal(cubePos0, 0);
	float cubeVal1 = cubeVal(cubePos0, 1);
	float cubeVal2 = cubeVal(cubePos0, 2);
	float cubeVal3 = cubeVal(cubePos0, 3);
	float cubeVal4 = cubeVal(cubePos0, 4);
	float cubeVal5 = cubeVal(cubePos0, 5);
	float cubeVal6 = cubeVal(cubePos0, 6);
	float cubeVal7 = cubeVal(cubePos0, 7);
	//Determine the index into the edge table which
	//tells us which vertices are inside of the surface
	cubeindex = int(cubeVal0 < isolevel);
	cubeindex += int(cubeVal1 < isolevel)*2;
	cubeindex += int(cubeVal2 < isolevel)*4;
	cubeindex += int(cubeVal3 < isolevel)*8;
	cubeindex += int(cubeVal4 < isolevel)*16;
	cubeindex += int(cubeVal5 < isolevel)*32;
	cubeindex += int(cubeVal6 < isolevel)*64;
	cubeindex += int(cubeVal7 < isolevel)*128;
	//Cube is entirely in/out of the surface
	if (cubeindex ==0 || cubeindex == 255)
		return;
	vec3 vertlist[12];
	//Find the vertices where the surface intersects the cube
	vertlist[0] = vertexInterp(isolevel, cubesPos[0], cubeVal0, cubesPos[1], cubeVal1);
	vertlist[1] = vertexInterp(isolevel, cubesPos[1], cubeVal1, cubesPos[2], cubeVal2);
	vertlist[2] = vertexInterp(isolevel, cubesPos[2], cubeVal2, cubesPos[3], cubeVal3);
	vertlist[3] = vertexInterp(isolevel, cubesPos[3], cubeVal3, cubesPos[0], cubeVal0);
	vertlist[4] = vertexInterp(isolevel, cubesPos[4], cubeVal4, cubesPos[5], cubeVal5);
	vertlist[5] = vertexInterp(isolevel, cubesPos(5), cubeVal5, cubesPos(6), cubeVal6);
	vertlist[6] = vertexInterp(isolevel, cubesPos(6), cubeVal6, cubesPos(7), cubeVal7);
	vertlist[7] = vertexInterp(isolevel, cubesPos(7), cubeVal7, cubesPos(4), cubeVal4);
	vertlist[8] = vertexInterp(isolevel, cubesPos(0), cubeVal0, cubesPos(4), cubeVal4);
	vertlist[9] = vertexInterp(isolevel, cubesPos(1), cubeVal1, cubesPos(5), cubeVal5);
	vertlist[10] = vertexInterp(isolevel, cubesPos(2), cubeVal2, cubesPos(6), cubeVal6);
	vertlist[11] = vertexInterp(isolevel, cubesPos(3), cubeVal3, cubesPos(7), cubeVal7);
	// Create the triangle
	gl_FrontColor=vec4(cos(isolevel*5.0-0.5), sin(isolevel*5.0-0.5), 0.5, 1.0);
	int i=0;
	//Strange bug with this way, uncomment to test
	//for (i=0; triTableValue(cubeindex, i)!=-1; i+=3) {
	for(i = 0; i < 16; i += 3)
	{
		int i0 = triTableValue(cubeindex, i);
		int i1 = triTableValue(cubeindex, i + 1);
		int i2 = triTableValue(cubeindex, i + 2);
		if(i0 != -1)
		{
			gl_Position = vec4(vertlist[i0], 1);
			EmitVertex();

			gl_Position = vec4(vertlist[i1], 1);
			EmitVertex();

			gl_Position = vec4(vertlist[i2], 1);
			EmitVertex();

			EndPrimitive();
		}
		else
		{
			break;
		}
		i=i+3; //Comment it for testing the strange bug
	}
}
// vim: set ft=c:
