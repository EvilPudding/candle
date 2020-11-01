#include "field.h"
#include "model.h"
#include "node.h"
#include "spatial.h"
#include "light.h"
#include <utils/drawable.h>
#include <utils/nk.h>
#include <utils/shader.h>
#include <systems/editmode.h>
#include <candle.h>

static float g_iso_level = 0.9;

static const vec3_t vert_offsets[8] = {
	{ 0.0f, 0.0f, 0.0f },
	{ 1.0f, 0.0f, 0.0f },
	{ 1.0f, 1.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f },
	{ 1.0f, 0.0f, 1.0f },
	{ 1.0f, 1.0f, 1.0f },
	{ 0.0f, 1.0f, 1.0f }
};

#define b3(r,g,b) {r, g, b}
#define z3 {-1, -1, -1}
const static ivec3_t tri_table[256 * 5] = {
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
};

static vec4_t get_cubeColor(vec3_t p, float time)
{
	float t = time * 4.0f;
	float r0 = (abs(cos(t * .3)) * 2. + .6) / 0.3;
	float r1 = (abs(sin(t * .35)) * 2. + .6) / 0.3;
	vec3_t origin = vec3(0., 2.5, 0.);
	vec3_t p1 = vec3_scale(vec3(sin(t), cos(t), sin(t * .612)), r0);
	vec3_t p2 = vec3_scale(vec3(sin(t * 1.1), cos(t * 1.2), sin(t * .512)), r1);
	vec3_t p_diff = vec3_sub(p, origin);
	vec3_t p_diff1 = vec3_sub(p_diff, p1);
	vec3_t p_diff2 = vec3_sub(p_diff, p2);
	float val = 1. / (vec3_dot(p_diff, p_diff) + 1.);
	float val2 = (.6 / (vec3_dot(p_diff1, p_diff1) + 0.7)) * 1.3f;
	float val3 = (.5 / (vec3_dot(p_diff2, p_diff2) + 0.9)) * 2.0f;
	return vec4(val, val2, val3, 1.0);
}

static vec3_t get_cubePos(c_field_t *self, int i, vec3_t cell)
{
	return vec3_add(cell, vec3_scale(vert_offsets[i], self->cell_size));
}

static float get_cube_val(vec3_t p, float time)
{
	vec4_t color = get_cubeColor(p, time);
	return color.x + color.y + color.z;
}

vec3_t isonormal(c_field_t *self, vec3_t pos)
{
	vec3_t nor;
	float offset = self->cell_size * 0.5;
	nor.x = get_cube_val(vec3_add(pos, vec3(offset, 0.0, 0.0)), self->time)
		  - get_cube_val(vec3_sub(pos, vec3(offset, 0.0, 0.0)), self->time);
	nor.y = get_cube_val(vec3_add(pos, vec3(0.0, offset, 0.0)), self->time)
		  - get_cube_val(vec3_sub(pos, vec3(0.0, offset, 0.0)), self->time);
	nor.z = get_cube_val(vec3_add(pos, vec3(0.0, 0.0, offset)), self->time)
		  - get_cube_val(vec3_sub(pos, vec3(0.0, 0.0, offset)), self->time);
	return vec3_inv(vec3_norm(nor));
}

static ivec3_t triTable_value(int i, int j)
{
	return tri_table[i * 5 + j];
}

static vec3_t vert_lerp(float iso_level, vec3_t v0, float l0, vec3_t v1, float l1)
{
	return vec3_mix(v0, v1, (iso_level - l0) / (l1 - l0));
}


static int c_field_position_changed(c_field_t *self);

static vs_t *g_field_vs;

vs_t *field_vs()
{
	if (!g_field_vs)
	{
		g_field_vs = vs_new("field", false, 1, geometry_modifier_new("#include \"candle:marching.glsl\"\n"));
	}
	return g_field_vs;
}

static void c_field_init(c_field_t *self)
{
	drawable_init(&self->draw, ref("visible"));
	drawable_set_entity(&self->draw, c_entity(self));
	drawable_add_group(&self->draw, ref("selectable"));
	drawable_set_entity(&self->draw, c_entity(self));

	self->mesh = mesh_new();
	self->mesh->has_texcoords = false;
	self->mesh->smooth_angle = 1.0f;
	self->mesh->static_normals = true;

	drawable_set_mesh(&self->draw, self->mesh);

	self->visible = 1;
}

void c_field_update_mat(c_field_t *self)
{
	if (self->mat)
	{
		int transp = mat_is_transparent(self->mat);
		if (self->draw.bind[0].grp == ref("transparent")
				|| self->draw.bind[0].grp == ref("visible"))
		{
			drawable_set_group(&self->draw, transp ? ref("transparent") : ref("visible"));
		}
	}

	drawable_set_mat(&self->draw, self->mat);
}

