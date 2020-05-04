
uniform sampler3D values;
/* uniform isampler2D triTableTex; */
/* uniform float iso_level; */
float iso_level = 0.9;
float grid_size = 0.2;
const vec3 vert_offsets[8] = vec3[](
	vec3(0.0, 0.0, 0.0),
	vec3(1.0, 0.0, 0.0),
	vec3(1.0, 1.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0),
	vec3(1.0, 0.0, 1.0),
	vec3(1.0, 1.0, 1.0),
	vec3(0.0, 1.0, 1.0)
);
#define b3(r,g,b) (ivec3(r, g, b))
#define z3 ivec3(-1, -1, -1)
ivec3 tri_table[256 * 5] = ivec3[](
	z3,             z3,             z3,             z3,             z3,    
	b3( 0,  8,  3), z3,             z3,             z3,             z3,    
	b3( 0,  1,  9), z3,             z3,             z3,             z3,    
	b3( 1,  8,  3), b3( 9,  8,  1), z3,             z3,             z3,    
	b3( 1,  2, 10), z3,             z3,             z3,             z3,    
	b3( 0,  8,  3), b3( 1,  2, 10), z3,             z3,             z3,    
	b3( 9,  2, 10), b3( 0,  2,  9), z3,             z3,             z3,    
	b3( 2,  8,  3), b3( 2, 10,  8), b3(10,  9,  8), z3,             z3,    
	b3( 3, 11,  2), z3,             z3,             z3,             z3,    
	b3( 0, 11,  2), b3( 8, 11,  0), z3,             z3,             z3,    
	b3( 1,  9,  0), b3( 2,  3, 11), z3,             z3,             z3,    
	b3( 1, 11,  2), b3( 1,  9, 11), b3( 9,  8, 11), z3,             z3,    
	b3( 3, 10,  1), b3(11, 10,  3), z3,             z3,             z3,    
	b3( 0, 10,  1), b3( 0,  8, 10), b3( 8, 11, 10), z3,             z3,    
	b3( 3,  9,  0), b3( 3, 11,  9), b3(11, 10,  9), z3,             z3,    
	b3( 9,  8, 10), b3(10,  8, 11), z3,             z3,             z3,    
	b3( 4,  7,  8), z3,             z3,             z3,             z3,    
	b3( 4,  3,  0), b3( 7,  3,  4), z3,             z3,             z3,    
	b3( 0,  1,  9), b3( 8,  4,  7), z3,             z3,             z3,    
	b3( 4,  1,  9), b3( 4,  7,  1), b3( 7,  3,  1), z3,             z3,    
	b3( 1,  2, 10), b3( 8,  4,  7), z3,             z3,             z3,    
	b3( 3,  4,  7), b3( 3,  0,  4), b3( 1,  2, 10), z3,             z3,    
	b3( 9,  2, 10), b3( 9,  0,  2), b3( 8,  4,  7), z3,             z3,    
	b3( 2, 10,  9), b3( 2,  9,  7), b3( 2,  7,  3), b3( 7,  9,  4), z3,    
	b3( 8,  4,  7), b3( 3, 11,  2), z3,             z3,             z3,    
	b3(11,  4,  7), b3(11,  2,  4), b3( 2,  0,  4), z3,             z3,    
	b3( 9,  0,  1), b3( 8,  4,  7), b3( 2,  3, 11), z3,             z3,    
	b3( 4,  7, 11), b3( 9,  4, 11), b3( 9, 11,  2), b3( 9,  2,  1), z3,    
	b3( 3, 10,  1), b3( 3, 11, 10), b3( 7,  8,  4), z3,             z3,    
	b3( 1, 11, 10), b3( 1,  4, 11), b3( 1,  0,  4), b3( 7, 11,  4), z3,    
	b3( 4,  7,  8), b3( 9,  0, 11), b3( 9, 11, 10), b3(11,  0,  3), z3,    
	b3( 4,  7, 11), b3( 4, 11,  9), b3( 9, 11, 10), z3,             z3,    
	b3( 9,  5,  4), z3,             z3,             z3,             z3,    
	b3( 9,  5,  4), b3( 0,  8,  3), z3,             z3,             z3,    
	b3( 0,  5,  4), b3( 1,  5,  0), z3,             z3,             z3,    
	b3( 8,  5,  4), b3( 8,  3,  5), b3( 3,  1,  5), z3,             z3,    
	b3( 1,  2, 10), b3( 9,  5,  4), z3,             z3,             z3,    
	b3( 3,  0,  8), b3( 1,  2, 10), b3( 4,  9,  5), z3,             z3,    
	b3( 5,  2, 10), b3( 5,  4,  2), b3( 4,  0,  2), z3,             z3,    
	b3( 2, 10,  5), b3( 3,  2,  5), b3( 3,  5,  4), b3( 3,  4,  8), z3,    
	b3( 9,  5,  4), b3( 2,  3, 11), z3,             z3,             z3,    
	b3( 0, 11,  2), b3( 0,  8, 11), b3( 4,  9,  5), z3,             z3,    
	b3( 0,  5,  4), b3( 0,  1,  5), b3( 2,  3, 11), z3,             z3,    
	b3( 2,  1,  5), b3( 2,  5,  8), b3( 2,  8, 11), b3( 4,  8,  5), z3,    
	b3(10,  3, 11), b3(10,  1,  3), b3( 9,  5,  4), z3,             z3,    
	b3( 4,  9,  5), b3( 0,  8,  1), b3( 8, 10,  1), b3( 8, 11, 10), z3,    
	b3( 5,  4,  0), b3( 5,  0, 11), b3( 5, 11, 10), b3(11,  0,  3), z3,    
	b3( 5,  4,  8), b3( 5,  8, 10), b3(10,  8, 11), z3,             z3,    
	b3( 9,  7,  8), b3( 5,  7,  9), z3,             z3,             z3,    
	b3( 9,  3,  0), b3( 9,  5,  3), b3( 5,  7,  3), z3,             z3,    
	b3( 0,  7,  8), b3( 0,  1,  7), b3( 1,  5,  7), z3,             z3,    
	b3( 1,  5,  3), b3( 3,  5,  7), z3,             z3,             z3,    
	b3( 9,  7,  8), b3( 9,  5,  7), b3(10,  1,  2), z3,             z3,    
	b3(10,  1,  2), b3( 9,  5,  0), b3( 5,  3,  0), b3( 5,  7,  3), z3,    
	b3( 8,  0,  2), b3( 8,  2,  5), b3( 8,  5,  7), b3(10,  5,  2), z3,    
	b3( 2, 10,  5), b3( 2,  5,  3), b3( 3,  5,  7), z3,             z3,    
	b3( 7,  9,  5), b3( 7,  8,  9), b3( 3, 11,  2), z3,             z3,    
	b3( 9,  5,  7), b3( 9,  7,  2), b3( 9,  2,  0), b3( 2,  7, 11), z3,    
	b3( 2,  3, 11), b3( 0,  1,  8), b3( 1,  7,  8), b3( 1,  5,  7), z3,    
	b3(11,  2,  1), b3(11,  1,  7), b3( 7,  1,  5), z3,             z3,    
	b3( 9,  5,  8), b3( 8,  5,  7), b3(10,  1,  3), b3(10,  3, 11), z3,    
	b3( 5,  7,  0), b3( 5,  0,  9), b3( 7, 11,  0), b3( 1,  0, 10), b3(11, 10,  0),
	b3(11, 10,  0), b3(11,  0,  3), b3(10,  5,  0), b3( 8,  0,  7), b3( 5,  7,  0),
	b3(11, 10,  5), b3( 7, 11,  5), z3,             z3,             z3,    
	b3(10,  6,  5), z3,             z3,             z3,             z3,    
	b3( 0,  8,  3), b3( 5, 10,  6), z3,             z3,             z3,    
	b3( 9,  0,  1), b3( 5, 10,  6), z3,             z3,             z3,    
	b3( 1,  8,  3), b3( 1,  9,  8), b3( 5, 10,  6), z3,             z3,    
	b3( 1,  6,  5), b3( 2,  6,  1), z3,             z3,             z3,    
	b3( 1,  6,  5), b3( 1,  2,  6), b3( 3,  0,  8), z3,             z3,    
	b3( 9,  6,  5), b3( 9,  0,  6), b3( 0,  2,  6), z3,             z3,    
	b3( 5,  9,  8), b3( 5,  8,  2), b3( 5,  2,  6), b3( 3,  2,  8), z3,    
	b3( 2,  3, 11), b3(10,  6,  5), z3,             z3,             z3,    
	b3(11,  0,  8), b3(11,  2,  0), b3(10,  6,  5), z3,             z3,    
	b3( 0,  1,  9), b3( 2,  3, 11), b3( 5, 10,  6), z3,             z3,    
	b3( 5, 10,  6), b3( 1,  9,  2), b3( 9, 11,  2), b3( 9,  8, 11), z3,    
	b3( 6,  3, 11), b3( 6,  5,  3), b3( 5,  1,  3), z3,             z3,    
	b3( 0,  8, 11), b3( 0, 11,  5), b3( 0,  5,  1), b3( 5, 11,  6), z3,    
	b3( 3, 11,  6), b3( 0,  3,  6), b3( 0,  6,  5), b3( 0,  5,  9), z3,    
	b3( 6,  5,  9), b3( 6,  9, 11), b3(11,  9,  8), z3,             z3,    
	b3( 5, 10,  6), b3( 4,  7,  8), z3,             z3,             z3,    
	b3( 4,  3,  0), b3( 4,  7,  3), b3( 6,  5, 10), z3,             z3,    
	b3( 1,  9,  0), b3( 5, 10,  6), b3( 8,  4,  7), z3,             z3,    
	b3(10,  6,  5), b3( 1,  9,  7), b3( 1,  7,  3), b3( 7,  9,  4), z3,    
	b3( 6,  1,  2), b3( 6,  5,  1), b3( 4,  7,  8), z3,             z3,    
	b3( 1,  2,  5), b3( 5,  2,  6), b3( 3,  0,  4), b3( 3,  4,  7), z3,    
	b3( 8,  4,  7), b3( 9,  0,  5), b3( 0,  6,  5), b3( 0,  2,  6), z3,    
	b3( 7,  3,  9), b3( 7,  9,  4), b3( 3,  2,  9), b3( 5,  9,  6), b3( 2,  6,  9),
	b3( 3, 11,  2), b3( 7,  8,  4), b3(10,  6,  5), z3,             z3,    
	b3( 5, 10,  6), b3( 4,  7,  2), b3( 4,  2,  0), b3( 2,  7, 11), z3,    
	b3( 0,  1,  9), b3( 4,  7,  8), b3( 2,  3, 11), b3( 5, 10,  6), z3,    
	b3( 9,  2,  1), b3( 9, 11,  2), b3( 9,  4, 11), b3( 7, 11,  4), b3( 5, 10,  6),
	b3( 8,  4,  7), b3( 3, 11,  5), b3( 3,  5,  1), b3( 5, 11,  6), z3,    
	b3( 5,  1, 11), b3( 5, 11,  6), b3( 1,  0, 11), b3( 7, 11,  4), b3( 0,  4, 11),
	b3( 0,  5,  9), b3( 0,  6,  5), b3( 0,  3,  6), b3(11,  6,  3), b3( 8,  4,  7),
	b3( 6,  5,  9), b3( 6,  9, 11), b3( 4,  7,  9), b3( 7, 11,  9), z3,    
	b3(10,  4,  9), b3( 6,  4, 10), z3,             z3,             z3,    
	b3( 4, 10,  6), b3( 4,  9, 10), b3( 0,  8,  3), z3,             z3,    
	b3(10,  0,  1), b3(10,  6,  0), b3( 6,  4,  0), z3,             z3,    
	b3( 8,  3,  1), b3( 8,  1,  6), b3( 8,  6,  4), b3( 6,  1, 10), z3,    
	b3( 1,  4,  9), b3( 1,  2,  4), b3( 2,  6,  4), z3,             z3,    
	b3( 3,  0,  8), b3( 1,  2,  9), b3( 2,  4,  9), b3( 2,  6,  4), z3,    
	b3( 0,  2,  4), b3( 4,  2,  6), z3,             z3,             z3,    
	b3( 8,  3,  2), b3( 8,  2,  4), b3( 4,  2,  6), z3,             z3,    
	b3(10,  4,  9), b3(10,  6,  4), b3(11,  2,  3), z3,             z3,    
	b3( 0,  8,  2), b3( 2,  8, 11), b3( 4,  9, 10), b3( 4, 10,  6), z3,    
	b3( 3, 11,  2), b3( 0,  1,  6), b3( 0,  6,  4), b3( 6,  1, 10), z3,    
	b3( 6,  4,  1), b3( 6,  1, 10), b3( 4,  8,  1), b3( 2,  1, 11), b3( 8, 11,  1),
	b3( 9,  6,  4), b3( 9,  3,  6), b3( 9,  1,  3), b3(11,  6,  3), z3,    
	b3( 8, 11,  1), b3( 8,  1,  0), b3(11,  6,  1), b3( 9,  1,  4), b3( 6,  4,  1),
	b3( 3, 11,  6), b3( 3,  6,  0), b3( 0,  6,  4), z3,             z3,    
	b3( 6,  4,  8), b3(11,  6,  8), z3,             z3,             z3,    
	b3( 7, 10,  6), b3( 7,  8, 10), b3( 8,  9, 10), z3,             z3,    
	b3( 0,  7,  3), b3( 0, 10,  7), b3( 0,  9, 10), b3( 6,  7, 10), z3,    
	b3(10,  6,  7), b3( 1, 10,  7), b3( 1,  7,  8), b3( 1,  8,  0), z3,    
	b3(10,  6,  7), b3(10,  7,  1), b3( 1,  7,  3), z3,             z3,    
	b3( 1,  2,  6), b3( 1,  6,  8), b3( 1,  8,  9), b3( 8,  6,  7), z3,    
	b3( 2,  6,  9), b3( 2,  9,  1), b3( 6,  7,  9), b3( 0,  9,  3), b3( 7,  3,  9),
	b3( 7,  8,  0), b3( 7,  0,  6), b3( 6,  0,  2), z3,             z3,    
	b3( 7,  3,  2), b3( 6,  7,  2), z3,             z3,             z3,    
	b3( 2,  3, 11), b3(10,  6,  8), b3(10,  8,  9), b3( 8,  6,  7), z3,    
	b3( 2,  0,  7), b3( 2,  7, 11), b3( 0,  9,  7), b3( 6,  7, 10), b3( 9, 10,  7),
	b3( 1,  8,  0), b3( 1,  7,  8), b3( 1, 10,  7), b3( 6,  7, 10), b3( 2,  3, 11),
	b3(11,  2,  1), b3(11,  1,  7), b3(10,  6,  1), b3( 6,  7,  1), z3,    
	b3( 8,  9,  6), b3( 8,  6,  7), b3( 9,  1,  6), b3(11,  6,  3), b3( 1,  3,  6),
	b3( 0,  9,  1), b3(11,  6,  7), z3,             z3,             z3,    
	b3( 7,  8,  0), b3( 7,  0,  6), b3( 3, 11,  0), b3(11,  6,  0), z3,    
	b3( 7, 11,  6), z3,             z3,             z3,             z3,    
	b3( 7,  6, 11), z3,             z3,             z3,             z3,    
	b3( 3,  0,  8), b3(11,  7,  6), z3,             z3,             z3,    
	b3( 0,  1,  9), b3(11,  7,  6), z3,             z3,             z3,    
	b3( 8,  1,  9), b3( 8,  3,  1), b3(11,  7,  6), z3,             z3,    
	b3(10,  1,  2), b3( 6, 11,  7), z3,             z3,             z3,    
	b3( 1,  2, 10), b3( 3,  0,  8), b3( 6, 11,  7), z3,             z3,    
	b3( 2,  9,  0), b3( 2, 10,  9), b3( 6, 11,  7), z3,             z3,    
	b3( 6, 11,  7), b3( 2, 10,  3), b3(10,  8,  3), b3(10,  9,  8), z3,    
	b3( 7,  2,  3), b3( 6,  2,  7), z3,             z3,             z3,    
	b3( 7,  0,  8), b3( 7,  6,  0), b3( 6,  2,  0), z3,             z3,    
	b3( 2,  7,  6), b3( 2,  3,  7), b3( 0,  1,  9), z3,             z3,    
	b3( 1,  6,  2), b3( 1,  8,  6), b3( 1,  9,  8), b3( 8,  7,  6), z3,    
	b3(10,  7,  6), b3(10,  1,  7), b3( 1,  3,  7), z3,             z3,    
	b3(10,  7,  6), b3( 1,  7, 10), b3( 1,  8,  7), b3( 1,  0,  8), z3,    
	b3( 0,  3,  7), b3( 0,  7, 10), b3( 0, 10,  9), b3( 6, 10,  7), z3,    
	b3( 7,  6, 10), b3( 7, 10,  8), b3( 8, 10,  9), z3,             z3,    
	b3( 6,  8,  4), b3(11,  8,  6), z3,             z3,             z3,    
	b3( 3,  6, 11), b3( 3,  0,  6), b3( 0,  4,  6), z3,             z3,    
	b3( 8,  6, 11), b3( 8,  4,  6), b3( 9,  0,  1), z3,             z3,    
	b3( 9,  4,  6), b3( 9,  6,  3), b3( 9,  3,  1), b3(11,  3,  6), z3,    
	b3( 6,  8,  4), b3( 6, 11,  8), b3( 2, 10,  1), z3,             z3,    
	b3( 1,  2, 10), b3( 3,  0, 11), b3( 0,  6, 11), b3( 0,  4,  6), z3,    
	b3( 4, 11,  8), b3( 4,  6, 11), b3( 0,  2,  9), b3( 2, 10,  9), z3,    
	b3(10,  9,  3), b3(10,  3,  2), b3( 9,  4,  3), b3(11,  3,  6), b3( 4,  6,  3),
	b3( 8,  2,  3), b3( 8,  4,  2), b3( 4,  6,  2), z3,             z3,    
	b3( 0,  4,  2), b3( 4,  6,  2), z3,             z3,             z3,    
	b3( 1,  9,  0), b3( 2,  3,  4), b3( 2,  4,  6), b3( 4,  3,  8), z3,    
	b3( 1,  9,  4), b3( 1,  4,  2), b3( 2,  4,  6), z3,             z3,    
	b3( 8,  1,  3), b3( 8,  6,  1), b3( 8,  4,  6), b3( 6, 10,  1), z3,    
	b3(10,  1,  0), b3(10,  0,  6), b3( 6,  0,  4), z3,             z3,    
	b3( 4,  6,  3), b3( 4,  3,  8), b3( 6, 10,  3), b3( 0,  3,  9), b3(10,  9,  3),
	b3(10,  9,  4), b3( 6, 10,  4), z3,             z3,             z3,    
	b3( 4,  9,  5), b3( 7,  6, 11), z3,             z3,             z3,    
	b3( 0,  8,  3), b3( 4,  9,  5), b3(11,  7,  6), z3,             z3,    
	b3( 5,  0,  1), b3( 5,  4,  0), b3( 7,  6, 11), z3,             z3,    
	b3(11,  7,  6), b3( 8,  3,  4), b3( 3,  5,  4), b3( 3,  1,  5), z3,    
	b3( 9,  5,  4), b3(10,  1,  2), b3( 7,  6, 11), z3,             z3,    
	b3( 6, 11,  7), b3( 1,  2, 10), b3( 0,  8,  3), b3( 4,  9,  5), z3,    
	b3( 7,  6, 11), b3( 5,  4, 10), b3( 4,  2, 10), b3( 4,  0,  2), z3,    
	b3( 3,  4,  8), b3( 3,  5,  4), b3( 3,  2,  5), b3(10,  5,  2), b3(11,  7,  6),
	b3( 7,  2,  3), b3( 7,  6,  2), b3( 5,  4,  9), z3,             z3,    
	b3( 9,  5,  4), b3( 0,  8,  6), b3( 0,  6,  2), b3( 6,  8,  7), z3,    
	b3( 3,  6,  2), b3( 3,  7,  6), b3( 1,  5,  0), b3( 5,  4,  0), z3,    
	b3( 6,  2,  8), b3( 6,  8,  7), b3( 2,  1,  8), b3( 4,  8,  5), b3( 1,  5,  8),
	b3( 9,  5,  4), b3(10,  1,  6), b3( 1,  7,  6), b3( 1,  3,  7), z3,    
	b3( 1,  6, 10), b3( 1,  7,  6), b3( 1,  0,  7), b3( 8,  7,  0), b3( 9,  5,  4),
	b3( 4,  0, 10), b3( 4, 10,  5), b3( 0,  3, 10), b3( 6, 10,  7), b3( 3,  7, 10),
	b3( 7,  6, 10), b3( 7, 10,  8), b3( 5,  4, 10), b3( 4,  8, 10), z3,    
	b3( 6,  9,  5), b3( 6, 11,  9), b3(11,  8,  9), z3,             z3,    
	b3( 3,  6, 11), b3( 0,  6,  3), b3( 0,  5,  6), b3( 0,  9,  5), z3,    
	b3( 0, 11,  8), b3( 0,  5, 11), b3( 0,  1,  5), b3( 5,  6, 11), z3,    
	b3( 6, 11,  3), b3( 6,  3,  5), b3( 5,  3,  1), z3,             z3,    
	b3( 1,  2, 10), b3( 9,  5, 11), b3( 9, 11,  8), b3(11,  5,  6), z3,    
	b3( 0, 11,  3), b3( 0,  6, 11), b3( 0,  9,  6), b3( 5,  6,  9), b3( 1,  2, 10),
	b3(11,  8,  5), b3(11,  5,  6), b3( 8,  0,  5), b3(10,  5,  2), b3( 0,  2,  5),
	b3( 6, 11,  3), b3( 6,  3,  5), b3( 2, 10,  3), b3(10,  5,  3), z3,    
	b3( 5,  8,  9), b3( 5,  2,  8), b3( 5,  6,  2), b3( 3,  8,  2), z3,    
	b3( 9,  5,  6), b3( 9,  6,  0), b3( 0,  6,  2), z3,             z3,    
	b3( 1,  5,  8), b3( 1,  8,  0), b3( 5,  6,  8), b3( 3,  8,  2), b3( 6,  2,  8),
	b3( 1,  5,  6), b3( 2,  1,  6), z3,             z3,             z3,    
	b3( 1,  3,  6), b3( 1,  6, 10), b3( 3,  8,  6), b3( 5,  6,  9), b3( 8,  9,  6),
	b3(10,  1,  0), b3(10,  0,  6), b3( 9,  5,  0), b3( 5,  6,  0), z3,    
	b3( 0,  3,  8), b3( 5,  6, 10), z3,             z3,             z3,    
	b3(10,  5,  6), z3,             z3,             z3,             z3,    
	b3(11,  5, 10), b3( 7,  5, 11), z3,             z3,             z3,    
	b3(11,  5, 10), b3(11,  7,  5), b3( 8,  3,  0), z3,             z3,    
	b3( 5, 11,  7), b3( 5, 10, 11), b3( 1,  9,  0), z3,             z3,    
	b3(10,  7,  5), b3(10, 11,  7), b3( 9,  8,  1), b3( 8,  3,  1), z3,    
	b3(11,  1,  2), b3(11,  7,  1), b3( 7,  5,  1), z3,             z3,    
	b3( 0,  8,  3), b3( 1,  2,  7), b3( 1,  7,  5), b3( 7,  2, 11), z3,    
	b3( 9,  7,  5), b3( 9,  2,  7), b3( 9,  0,  2), b3( 2, 11,  7), z3,    
	b3( 7,  5,  2), b3( 7,  2, 11), b3( 5,  9,  2), b3( 3,  2,  8), b3( 9,  8,  2),
	b3( 2,  5, 10), b3( 2,  3,  5), b3( 3,  7,  5), z3,             z3,    
	b3( 8,  2,  0), b3( 8,  5,  2), b3( 8,  7,  5), b3(10,  2,  5), z3,    
	b3( 9,  0,  1), b3( 5, 10,  3), b3( 5,  3,  7), b3( 3, 10,  2), z3,    
	b3( 9,  8,  2), b3( 9,  2,  1), b3( 8,  7,  2), b3(10,  2,  5), b3( 7,  5,  2),
	b3( 1,  3,  5), b3( 3,  7,  5), z3,             z3,             z3,    
	b3( 0,  8,  7), b3( 0,  7,  1), b3( 1,  7,  5), z3,             z3,    
	b3( 9,  0,  3), b3( 9,  3,  5), b3( 5,  3,  7), z3,             z3,    
	b3( 9,  8,  7), b3( 5,  9,  7), z3,             z3,             z3,    
	b3( 5,  8,  4), b3( 5, 10,  8), b3(10, 11,  8), z3,             z3,    
	b3( 5,  0,  4), b3( 5, 11,  0), b3( 5, 10, 11), b3(11,  3,  0), z3,    
	b3( 0,  1,  9), b3( 8,  4, 10), b3( 8, 10, 11), b3(10,  4,  5), z3,    
	b3(10, 11,  4), b3(10,  4,  5), b3(11,  3,  4), b3( 9,  4,  1), b3( 3,  1,  4),
	b3( 2,  5,  1), b3( 2,  8,  5), b3( 2, 11,  8), b3( 4,  5,  8), z3,    
	b3( 0,  4, 11), b3( 0, 11,  3), b3( 4,  5, 11), b3( 2, 11,  1), b3( 5,  1, 11),
	b3( 0,  2,  5), b3( 0,  5,  9), b3( 2, 11,  5), b3( 4,  5,  8), b3(11,  8,  5),
	b3( 9,  4,  5), b3( 2, 11,  3), z3,             z3,             z3,    
	b3( 2,  5, 10), b3( 3,  5,  2), b3( 3,  4,  5), b3( 3,  8,  4), z3,    
	b3( 5, 10,  2), b3( 5,  2,  4), b3( 4,  2,  0), z3,             z3,    
	b3( 3, 10,  2), b3( 3,  5, 10), b3( 3,  8,  5), b3( 4,  5,  8), b3( 0,  1,  9),
	b3( 5, 10,  2), b3( 5,  2,  4), b3( 1,  9,  2), b3( 9,  4,  2), z3,    
	b3( 8,  4,  5), b3( 8,  5,  3), b3( 3,  5,  1), z3,             z3,    
	b3( 0,  4,  5), b3( 1,  0,  5), z3,             z3,             z3,    
	b3( 8,  4,  5), b3( 8,  5,  3), b3( 9,  0,  5), b3( 0,  3,  5), z3,    
	b3( 9,  4,  5), z3,             z3,             z3,             z3,    
	b3( 4, 11,  7), b3( 4,  9, 11), b3( 9, 10, 11), z3,             z3,    
	b3( 0,  8,  3), b3( 4,  9,  7), b3( 9, 11,  7), b3( 9, 10, 11), z3,    
	b3( 1, 10, 11), b3( 1, 11,  4), b3( 1,  4,  0), b3( 7,  4, 11), z3,    
	b3( 3,  1,  4), b3( 3,  4,  8), b3( 1, 10,  4), b3( 7,  4, 11), b3(10, 11,  4),
	b3( 4, 11,  7), b3( 9, 11,  4), b3( 9,  2, 11), b3( 9,  1,  2), z3,    
	b3( 9,  7,  4), b3( 9, 11,  7), b3( 9,  1, 11), b3( 2, 11,  1), b3( 0,  8,  3),
	b3(11,  7,  4), b3(11,  4,  2), b3( 2,  4,  0), z3,             z3,    
	b3(11,  7,  4), b3(11,  4,  2), b3( 8,  3,  4), b3( 3,  2,  4), z3,    
	b3( 2,  9, 10), b3( 2,  7,  9), b3( 2,  3,  7), b3( 7,  4,  9), z3,    
	b3( 9, 10,  7), b3( 9,  7,  4), b3(10,  2,  7), b3( 8,  7,  0), b3( 2,  0,  7),
	b3( 3,  7, 10), b3( 3, 10,  2), b3( 7,  4, 10), b3( 1, 10,  0), b3( 4,  0, 10),
	b3( 1, 10,  2), b3( 8,  7,  4), z3,             z3,             z3,    
	b3( 4,  9,  1), b3( 4,  1,  7), b3( 7,  1,  3), z3,             z3,    
	b3( 4,  9,  1), b3( 4,  1,  7), b3( 0,  8,  1), b3( 8,  7,  1), z3,    
	b3( 4,  0,  3), b3( 7,  4,  3), z3,             z3,             z3,    
	b3( 4,  8,  7), z3,             z3,             z3,             z3,    
	b3( 9, 10,  8), b3(10, 11,  8), z3,             z3,             z3,    
	b3( 3,  0,  9), b3( 3,  9, 11), b3(11,  9, 10), z3,             z3,    
	b3( 0,  1, 10), b3( 0, 10,  8), b3( 8, 10, 11), z3,             z3,    
	b3( 3,  1, 10), b3(11,  3, 10), z3,             z3,             z3,    
	b3( 1,  2, 11), b3( 1, 11,  9), b3( 9, 11,  8), z3,             z3,    
	b3( 3,  0,  9), b3( 3,  9, 11), b3( 1,  2,  9), b3( 2, 11,  9), z3,    
	b3( 0,  2, 11), b3( 8,  0, 11), z3,             z3,             z3,    
	b3( 3,  2, 11), z3,             z3,             z3,             z3,    
	b3( 2,  3,  8), b3( 2,  8, 10), b3(10,  8,  9), z3,             z3,    
	b3( 9, 10,  2), b3( 0,  9,  2), z3,             z3,             z3,    
	b3( 2,  3,  8), b3( 2,  8, 10), b3( 0,  1,  8), b3( 1, 10,  8), z3,    
	b3( 1, 10,  2), z3,             z3,             z3,             z3,    
	b3( 1,  3,  8), b3( 9,  1,  8), z3,             z3,             z3,    
	b3( 0,  9,  1), z3,             z3,             z3,             z3,    
	b3( 0,  3,  8), z3,             z3,             z3,             z3,    
	z3,             z3,             z3,             z3,             z3
);

