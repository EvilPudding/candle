
uniform sampler3D values;
/* uniform isampler2D triTableTex; */
/* uniform float iso_level; */
float iso_level = 0.5f;
const vec3 vertOffsets[8] = vec3[](
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
	z3,				z3,				z3,				z3,				z3,	
	b3( 0,  8,  3),	z3,				z3,				z3,				z3,	
	b3( 0,  1,  9),	z3,				z3,				z3,				z3,	
	b3( 1,  8,  3),	b3( 9,  8,  1),	z3,				z3,				z3,	
	b3( 1,  2, 10),	z3,				z3,				z3,				z3,	
	b3( 0,  8,  3),	b3( 1,  2, 10),	z3,				z3,				z3,	
	b3( 9,  2, 10),	b3( 0,  2,  9),	z3,				z3,				z3,	
	b3( 2,  8,  3),	b3( 2, 10,  8),	b3(10,  9,  8),	z3,				z3,	
	b3( 3, 11,  2),	z3,				z3,				z3,				z3,	
	b3( 0, 11,  2),	b3( 8, 11,  0),	z3,				z3,				z3,	
	b3( 1,  9,  0),	b3( 2,  3, 11),	z3,				z3,				z3,	
	b3( 1, 11,  2),	b3( 1,  9, 11),	b3( 9,  8, 11),	z3,				z3,	
	b3( 3, 10,  1),	b3(11, 10,  3),	z3,				z3,				z3,	
	b3( 0, 10,  1),	b3( 0,  8, 10),	b3( 8, 11, 10),	z3,				z3,	
	b3( 3,  9,  0),	b3( 3, 11,  9),	b3(11, 10,  9),	z3,				z3,	
	b3( 9,  8, 10),	b3(10,  8, 11),	z3,				z3,				z3,	
	b3( 4,  7,  8),	z3,				z3,				z3,				z3,	
	b3( 4,  3,  0),	b3( 7,  3,  4),	z3,				z3,				z3,	
	b3( 0,  1,  9),	b3( 8,  4,  7),	z3,				z3,				z3,	
	b3( 4,  1,  9),	b3( 4,  7,  1),	b3( 7,  3,  1),	z3,				z3,	
	b3( 1,  2, 10),	b3( 8,  4,  7),	z3,				z3,				z3,	
	b3( 3,  4,  7),	b3( 3,  0,  4),	b3( 1,  2, 10),	z3,				z3,	
	b3( 9,  2, 10),	b3( 9,  0,  2),	b3( 8,  4,  7),	z3,				z3,	
	b3( 2, 10,  9),	b3( 2,  9,  7),	b3( 2,  7,  3),	b3( 7,  9,  4),	z3,	
	b3( 8,  4,  7),	b3( 3, 11,  2),	z3,				z3,				z3,	
	b3(11,  4,  7),	b3(11,  2,  4),	b3( 2,  0,  4),	z3,				z3,	
	b3( 9,  0,  1),	b3( 8,  4,  7),	b3( 2,  3, 11),	z3,				z3,	
	b3( 4,  7, 11),	b3( 9,  4, 11),	b3( 9, 11,  2),	b3( 9,  2,  1),	z3,	
	b3( 3, 10,  1),	b3( 3, 11, 10),	b3( 7,  8,  4),	z3,				z3,	
	b3( 1, 11, 10),	b3( 1,  4, 11),	b3( 1,  0,  4),	b3( 7, 11,  4),	z3,	
	b3( 4,  7,  8),	b3( 9,  0, 11),	b3( 9, 11, 10),	b3(11,  0,  3),	z3,	
	b3( 4,  7, 11),	b3( 4, 11,  9),	b3( 9, 11, 10),	z3,				z3,	
	b3( 9,  5,  4),	z3,				z3,				z3,				z3,	
	b3( 9,  5,  4),	b3( 0,  8,  3),	z3,				z3,				z3,	
	b3( 0,  5,  4),	b3( 1,  5,  0),	z3,				z3,				z3,	
	b3( 8,  5,  4),	b3( 8,  3,  5),	b3( 3,  1,  5),	z3,				z3,	
	b3( 1,  2, 10),	b3( 9,  5,  4),	z3,				z3,				z3,	
	b3( 3,  0,  8),	b3( 1,  2, 10),	b3( 4,  9,  5),	z3,				z3,	
	b3( 5,  2, 10),	b3( 5,  4,  2),	b3( 4,  0,  2),	z3,				z3,	
	b3( 2, 10,  5),	b3( 3,  2,  5),	b3( 3,  5,  4),	b3( 3,  4,  8),	z3,	
	b3( 9,  5,  4),	b3( 2,  3, 11),	z3,				z3,				z3,	
	b3( 0, 11,  2),	b3( 0,  8, 11),	b3( 4,  9,  5),	z3,				z3,	
	b3( 0,  5,  4),	b3( 0,  1,  5),	b3( 2,  3, 11),	z3,				z3,	
	b3( 2,  1,  5),	b3( 2,  5,  8),	b3( 2,  8, 11),	b3( 4,  8,  5),	z3,	
	b3(10,  3, 11),	b3(10,  1,  3),	b3( 9,  5,  4),	z3,				z3,	
	b3( 4,  9,  5),	b3( 0,  8,  1),	b3( 8, 10,  1),	b3( 8, 11, 10),	z3,	
	b3( 5,  4,  0),	b3( 5,  0, 11),	b3( 5, 11, 10),	b3(11,  0,  3),	z3,	
	b3( 5,  4,  8),	b3( 5,  8, 10),	b3(10,  8, 11),	z3,				z3,	
	b3( 9,  7,  8),	b3( 5,  7,  9),	z3,				z3,				z3,	
	b3( 9,  3,  0),	b3( 9,  5,  3),	b3( 5,  7,  3),	z3,				z3,	
	b3( 0,  7,  8),	b3( 0,  1,  7),	b3( 1,  5,  7),	z3,				z3,	
	b3( 1,  5,  3),	b3( 3,  5,  7),	z3,				z3,				z3,	
	b3( 9,  7,  8),	b3( 9,  5,  7),	b3(10,  1,  2),	z3,				z3,	
	b3(10,  1,  2),	b3( 9,  5,  0),	b3( 5,  3,  0),	b3( 5,  7,  3),	z3,	
	b3( 8,  0,  2),	b3( 8,  2,  5),	b3( 8,  5,  7),	b3(10,  5,  2),	z3,	
	b3( 2, 10,  5),	b3( 2,  5,  3),	b3( 3,  5,  7),	z3,				z3,	
	b3( 7,  9,  5),	b3( 7,  8,  9),	b3( 3, 11,  2),	z3,				z3,	
	b3( 9,  5,  7),	b3( 9,  7,  2),	b3( 9,  2,  0),	b3( 2,  7, 11),	z3,	
	b3( 2,  3, 11),	b3( 0,  1,  8),	b3( 1,  7,  8),	b3( 1,  5,  7),	z3,	
	b3(11,  2,  1),	b3(11,  1,  7),	b3( 7,  1,  5),	z3,				z3,	
	b3( 9,  5,  8),	b3( 8,  5,  7),	b3(10,  1,  3),	b3(10,  3, 11),	z3,	
	b3( 5,  7,  0),	b3( 5,  0,  9),	b3( 7, 11,  0),	b3( 1,  0, 10),	b3(11, 10,  0),
	b3(11, 10,  0),	b3(11,  0,  3),	b3(10,  5,  0),	b3( 8,  0,  7),	b3( 5,  7,  0),
	b3(11, 10,  5),	b3( 7, 11,  5),	z3,				z3,				z3,	
	b3(10,  6,  5),	z3,				z3,				z3,				z3,	
	b3( 0,  8,  3),	b3( 5, 10,  6),	z3,				z3,				z3,	
	b3( 9,  0,  1),	b3( 5, 10,  6),	z3,				z3,				z3,	
	b3( 1,  8,  3),	b3( 1,  9,  8),	b3( 5, 10,  6),	z3,				z3,	
	b3( 1,  6,  5),	b3( 2,  6,  1),	z3,				z3,				z3,	
	b3( 1,  6,  5),	b3( 1,  2,  6),	b3( 3,  0,  8),	z3,				z3,	
	b3( 9,  6,  5),	b3( 9,  0,  6),	b3( 0,  2,  6),	z3,				z3,	
	b3( 5,  9,  8),	b3( 5,  8,  2),	b3( 5,  2,  6),	b3( 3,  2,  8),	z3,	
	b3( 2,  3, 11),	b3(10,  6,  5),	z3,				z3,				z3,	
	b3(11,  0,  8),	b3(11,  2,  0),	b3(10,  6,  5),	z3,				z3,	
	b3( 0,  1,  9),	b3( 2,  3, 11),	b3( 5, 10,  6),	z3,				z3,	
	b3( 5, 10,  6),	b3( 1,  9,  2),	b3( 9, 11,  2),	b3( 9,  8, 11),	z3,	
	b3( 6,  3, 11),	b3( 6,  5,  3),	b3( 5,  1,  3),	z3,				z3,	
	b3( 0,  8, 11),	b3( 0, 11,  5),	b3( 0,  5,  1),	b3( 5, 11,  6),	z3,	
	b3( 3, 11,  6),	b3( 0,  3,  6),	b3( 0,  6,  5),	b3( 0,  5,  9),	z3,	
	b3( 6,  5,  9),	b3( 6,  9, 11),	b3(11,  9,  8),	z3,				z3,	
	b3( 5, 10,  6),	b3( 4,  7,  8),	z3,				z3,				z3,	
	b3( 4,  3,  0),	b3( 4,  7,  3),	b3( 6,  5, 10),	z3,				z3,	
	b3( 1,  9,  0),	b3( 5, 10,  6),	b3( 8,  4,  7),	z3,				z3,	
	b3(10,  6,  5),	b3( 1,  9,  7),	b3( 1,  7,  3),	b3( 7,  9,  4),	z3,	
	b3( 6,  1,  2),	b3( 6,  5,  1),	b3( 4,  7,  8),	z3,				z3,	
	b3( 1,  2,  5),	b3( 5,  2,  6),	b3( 3,  0,  4),	b3( 3,  4,  7),	z3,	
	b3( 8,  4,  7),	b3( 9,  0,  5),	b3( 0,  6,  5),	b3( 0,  2,  6),	z3,	
	b3( 7,  3,  9),	b3( 7,  9,  4),	b3( 3,  2,  9),	b3( 5,  9,  6),	b3( 2,  6,  9),
	b3( 3, 11,  2),	b3( 7,  8,  4),	b3(10,  6,  5),	z3,				z3,	
	b3( 5, 10,  6),	b3( 4,  7,  2),	b3( 4,  2,  0),	b3( 2,  7, 11),	z3,	
	b3( 0,  1,  9),	b3( 4,  7,  8),	b3( 2,  3, 11),	b3( 5, 10,  6),	z3,	
	b3( 9,  2,  1),	b3( 9, 11,  2),	b3( 9,  4, 11),	b3( 7, 11,  4),	b3( 5, 10,  6),
	b3( 8,  4,  7),	b3( 3, 11,  5),	b3( 3,  5,  1),	b3( 5, 11,  6),	z3,	
	b3( 5,  1, 11),	b3( 5, 11,  6),	b3( 1,  0, 11),	b3( 7, 11,  4),	b3( 0,  4, 11),
	b3( 0,  5,  9),	b3( 0,  6,  5),	b3( 0,  3,  6),	b3(11,  6,  3),	b3( 8,  4,  7),
	b3( 6,  5,  9),	b3( 6,  9, 11),	b3( 4,  7,  9),	b3( 7, 11,  9),	z3,	
	b3(10,  4,  9),	b3( 6,  4, 10),	z3,				z3,				z3,	
	b3( 4, 10,  6),	b3( 4,  9, 10),	b3( 0,  8,  3),	z3,				z3,	
	b3(10,  0,  1),	b3(10,  6,  0),	b3( 6,  4,  0),	z3,				z3,	
	b3( 8,  3,  1),	b3( 8,  1,  6),	b3( 8,  6,  4),	b3( 6,  1, 10),	z3,	
	b3( 1,  4,  9),	b3( 1,  2,  4),	b3( 2,  6,  4),	z3,				z3,	
	b3( 3,  0,  8),	b3( 1,  2,  9),	b3( 2,  4,  9),	b3( 2,  6,  4),	z3,	
	b3( 0,  2,  4),	b3( 4,  2,  6),	z3,				z3,				z3,	
	b3( 8,  3,  2),	b3( 8,  2,  4),	b3( 4,  2,  6),	z3,				z3,	
	b3(10,  4,  9),	b3(10,  6,  4),	b3(11,  2,  3),	z3,				z3,	
	b3( 0,  8,  2),	b3( 2,  8, 11),	b3( 4,  9, 10),	b3( 4, 10,  6),	z3,	
	b3( 3, 11,  2),	b3( 0,  1,  6),	b3( 0,  6,  4),	b3( 6,  1, 10),	z3,	
	b3( 6,  4,  1),	b3( 6,  1, 10),	b3( 4,  8,  1),	b3( 2,  1, 11),	b3( 8, 11,  1),
	b3( 9,  6,  4),	b3( 9,  3,  6),	b3( 9,  1,  3),	b3(11,  6,  3),	z3,	
	b3( 8, 11,  1),	b3( 8,  1,  0),	b3(11,  6,  1),	b3( 9,  1,  4),	b3( 6,  4,  1),
	b3( 3, 11,  6),	b3( 3,  6,  0),	b3( 0,  6,  4),	z3,				z3,	
	b3( 6,  4,  8),	b3(11,  6,  8),	z3,				z3,				z3,	
	b3( 7, 10,  6),	b3( 7,  8, 10),	b3( 8,  9, 10),	z3,				z3,	
	b3( 0,  7,  3),	b3( 0, 10,  7),	b3( 0,  9, 10),	b3( 6,  7, 10),	z3,	
	b3(10,  6,  7),	b3( 1, 10,  7),	b3( 1,  7,  8),	b3( 1,  8,  0),	z3,	
	b3(10,  6,  7),	b3(10,  7,  1),	b3( 1,  7,  3),	z3,				z3,	
	b3( 1,  2,  6),	b3( 1,  6,  8),	b3( 1,  8,  9),	b3( 8,  6,  7),	z3,	
	b3( 2,  6,  9),	b3( 2,  9,  1),	b3( 6,  7,  9),	b3( 0,  9,  3),	b3( 7,  3,  9),
	b3( 7,  8,  0),	b3( 7,  0,  6),	b3( 6,  0,  2),	z3,				z3,	
	b3( 7,  3,  2),	b3( 6,  7,  2),	z3,				z3,				z3,	
	b3( 2,  3, 11),	b3(10,  6,  8),	b3(10,  8,  9),	b3( 8,  6,  7),	z3,	
	b3( 2,  0,  7),	b3( 2,  7, 11),	b3( 0,  9,  7),	b3( 6,  7, 10),	b3( 9, 10,  7),
	b3( 1,  8,  0),	b3( 1,  7,  8),	b3( 1, 10,  7),	b3( 6,  7, 10),	b3( 2,  3, 11),
	b3(11,  2,  1),	b3(11,  1,  7),	b3(10,  6,  1),	b3( 6,  7,  1),	z3,	
	b3( 8,  9,  6),	b3( 8,  6,  7),	b3( 9,  1,  6),	b3(11,  6,  3),	b3( 1,  3,  6),
	b3( 0,  9,  1),	b3(11,  6,  7),	z3,				z3,				z3,	
	b3( 7,  8,  0),	b3( 7,  0,  6),	b3( 3, 11,  0),	b3(11,  6,  0),	z3,	
	b3( 7, 11,  6),	z3,				z3,				z3,				z3,	
	b3( 7,  6, 11),	z3,				z3,				z3,				z3,	
	b3( 3,  0,  8),	b3(11,  7,  6),	z3,				z3,				z3,	
	b3( 0,  1,  9),	b3(11,  7,  6),	z3,				z3,				z3,	
	b3( 8,  1,  9),	b3( 8,  3,  1),	b3(11,  7,  6),	z3,				z3,	
	b3(10,  1,  2),	b3( 6, 11,  7),	z3,				z3,				z3,	
	b3( 1,  2, 10),	b3( 3,  0,  8),	b3( 6, 11,  7),	z3,				z3,	
	b3( 2,  9,  0),	b3( 2, 10,  9),	b3( 6, 11,  7),	z3,				z3,	
	b3( 6, 11,  7),	b3( 2, 10,  3),	b3(10,  8,  3),	b3(10,  9,  8),	z3,	
	b3( 7,  2,  3),	b3( 6,  2,  7),	z3,				z3,				z3,	
	b3( 7,  0,  8),	b3( 7,  6,  0),	b3( 6,  2,  0),	z3,				z3,	
	b3( 2,  7,  6),	b3( 2,  3,  7),	b3( 0,  1,  9),	z3,				z3,	
	b3( 1,  6,  2),	b3( 1,  8,  6),	b3( 1,  9,  8),	b3( 8,  7,  6),	z3,	
	b3(10,  7,  6),	b3(10,  1,  7),	b3( 1,  3,  7),	z3,				z3,	
	b3(10,  7,  6),	b3( 1,  7, 10),	b3( 1,  8,  7),	b3( 1,  0,  8),	z3,	
	b3( 0,  3,  7),	b3( 0,  7, 10),	b3( 0, 10,  9),	b3( 6, 10,  7),	z3,	
	b3( 7,  6, 10),	b3( 7, 10,  8),	b3( 8, 10,  9),	z3,				z3,	
	b3( 6,  8,  4),	b3(11,  8,  6),	z3,				z3,				z3,	
	b3( 3,  6, 11),	b3( 3,  0,  6),	b3( 0,  4,  6),	z3,				z3,	
	b3( 8,  6, 11),	b3( 8,  4,  6),	b3( 9,  0,  1),	z3,				z3,	
	b3( 9,  4,  6),	b3( 9,  6,  3),	b3( 9,  3,  1),	b3(11,  3,  6),	z3,	
	b3( 6,  8,  4),	b3( 6, 11,  8),	b3( 2, 10,  1),	z3,				z3,	
	b3( 1,  2, 10),	b3( 3,  0, 11),	b3( 0,  6, 11),	b3( 0,  4,  6),	z3,	
	b3( 4, 11,  8),	b3( 4,  6, 11),	b3( 0,  2,  9),	b3( 2, 10,  9),	z3,	
	b3(10,  9,  3),	b3(10,  3,  2),	b3( 9,  4,  3),	b3(11,  3,  6),	b3( 4,  6,  3),
	b3( 8,  2,  3),	b3( 8,  4,  2),	b3( 4,  6,  2),	z3,				z3,	
	b3( 0,  4,  2),	b3( 4,  6,  2),	z3,				z3,				z3,	
	b3( 1,  9,  0),	b3( 2,  3,  4),	b3( 2,  4,  6),	b3( 4,  3,  8),	z3,	
	b3( 1,  9,  4),	b3( 1,  4,  2),	b3( 2,  4,  6),	z3,				z3,	
	b3( 8,  1,  3),	b3( 8,  6,  1),	b3( 8,  4,  6),	b3( 6, 10,  1),	z3,	
	b3(10,  1,  0),	b3(10,  0,  6),	b3( 6,  0,  4),	z3,				z3,	
	b3( 4,  6,  3),	b3( 4,  3,  8),	b3( 6, 10,  3),	b3( 0,  3,  9),	b3(10,  9,  3),
	b3(10,  9,  4),	b3( 6, 10,  4),	z3,				z3,				z3,	
	b3( 4,  9,  5),	b3( 7,  6, 11),	z3,				z3,				z3,	
	b3( 0,  8,  3),	b3( 4,  9,  5),	b3(11,  7,  6),	z3,				z3,	
	b3( 5,  0,  1),	b3( 5,  4,  0),	b3( 7,  6, 11),	z3,				z3,	
	b3(11,  7,  6),	b3( 8,  3,  4),	b3( 3,  5,  4),	b3( 3,  1,  5),	z3,	
	b3( 9,  5,  4),	b3(10,  1,  2),	b3( 7,  6, 11),	z3,				z3,	
	b3( 6, 11,  7),	b3( 1,  2, 10),	b3( 0,  8,  3),	b3( 4,  9,  5),	z3,	
	b3( 7,  6, 11),	b3( 5,  4, 10),	b3( 4,  2, 10),	b3( 4,  0,  2),	z3,	
	b3( 3,  4,  8),	b3( 3,  5,  4),	b3( 3,  2,  5),	b3(10,  5,  2),	b3(11,  7,  6),
	b3( 7,  2,  3),	b3( 7,  6,  2),	b3( 5,  4,  9),	z3,				z3,	
	b3( 9,  5,  4),	b3( 0,  8,  6),	b3( 0,  6,  2),	b3( 6,  8,  7),	z3,	
	b3( 3,  6,  2),	b3( 3,  7,  6),	b3( 1,  5,  0),	b3( 5,  4,  0),	z3,	
	b3( 6,  2,  8),	b3( 6,  8,  7),	b3( 2,  1,  8),	b3( 4,  8,  5),	b3( 1,  5,  8),
	b3( 9,  5,  4),	b3(10,  1,  6),	b3( 1,  7,  6),	b3( 1,  3,  7),	z3,	
	b3( 1,  6, 10),	b3( 1,  7,  6),	b3( 1,  0,  7),	b3( 8,  7,  0),	b3( 9,  5,  4),
	b3( 4,  0, 10),	b3( 4, 10,  5),	b3( 0,  3, 10),	b3( 6, 10,  7),	b3( 3,  7, 10),
	b3( 7,  6, 10),	b3( 7, 10,  8),	b3( 5,  4, 10),	b3( 4,  8, 10),	z3,	
	b3( 6,  9,  5),	b3( 6, 11,  9),	b3(11,  8,  9),	z3,				z3,	
	b3( 3,  6, 11),	b3( 0,  6,  3),	b3( 0,  5,  6),	b3( 0,  9,  5),	z3,	
	b3( 0, 11,  8),	b3( 0,  5, 11),	b3( 0,  1,  5),	b3( 5,  6, 11),	z3,	
	b3( 6, 11,  3),	b3( 6,  3,  5),	b3( 5,  3,  1),	z3,				z3,	
	b3( 1,  2, 10),	b3( 9,  5, 11),	b3( 9, 11,  8),	b3(11,  5,  6),	z3,	
	b3( 0, 11,  3),	b3( 0,  6, 11),	b3( 0,  9,  6),	b3( 5,  6,  9),	b3( 1,  2, 10),
	b3(11,  8,  5),	b3(11,  5,  6),	b3( 8,  0,  5),	b3(10,  5,  2),	b3( 0,  2,  5),
	b3( 6, 11,  3),	b3( 6,  3,  5),	b3( 2, 10,  3),	b3(10,  5,  3),	z3,	
	b3( 5,  8,  9),	b3( 5,  2,  8),	b3( 5,  6,  2),	b3( 3,  8,  2),	z3,	
	b3( 9,  5,  6),	b3( 9,  6,  0),	b3( 0,  6,  2),	z3,				z3,	
	b3( 1,  5,  8),	b3( 1,  8,  0),	b3( 5,  6,  8),	b3( 3,  8,  2),	b3( 6,  2,  8),
	b3( 1,  5,  6),	b3( 2,  1,  6),	z3,				z3,				z3,	
	b3( 1,  3,  6),	b3( 1,  6, 10),	b3( 3,  8,  6),	b3( 5,  6,  9),	b3( 8,  9,  6),
	b3(10,  1,  0),	b3(10,  0,  6),	b3( 9,  5,  0),	b3( 5,  6,  0),	z3,	
	b3( 0,  3,  8),	b3( 5,  6, 10),	z3,				z3,				z3,	
	b3(10,  5,  6),	z3,				z3,				z3,				z3,	
	b3(11,  5, 10),	b3( 7,  5, 11),	z3,				z3,				z3,	
	b3(11,  5, 10),	b3(11,  7,  5),	b3( 8,  3,  0),	z3,				z3,	
	b3( 5, 11,  7),	b3( 5, 10, 11),	b3( 1,  9,  0),	z3,				z3,	
	b3(10,  7,  5),	b3(10, 11,  7),	b3( 9,  8,  1),	b3( 8,  3,  1),	z3,	
	b3(11,  1,  2),	b3(11,  7,  1),	b3( 7,  5,  1),	z3,				z3,	
	b3( 0,  8,  3),	b3( 1,  2,  7),	b3( 1,  7,  5),	b3( 7,  2, 11),	z3,	
	b3( 9,  7,  5),	b3( 9,  2,  7),	b3( 9,  0,  2),	b3( 2, 11,  7),	z3,	
	b3( 7,  5,  2),	b3( 7,  2, 11),	b3( 5,  9,  2),	b3( 3,  2,  8),	b3( 9,  8,  2),
	b3( 2,  5, 10),	b3( 2,  3,  5),	b3( 3,  7,  5),	z3,				z3,	
	b3( 8,  2,  0),	b3( 8,  5,  2),	b3( 8,  7,  5),	b3(10,  2,  5),	z3,	
	b3( 9,  0,  1),	b3( 5, 10,  3),	b3( 5,  3,  7),	b3( 3, 10,  2),	z3,	
	b3( 9,  8,  2),	b3( 9,  2,  1),	b3( 8,  7,  2),	b3(10,  2,  5),	b3( 7,  5,  2),
	b3( 1,  3,  5),	b3( 3,  7,  5),	z3,				z3,				z3,	
	b3( 0,  8,  7),	b3( 0,  7,  1),	b3( 1,  7,  5),	z3,				z3,	
	b3( 9,  0,  3),	b3( 9,  3,  5),	b3( 5,  3,  7),	z3,				z3,	
	b3( 9,  8,  7),	b3( 5,  9,  7),	z3,				z3,				z3,	
	b3( 5,  8,  4),	b3( 5, 10,  8),	b3(10, 11,  8),	z3,				z3,	
	b3( 5,  0,  4),	b3( 5, 11,  0),	b3( 5, 10, 11),	b3(11,  3,  0),	z3,	
	b3( 0,  1,  9),	b3( 8,  4, 10),	b3( 8, 10, 11),	b3(10,  4,  5),	z3,	
	b3(10, 11,  4),	b3(10,  4,  5),	b3(11,  3,  4),	b3( 9,  4,  1),	b3( 3,  1,  4),
	b3( 2,  5,  1),	b3( 2,  8,  5),	b3( 2, 11,  8),	b3( 4,  5,  8),	z3,	
	b3( 0,  4, 11),	b3( 0, 11,  3),	b3( 4,  5, 11),	b3( 2, 11,  1),	b3( 5,  1, 11),
	b3( 0,  2,  5),	b3( 0,  5,  9),	b3( 2, 11,  5),	b3( 4,  5,  8),	b3(11,  8,  5),
	b3( 9,  4,  5),	b3( 2, 11,  3),	z3,				z3,				z3,	
	b3( 2,  5, 10),	b3( 3,  5,  2),	b3( 3,  4,  5),	b3( 3,  8,  4),	z3,	
	b3( 5, 10,  2),	b3( 5,  2,  4),	b3( 4,  2,  0),	z3,				z3,	
	b3( 3, 10,  2),	b3( 3,  5, 10),	b3( 3,  8,  5),	b3( 4,  5,  8),	b3( 0,  1,  9),
	b3( 5, 10,  2),	b3( 5,  2,  4),	b3( 1,  9,  2),	b3( 9,  4,  2),	z3,	
	b3( 8,  4,  5),	b3( 8,  5,  3),	b3( 3,  5,  1),	z3,				z3,	
	b3( 0,  4,  5),	b3( 1,  0,  5),	z3,				z3,				z3,	
	b3( 8,  4,  5),	b3( 8,  5,  3),	b3( 9,  0,  5),	b3( 0,  3,  5),	z3,	
	b3( 9,  4,  5),	z3,				z3,				z3,				z3,	
	b3( 4, 11,  7),	b3( 4,  9, 11),	b3( 9, 10, 11),	z3,				z3,	
	b3( 0,  8,  3),	b3( 4,  9,  7),	b3( 9, 11,  7),	b3( 9, 10, 11),	z3,	
	b3( 1, 10, 11),	b3( 1, 11,  4),	b3( 1,  4,  0),	b3( 7,  4, 11),	z3,	
	b3( 3,  1,  4),	b3( 3,  4,  8),	b3( 1, 10,  4),	b3( 7,  4, 11),	b3(10, 11,  4),
	b3( 4, 11,  7),	b3( 9, 11,  4),	b3( 9,  2, 11),	b3( 9,  1,  2),	z3,	
	b3( 9,  7,  4),	b3( 9, 11,  7),	b3( 9,  1, 11),	b3( 2, 11,  1),	b3( 0,  8,  3),
	b3(11,  7,  4),	b3(11,  4,  2),	b3( 2,  4,  0),	z3,				z3,	
	b3(11,  7,  4),	b3(11,  4,  2),	b3( 8,  3,  4),	b3( 3,  2,  4),	z3,	
	b3( 2,  9, 10),	b3( 2,  7,  9),	b3( 2,  3,  7),	b3( 7,  4,  9),	z3,	
	b3( 9, 10,  7),	b3( 9,  7,  4),	b3(10,  2,  7),	b3( 8,  7,  0),	b3( 2,  0,  7),
	b3( 3,  7, 10),	b3( 3, 10,  2),	b3( 7,  4, 10),	b3( 1, 10,  0),	b3( 4,  0, 10),
	b3( 1, 10,  2),	b3( 8,  7,  4),	z3,				z3,				z3,	
	b3( 4,  9,  1),	b3( 4,  1,  7),	b3( 7,  1,  3),	z3,				z3,	
	b3( 4,  9,  1),	b3( 4,  1,  7),	b3( 0,  8,  1),	b3( 8,  7,  1),	z3,	
	b3( 4,  0,  3),	b3( 7,  4,  3),	z3,				z3,				z3,	
	b3( 4,  8,  7),	z3,				z3,				z3,				z3,	
	b3( 9, 10,  8),	b3(10, 11,  8),	z3,				z3,				z3,	
	b3( 3,  0,  9),	b3( 3,  9, 11),	b3(11,  9, 10),	z3,				z3,	
	b3( 0,  1, 10),	b3( 0, 10,  8),	b3( 8, 10, 11),	z3,				z3,	
	b3( 3,  1, 10),	b3(11,  3, 10),	z3,				z3,				z3,	
	b3( 1,  2, 11),	b3( 1, 11,  9),	b3( 9, 11,  8),	z3,				z3,	
	b3( 3,  0,  9),	b3( 3,  9, 11),	b3( 1,  2,  9),	b3( 2, 11,  9),	z3,	
	b3( 0,  2, 11),	b3( 8,  0, 11),	z3,				z3,				z3,	
	b3( 3,  2, 11),	z3,				z3,				z3,				z3,	
	b3( 2,  3,  8),	b3( 2,  8, 10),	b3(10,  8,  9),	z3,				z3,	
	b3( 9, 10,  2),	b3( 0,  9,  2),	z3,				z3,				z3,	
	b3( 2,  3,  8),	b3( 2,  8, 10),	b3( 0,  1,  8),	b3( 1, 10,  8),	z3,	
	b3( 1, 10,  2),	z3,				z3,				z3,				z3,	
	b3( 1,  3,  8),	b3( 9,  1,  8),	z3,				z3,				z3,	
	b3( 0,  9,  1),	z3,				z3,				z3,				z3,	
	b3( 0,  3,  8),	z3,				z3,				z3,				z3,	
	z3,				z3,				z3,				z3,				z3
);