void c_field_set_mat(c_field_t *self, mat_t *mat)
{
	if (!mat)
	{
		mat = g_mats[rand()%g_mats_num];
	}
	if (self->mat != mat)
	{
		self->mat = mat;
		c_field_update_mat(self);
	}

}

int c_field_menu(c_field_t *self, void *ctx)
{
	int changes = 0;
	if (self->mat && self->mat->name[0] != '_')
	{
		changes |= mat_menu(self->mat, ctx);
		c_field_update_mat(self);
	}
	if (changes)
	{
		drawable_model_changed(&self->draw);
	}
	return CONTINUE;
}

void c_field_pre_draw(c_field_t *self, shader_t *shader)
{

	/* uint32_t loc = shader_cached_uniform(shader, ref("values")); */
	/* glUniform1i(loc, 16); */
	/* glActiveTexture(GL_TEXTURE0 + 16); */
	/* texture_bind(self->values, 0); */
	glerr();
}


static void c_field_update_cell(c_field_t *self, vec3_t cell)
{
	int i;
	int cubeindex = 0;
	vec3_t cubes_pos[8];
	vec3_t vertlist[12];
	float cube_val[8];

	cubes_pos[0] = get_cubePos(self, 0, cell);
	cubes_pos[1] = get_cubePos(self, 1, cell);
	cubes_pos[2] = get_cubePos(self, 2, cell);
	cubes_pos[3] = get_cubePos(self, 3, cell);
	cubes_pos[4] = get_cubePos(self, 4, cell);
	cubes_pos[5] = get_cubePos(self, 5, cell);
	cubes_pos[6] = get_cubePos(self, 6, cell);
	cubes_pos[7] = get_cubePos(self, 7, cell);

	cube_val[0] = get_cube_val(cubes_pos[0], self->time);
	cube_val[1] = get_cube_val(cubes_pos[1], self->time);
	cube_val[2] = get_cube_val(cubes_pos[2], self->time);
	cube_val[3] = get_cube_val(cubes_pos[3], self->time);
	cube_val[4] = get_cube_val(cubes_pos[4], self->time);
	cube_val[5] = get_cube_val(cubes_pos[5], self->time);
	cube_val[6] = get_cube_val(cubes_pos[6], self->time);
	cube_val[7] = get_cube_val(cubes_pos[7], self->time);

	cubeindex  = (cube_val[0] < g_iso_level);
	cubeindex += (cube_val[1] < g_iso_level) * 2;
	cubeindex += (cube_val[2] < g_iso_level) * 4;
	cubeindex += (cube_val[3] < g_iso_level) * 8;
	cubeindex += (cube_val[4] < g_iso_level) * 16;
	cubeindex += (cube_val[5] < g_iso_level) * 32;
	cubeindex += (cube_val[6] < g_iso_level) * 64;
	cubeindex += (cube_val[7] < g_iso_level) * 128;
	if (cubeindex == 0 || cubeindex == 255)
		return;

	vertlist[0] = vert_lerp(g_iso_level, cubes_pos[0], cube_val[0], cubes_pos[1], cube_val[1]);
	vertlist[1] = vert_lerp(g_iso_level, cubes_pos[1], cube_val[1], cubes_pos[2], cube_val[2]);
	vertlist[2] = vert_lerp(g_iso_level, cubes_pos[2], cube_val[2], cubes_pos[3], cube_val[3]);
	vertlist[3] = vert_lerp(g_iso_level, cubes_pos[3], cube_val[3], cubes_pos[0], cube_val[0]);
	vertlist[4] = vert_lerp(g_iso_level, cubes_pos[4], cube_val[4], cubes_pos[5], cube_val[5]);
	vertlist[5] = vert_lerp(g_iso_level, cubes_pos[5], cube_val[5], cubes_pos[6], cube_val[6]);
	vertlist[6] = vert_lerp(g_iso_level, cubes_pos[6], cube_val[6], cubes_pos[7], cube_val[7]);
	vertlist[7] = vert_lerp(g_iso_level, cubes_pos[7], cube_val[7], cubes_pos[4], cube_val[4]);
	vertlist[8] = vert_lerp(g_iso_level, cubes_pos[0], cube_val[0], cubes_pos[4], cube_val[4]);
	vertlist[9] = vert_lerp(g_iso_level, cubes_pos[1], cube_val[1], cubes_pos[5], cube_val[5]);
	vertlist[10] = vert_lerp(g_iso_level, cubes_pos[2], cube_val[2], cubes_pos[6], cube_val[6]);
	vertlist[11] = vert_lerp(g_iso_level, cubes_pos[3], cube_val[3], cubes_pos[7], cube_val[7]);

	for (i = 0; i < 5; i++)
	{
		const ivec3_t I = triTable_value(cubeindex, i);
		if(I.x != -1)
		{
			int i0 = mesh_assert_vert(self->mesh, vertlist[I.x]);
			int i1 = mesh_assert_vert(self->mesh, vertlist[I.y]);
			int i2 = mesh_assert_vert(self->mesh, vertlist[I.z]);

			vertex_t *v0 = m_vert(self->mesh, i0);
			vertex_t *v1 = m_vert(self->mesh, i1);
			vertex_t *v2 = m_vert(self->mesh, i2);
			v0->color = get_cubeColor(vertlist[I.x], self->time);
			v1->color = get_cubeColor(vertlist[I.x], self->time);
			v2->color = get_cubeColor(vertlist[I.x], self->time);

			mesh_add_triangle(self->mesh,
			                  i0, isonormal(self, vertlist[I.x]), Z2,
			                  i1, isonormal(self, vertlist[I.y]), Z2,
			                  i2, isonormal(self, vertlist[I.z]), Z2);
			/* mesh_add_triangle(self->mesh, */
			                  /* i0, Z3, Z2, */
			                  /* i1, Z3, Z2, */
			                  /* i2, Z3, Z2); */
		}
		else
		{
			break;
		}
	}
}

static void c_field_update_mesh(c_field_t *self)
{
	vec3_t size;
	uvec3_t segments;
	unsigned int x, y, z;

	size = vec3_sub(self->end, self->start);
	segments = uvec3(size.x / self->cell_size,
	                 size.y / self->cell_size,
	                 size.z / self->cell_size);

	mesh_lock(self->mesh);
	mesh_clear(self->mesh);
	for (x = 0; x <= segments.x; x++)
	{
		const float plane = self->start.x + ((float)x) * (size.x / segments.x);
		for (y = 0; y <= segments.y; y++)
		{
			const float row = self->start.y + ((float)y) * (size.y / segments.y);
			for (z = 0; z <= segments.z; z++)
			{
				const vec3_t cell = vec3(plane, row,
				                         self->start.z + ((float)z) * (size.z / segments.z));
				c_field_update_cell(self, cell);
			}
		}
	}

	mesh_unlock(self->mesh);
}

int c_field_update(c_field_t *self)
{
	self->time += 0.01f;
	if (!self->use_geometry_shader)
	{
		c_field_update_mesh(self);
	}
	return CONTINUE;
}


c_field_t *c_field_new(mat_t *mat, vec3_t start, vec3_t end, float cell_size,
                       int cast_shadow)
{
	/* uint32_t x, y, z; */
	c_field_t *self = component_new(ct_field);
	self->start = start;
	self->end = end;
	self->cell_size = cell_size;

	self->cast_shadow = cast_shadow;

	if (self->use_geometry_shader)
	{
		vec3_t size;
		uvec3_t segments;
		size = vec3_sub(end, start);
		segments = uvec3(size.x / cell_size + 1,
		                 size.y / cell_size + 1,
		                 size.z / cell_size + 1);

		mesh_point_grid(self->mesh, start, size, segments);
		drawable_set_callback(&self->draw, (draw_cb)c_field_pre_draw, self);
		drawable_set_vs(&self->draw, field_vs());
		if (cast_shadow)
			drawable_add_group(&self->draw, ref("shadow"));
	}
	else
	{
		drawable_set_vs(&self->draw, model_vs());
	}

	c_field_set_mat(self, mat);

	c_field_position_changed(self);
	return self;
}

int c_field_created(c_field_t *self)
{
	entity_signal_same(c_entity(self), ref("mesh_changed"), NULL, NULL);
	return CONTINUE;
}

static int c_field_position_changed(c_field_t *self)
{
	c_node_t *node = c_node(self);
	c_node_update_model(node);
	drawable_set_transform(&self->draw, node->model);
	drawable_set_entity(&self->draw, node->unpack_inheritance);

	return CONTINUE;
}

void c_field_destroy(c_field_t *self)
{
	drawable_set_mesh(&self->draw, NULL);
}

void ct_field(ct_t *self)
{
	ct_init(self, "field", sizeof(c_field_t));
	ct_set_init(self, (init_cb)c_field_init);
	ct_set_destroy(self, (destroy_cb)c_field_destroy);
	ct_add_dependency(self, ct_node);

	ct_add_listener(self, ENTITY, 0, ref("entity_created"), c_field_created);

	ct_add_listener(self, WORLD, 0, ref("component_menu"), c_field_menu);

	ct_add_listener(self, ENTITY, 0, ref("node_changed"), c_field_position_changed);

	ct_add_listener(self, WORLD, 1, ref("world_pre_draw"), c_field_update);
}