vec3 get_cubePos(int i)
{
	return gl_in[0].gl_Position.xyz + vert_offsets[i] * grid_size;
}

vec3 get_cubeColor(vec3 p)
{
	float t = scene.time * 3.;
	float r0 = abs(cos(t * .3)) * 2. + .6;
	float r1 = abs(sin(t * .35)) * 2. + .6;
	vec3 origin = vec3(0., 2.5, 0.);
	vec3 p1 = vec3(sin(t), cos(t), sin(t * .612)) * r0;
	vec3 p2 = vec3(sin(t * 1.1), cos(t * 1.2), sin(t * .512)) * r1;
	float val = 1. / (dot(p - origin, p - origin) + 1.);
	float val2 = .6 /  (dot(p - origin - p1, p - origin - p1) + 0.7);
	float val3 = .5 /  (dot(p - origin - p2, p - origin - p2) + 0.9);
	return vec3(val, val2, val3);
}

float get_cubeVal(vec3 p)
{
	vec3 color = get_cubeColor(p);
	return color.x + color.y + color.z;
}

vec3 isonormal(vec3 pos)
{
	vec3 nor;
	float offset = grid_size * 0.5;
	nor.x = get_cubeVal(pos + vec3(grid_size, 0.0, 0.0))
	      - get_cubeVal(pos - vec3(grid_size, 0.0, 0.0));
	nor.y = get_cubeVal(pos + vec3(0.0, grid_size, 0.0))
	      - get_cubeVal(pos - vec3(0.0, grid_size, 0.0));
	nor.z = get_cubeVal(pos + vec3(0.0, 0.0, grid_size))
	      - get_cubeVal(pos - vec3(0.0, 0.0, grid_size));
	return -normalize(nor);
}