vec3 get_cubePos(int i)
{
	return gl_in[0].gl_Position.xyz + vertOffsets[i];
}

float get_cubeVal(vec3 p)
{
	return texture(values, (p + 1.0) / 2.0).r;
}

ivec3 triTableValue(int i, int j)
{
	return tri_table[i * 5 + j];
	/* return texelFetch(triTableTex, ivec2(j, i), 0).a; */
}

vec3 vertInterp(float iso_level, vec3 v0, float l0, vec3 v1, float l1)
{
	return mix(v0, v1, (iso_level - l0) / (l1 - l0));
}

void main(void)
{
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
	if (cubeindex ==0 || cubeindex == 255)
		return;
	vec3 vertlist[12];
	/* Find the vertices where the surface intersects the cube */
	vertlist[0] = vertInterp(iso_level, cubesPos[0], cubeVal0, cubesPos[1], cubeVal1);
	vertlist[1] = vertInterp(iso_level, cubesPos[1], cubeVal1, cubesPos[2], cubeVal2);
	vertlist[2] = vertInterp(iso_level, cubesPos[2], cubeVal2, cubesPos[3], cubeVal3);
	vertlist[3] = vertInterp(iso_level, cubesPos[3], cubeVal3, cubesPos[0], cubeVal0);
	vertlist[4] = vertInterp(iso_level, cubesPos[4], cubeVal4, cubesPos[5], cubeVal5);
	vertlist[5] = vertInterp(iso_level, cubesPos[5], cubeVal5, cubesPos[6], cubeVal6);
	vertlist[6] = vertInterp(iso_level, cubesPos[6], cubeVal6, cubesPos[7], cubeVal7);
	vertlist[7] = vertInterp(iso_level, cubesPos[7], cubeVal7, cubesPos[4], cubeVal4);
	vertlist[8] = vertInterp(iso_level, cubesPos[0], cubeVal0, cubesPos[4], cubeVal4);
	vertlist[9] = vertInterp(iso_level, cubesPos[1], cubeVal1, cubesPos[5], cubeVal5);
	vertlist[10] = vertInterp(iso_level, cubesPos[2], cubeVal2, cubesPos[6], cubeVal6);
	vertlist[11] = vertInterp(iso_level, cubesPos[3], cubeVal3, cubesPos[7], cubeVal7);
	/* gl_FrontColor=vec4(cos(iso_level*5.0-0.5), sin(iso_level*5.0-0.5), 0.5, 1.0); */
	int i=0;
	/* Strange bug with this way, uncomment to test */
	/* for (i=0; triTableValue(cubeindex, i)!=-1; i+=3) { */
	for(i = 0; i < 15; i += 3)
	{
		ivec3 I = triTableValue(cubeindex, i);
		/* int i0 = triTableValue(cubeindex, i); */
		/* int i1 = triTableValue(cubeindex, i + 1); */
		/* int i2 = triTableValue(cubeindex, i + 2); */
		if(I.x != -1)
		{
			gl_Position = vec4(vertlist[I.x], 1);
			vertex_position = gl_Position.xyz;
			EmitVertex();

			gl_Position = vec4(vertlist[I.y], 1);
			vertex_position = gl_Position.xyz;
			EmitVertex();

			gl_Position = vec4(vertlist[I.z], 1);
			vertex_position = gl_Position.xyz;
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