ivec3 triTableValue(int i, int j)
{
	return tri_table[i * 5 + j];
	/* return texelFetch(triTableTex, ivec2(j, i), 0).a; */
}

vec3 vert_lerp(float iso_level, vec3 v0, float l0, vec3 v1, float l1)
{
	return mix(v0, v1, (iso_level - l0) / (l1 - l0));
}

void main(void)
{
	vec4 pos;

	id = $id[0];
	matid = $matid[0];
	object_id = $object_id[0];
	poly_id = $poly_id[0];
	obj_pos = $obj_pos[0];
	model = $model[0];

	int cubeindex = 0;
	vec3 cubesPos[8];
	cubesPos[0] = get_cubePos(0);
	cubesPos[1] = get_cubePos(1);
	cubesPos[2] = get_cubePos(2);
	cubesPos[3] = get_cubePos(3);
	cubesPos[4] = get_cubePos(4);
	cubesPos[5] = get_cubePos(5);
	cubesPos[6] = get_cubePos(6);
	cubesPos[7] = get_cubePos(7);

	float cubeVal0 = get_cubeVal(cubesPos[0]);
	float cubeVal1 = get_cubeVal(cubesPos[1]);
	float cubeVal2 = get_cubeVal(cubesPos[2]);
	float cubeVal3 = get_cubeVal(cubesPos[3]);
	float cubeVal4 = get_cubeVal(cubesPos[4]);
	float cubeVal5 = get_cubeVal(cubesPos[5]);
	float cubeVal6 = get_cubeVal(cubesPos[6]);
	float cubeVal7 = get_cubeVal(cubesPos[7]);
	/* Determine the index into the edge table which */
	/* tells us which vertices are inside of the surface */
	cubeindex = int(cubeVal0 < iso_level);
	cubeindex += int(cubeVal1 < iso_level)*2;
	cubeindex += int(cubeVal2 < iso_level)*4;
	cubeindex += int(cubeVal3 < iso_level)*8;
	cubeindex += int(cubeVal4 < iso_level)*16;
	cubeindex += int(cubeVal5 < iso_level)*32;
	cubeindex += int(cubeVal6 < iso_level)*64;
	cubeindex += int(cubeVal7 < iso_level)*128;
	/* Cube is entirely in/out of the surface */
	if (cubeindex == 0)
		return;

	if (cubeindex == 255)
		return;

	vec3 vertlist[12];
	/* Find the vertices where the surface intersects the cube */
	vertlist[0] = vert_lerp(iso_level, cubesPos[0], cubeVal0, cubesPos[1], cubeVal1);
	vertlist[1] = vert_lerp(iso_level, cubesPos[1], cubeVal1, cubesPos[2], cubeVal2);
	vertlist[2] = vert_lerp(iso_level, cubesPos[2], cubeVal2, cubesPos[3], cubeVal3);
	vertlist[3] = vert_lerp(iso_level, cubesPos[3], cubeVal3, cubesPos[0], cubeVal0);
	vertlist[4] = vert_lerp(iso_level, cubesPos[4], cubeVal4, cubesPos[5], cubeVal5);
	vertlist[5] = vert_lerp(iso_level, cubesPos[5], cubeVal5, cubesPos[6], cubeVal6);
	vertlist[6] = vert_lerp(iso_level, cubesPos[6], cubeVal6, cubesPos[7], cubeVal7);
	vertlist[7] = vert_lerp(iso_level, cubesPos[7], cubeVal7, cubesPos[4], cubeVal4);
	vertlist[8] = vert_lerp(iso_level, cubesPos[0], cubeVal0, cubesPos[4], cubeVal4);
	vertlist[9] = vert_lerp(iso_level, cubesPos[1], cubeVal1, cubesPos[5], cubeVal5);
	vertlist[10] = vert_lerp(iso_level, cubesPos[2], cubeVal2, cubesPos[6], cubeVal6);
	vertlist[11] = vert_lerp(iso_level, cubesPos[3], cubeVal3, cubesPos[7], cubeVal7);

	for(int i = 0; i < 5; i++)
	{
		ivec3 I = triTableValue(cubeindex, i);
		mat4 MV = camera(view) * model;
		if(I.x != -1)
		{
			vec4 pos0;
			vec4 pos1;
			vec4 pos2;
			vec3 wpos0;
			vec3 wpos1;
			vec3 wpos2;
			vec3 cpos0;
			vec3 cpos1;
			vec3 cpos2;
			vec3 norm0;
			vec3 norm1;
			vec3 norm2;

			pos0 = vec4(vertlist[I.x], 1.0);
			norm0 = isonormal(pos0.xyz);
			norm0 = (MV * vec4(norm0, 0.0f)).xyz;
			pos0 = model * pos0;
			wpos0 = pos.xyz;
			pos0 = camera(view) * pos0;
			cpos0 = pos0.xyz;
			pos0 = camera(projection) * pos0;

			pos1 = vec4(vertlist[I.y], 1.0);
			norm1 = isonormal(pos1.xyz);
			norm1 = (MV * vec4(norm1, 0.0f)).xyz;
			pos1 = model * pos1;
			wpos1 = pos1.xyz;
			pos1 = camera(view) * pos1;
			cpos1 = pos1.xyz;
			pos1 = camera(projection) * pos1;

			pos2 = vec4(vertlist[I.z], 1.0);
			norm2 = isonormal(pos2.xyz);
			norm2 = (MV * vec4(norm2, 0.0f)).xyz;
			pos2 = model * pos2;
			wpos2 = pos2.xyz;
			pos2 = camera(view) * pos2;
			cpos2 = pos2.xyz;
			pos2 = camera(projection) * pos2;

			/* vertex_normal = normalize(cross(cpos1.xyz - cpos0.xyz, cpos2.xyz - cpos0.xyz)); */

			vertex_normal = norm0;
			poly_color = get_cubeColor(vertlist[I.x]);
			vertex_world_position = wpos0;
			vertex_position = cpos0;
			gl_Position = pos0;
			EmitVertex();

			vertex_world_position = wpos1;
			vertex_position = cpos1;
			poly_color = get_cubeColor(vertlist[I.y]);
			vertex_normal = norm1;
			gl_Position = pos1;
			EmitVertex();

			vertex_world_position = wpos2;
			vertex_position = cpos2;
			poly_color = get_cubeColor(vertlist[I.z]);
			vertex_normal = norm2;
			gl_Position = pos2;
			EmitVertex();

			EndPrimitive();
		}
		else
		{
			break;
		}
	}
}
// vim: set ft=c:
