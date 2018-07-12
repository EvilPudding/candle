#ifndef MAFS_H
#define MAFS_H

#ifndef __OPENCL_C_VERSION__
#include <math.h>
#include <float.h>
#define CONST static const
void sincosf(float x, float *sin, float *cos);
#else
#define nextafterf nextafter
#define sqrtf sqrt
#define cbrtf cbrt
#define powf pow
#define sinf sin
#define cosf cos
#define CONST __constant
#endif

#define n_t float

#define MAFS_DEFINE_STRUCTS(n_t, type) \
typedef struct type##2_t { union { \
	struct { n_t x, y; }; \
	struct { n_t u, v; }; \
	n_t _[2]; \
}; } type##2_t; \
typedef struct type##3_t { union { \
	struct { n_t x, y, z; }; \
	struct { n_t r, g, b; }; \
	struct { n_t u, v, w; }; \
	n_t _[3]; \
	type##2_t xy; \
}; } type##3_t; \
typedef struct type##4_t { union { \
	struct { n_t x, y, z, w; }; \
	struct { n_t r, g, b, a; }; \
	n_t _[4]; \
	type##2_t xy; \
	type##3_t xyz; \
}; } type##4_t; \

#ifndef __OPENCL_C_VERSION__

#define MAFS_DEFINE_CONSTRUCTOR(n_t, type) \
static inline type##2_t type##2(n_t x, n_t y) { return (type##2_t){.x = x, .y = y}; } \
static inline type##2_t type##2_n_n(n_t x, n_t y) { return type##2(x, y); } \
static inline type##2_t type##2_n(n_t f) { return type##2(f, f); } \
static inline type##2_t type##2_v2(type##2_t v) { return v; } \
static inline type##3_t type##3(n_t x, n_t y, n_t z) { return ((type##3_t){{{x, y, z}}}); } \
static inline type##3_t type##3_n_n_n(n_t x, n_t y, n_t z) { return type##3(x, y, z); } \
static inline type##3_t type##3_v2_n(type##2_t xy, n_t z) { return type##3(xy.x, xy.y, z); } \
static inline type##3_t type##3_n_v2(n_t x, type##2_t yz) { return type##3(x, yz.x, yz.y); } \
static inline type##3_t type##3_n(n_t f) { return type##3(f, f, f); } \
static inline type##3_t type##3_v3(type##3_t v) { return v; } \
static inline type##4_t type##4(n_t x, n_t y, n_t z, n_t w) { return ((type##4_t){{{x, y, z, w}}}); } \
static inline type##4_t type##4_n_n_n_n(n_t x, n_t y, n_t z, n_t w) { return type##4(x, y, z, w); } \
static inline type##4_t type##4_v3_n(type##3_t xyz, n_t w) { return type##4(xyz.x, xyz.y, xyz.z, w); } \
static inline type##4_t type##4_n_v3(n_t x, type##3_t yzw) { return type##4(x, yzw.x, yzw.y, yzw.z); } \
static inline type##4_t type##4_n(n_t f) { return type##4(f, f, f, f); } \
static inline type##4_t type##4_v4(type##4_t v) { return v; }

#else

#define MAFS_DEFINE_CONSTRUCTOR(n_t, type) \
static inline type##2_t __attribute__((overloadable)) type##2(n_t x, n_t y) { return (type##2_t){ .x = x, . y = y }; } \
static inline type##2_t __attribute__((overloadable)) type##2(n_t f) { return type##2( f, f ); } \
static inline type##2_t __attribute__((overloadable)) type##2(type##2_t v) { return v; } \
static inline type##3_t __attribute__((overloadable)) type##3(n_t x, n_t y, n_t z) { return (type##3_t){ .x = x, .y = y, .z = z }; } \
static inline type##3_t __attribute__((overloadable)) type##3(type##2_t xy, n_t z) { return type##3( xy.x, xy.y, z ); } \
static inline type##3_t __attribute__((overloadable)) type##3(n_t x, type##2_t yz) { return type##3( x, yz.x, yz.y ); } \
static inline type##3_t __attribute__((overloadable)) type##3(n_t f) { return type##3( f, f, f ); } \
static inline type##3_t __attribute__((overloadable)) type##3(type##3_t v) { return v; } \
static inline type##4_t __attribute__((overloadable)) type##4(n_t x, n_t y, n_t z, n_t w) { return (type##4_t){ .x = x, .y = y, .z = z, .w = w}; } \
static inline type##4_t __attribute__((overloadable)) type##4(type##3_t xyz, n_t w) { return type##4( xyz.x, xyz.y, xyz.z, w ); } \
static inline type##4_t __attribute__((overloadable)) type##4(n_t x, type##3_t yzw) { return type##4( x, yzw.x, yzw.y, yzw.z ); } \
static inline type##4_t __attribute__((overloadable)) type##4(n_t f) { return type##4( f, f, f, f ); } \
static inline type##4_t __attribute__((overloadable)) type##4(type##4_t v) { return v; }

#endif

#ifndef __OPENCL_C_VERSION__
#include <stdio.h>

#endif

#define vec_oper(n, r, oper) \
do { int x; for(x=0; x<n; ++x) { r._[x] = oper; } } while(0)


#define MAFS_DEFINE_VEC_PRINT(n_t, type, format, n) \
static inline void type##n##_print(type##n##_t const a) \
{ \
	int i; \
	printf("vec" #n "(" format, a._[0]); \
	for(i=1; i<n; ++i) \
		printf(", " format, a._[i]); \
	printf(")\n"); \
}

#define MAFS_DEFINE_VEC(n_t, type, n, sqrt, pow, floor, round) \
static inline type##n##_t type##n##_add_number(type##n##_t const a, n_t b) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i] + b; \
	return r; \
} \
static inline type##n##_t type##n##_div(type##n##_t const a, type##n##_t const b) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i] / b._[i]; \
	return r; \
} \
static inline type##n##_t type##n##_mul(type##n##_t const a, type##n##_t const b) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i] * b._[i]; \
	return r; \
} \
static inline type##n##_t type##n##_div_number(type##n##_t const a, n_t b) \
{ \
	type##n##_t r; \
	int i; \
	n_t inv = 1.0f / b; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i] * inv; \
	return r; \
} \
static inline type##n##_t type##n##_mul_number(type##n##_t const a, n_t b) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i] * b; \
	return r; \
} \
static inline type##n##_t type##n##_sub_number(type##n##_t const a, n_t b) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i] - b; \
	return r; \
} \
static inline int type##n##_equals(type##n##_t const a, type##n##_t const b) \
{ \
	int i; \
	for(i=0; i<n; ++i) \
		if(a._[i] != b._[i]) return 0; \
	return 1; \
} \
static inline type##n##_t type##n##_add(type##n##_t const a, type##n##_t const b) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i] + b._[i]; \
	return r; \
} \
static inline type##n##_t type##n##_sub(type##n##_t const a, type##n##_t const b) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i] - b._[i]; \
	return r; \
} \
static inline type##n##_t type##n##_scale(type##n##_t const v, n_t const s) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = v._[i] * s; \
	return r; \
} \
static inline type##n##_t type##n##_inv(type##n##_t const a) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = -a._[i]; \
	return r; \
} \
static inline n_t type##n##_dot(type##n##_t const a, type##n##_t const b) \
{ \
	n_t p = 0.0f; \
	int i; \
	for(i=0; i<n; ++i) \
		p += b._[i]*a._[i]; \
	return p; \
} \
static inline int type##n##_null(type##n##_t const v) \
{ \
	int i; \
	for(i=0; i<n; ++i) \
		if(v._[i]) return 0; \
	return 1; \
} \
static inline n_t type##n##_len_square(type##n##_t const v) \
{ \
	return type##n##_dot(v,v); \
} \
static inline n_t type##n##_len(type##n##_t const v) \
{ \
	return sqrt(type##n##_dot(v,v)); \
} \
static inline n_t type##n##_dist(type##n##_t const v1, type##n##_t const v2) \
{ \
	type##n##_t t = type##n##_sub(v1, v2); \
	return type##n##_len(t); \
} \
static inline type##n##_t type##n##_get_unit(const type##n##_t v) \
{ \
    n_t len = type##n##_len(v); \
    if(len < FLT_EPSILON ) return v; \
    return type##n##_div_number(v, len); \
} \
static inline type##n##_t type##n##_norm(type##n##_t const v) \
{ \
	return type##n##_scale(v, 1.0 / type##n##_len(v)); \
} \
static inline type##n##_t type##n##_abs(type##n##_t a) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = fabs(a._[i]); \
	return r; \
} \
static inline type##n##_t type##n##_round(type##n##_t a) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = round(a._[i]); \
	return r; \
} \
static inline type##n##_t type##n##_floor(type##n##_t a) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = floor(a._[i]); \
	return r; \
} \
static inline type##n##_t type##n##_fract(type##n##_t a) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i] - floor(a._[i]); \
	return r; \
} \
static inline type##n##_t type##n##_min(type##n##_t a, type##n##_t b) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i] < b._[i] ? \
				a._[i] : b._[i]; \
	return r; \
} \
static inline type##n##_t type##n##_clamp(type##n##_t a, n_t min, n_t max) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i]>min ? \
				(a._[i]<max?a._[i]:max) : min; \
	return r; \
} \
static inline type##n##_t type##n##_max(type##n##_t a, type##n##_t b) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = a._[i]>b._[i] ? \
				a._[i] : b._[i]; \
	return r; \
} \
static inline type##n##_t type##n##_mix(type##n##_t x, type##n##_t y, n_t a) \
{ \
	type##n##_t r; \
	int i; \
	n_t inv = 1.0f - a; \
	for(i=0; i<n; ++i) \
		r._[i] = x._[i] * inv + y._[i] * a; \
	return r; \
} \
static inline type##n##_t type##n##_step(type##n##_t edge, type##n##_t x) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = !(x._[i] < edge._[i]); \
	return r; \
} \
static inline type##n##_t type##n##_step_number(n_t edge, type##n##_t x) \
{ \
	type##n##_t r; \
	int i; \
	for(i=0; i<n; ++i) \
		r._[i] = !(x._[i] < edge); \
	return r; \
}

#define MAFS_DEFINE_SPECIFIC(n_t, type, sqrt, pow) \
static inline n_t type##2_cross(type##2_t const a, type##2_t const b) \
{ \
	return a.x * b.y - a.y * b.x; \
} \
static inline type##3_t type##3_cross(type##3_t const a, type##3_t const b) \
{ \
	type##3_t r; \
	r.x = a.y*b.z - a.z*b.y; \
	r.y = a.z*b.x - a.x*b.z; \
	r.z = a.x*b.y - a.y*b.x; \
	return r; \
} \
static inline type##4_t type##4_cross(type##4_t const a, type##4_t const b, \
		type##4_t const c) \
{ \
	type##4_t r; \
	r.x = -a.w*b.z*c.y + a.z*b.w*c.y + a.w*b.y*c.z \
		  -a.y*b.w*c.z - a.z*b.y*c.w + a.y*b.z*c.w; \
	r.y =  a.w*b.z*c.x - a.z*b.w*c.x - a.w*b.x*c.z \
		  +a.x*b.w*c.z + a.z*b.x*c.w - a.x*b.z*c.w; \
	r.z = -a.w*b.y*c.x + a.y*b.w*c.x + a.w*b.x*c.y \
		  -a.x*b.w*c.y - a.y*b.x*c.w + a.x*b.y*c.w; \
	r.w =  a.z*b.y*c.x - a.y*b.z*c.x - a.z*b.x*c.y \
		  +a.x*b.z*c.y + a.y*b.x*c.z - a.x*b.y*c.z; \
	return r; \
} \
static inline type##3_t type##3_double_cross(const type##3_t v1, const type##3_t v2) \
{ \
	return type##3_cross(type##3_cross(v1, v2), v1); \
} \
static inline type##3_t type##3_reflect(type##3_t const v, type##3_t const n) \
{ \
	type##3_t r; \
	n_t p  = 2.f*type##3_dot(v, n); \
	int i; \
	for(i=0;i<3;++i) \
		r._[i] = v._[i] - p*n._[i]; \
	return r; \
} \
/* static inline type##4_t type##_cross(type##4_t a, type##4_t b) \ */ \
/* { \ */ \
/* 	type##4_t r; \ */ \
/* 	r.x = a.y * b.z - a.z * b.y; \ */ \
/* 	r.y = a.z * b.x - a.x * b.z; \ */ \
/* 	r.z = a.x * b.y - a.y * b.x; \ */ \
/* 	r.w = 1.f; \ */ \
/* 	return r; \ */ \
/* } \ */ \
static inline type##4_t type##4_reflect(type##4_t v, type##4_t n) \
{ \
	type##4_t r; \
	n_t p  = 2.f*type##4_dot(v, n); \
	int i; \
	for(i=0;i<4;++i) \
		r._[i] = v._[i] - p*n._[i]; \
	return r; \
} \
static inline type##3_t type##3_perpendicular(const type##3_t v, const type##3_t p) \
{ \
    const n_t  pp = p.x*p.x + p.y*p.y + p.z*p.z; \
    if(pp > 0.0) \
	{ \
        const n_t  c = (v.x*p.x + v.y*p.y + v.z*p.z) / pp; \
        const type##3_t  result = type##3( v.x - c*p.x, \
                                 v.y - c*p.y, \
                                 v.z - c*p.z ); \
        return result; \
    } \
	return v; \
} \
static inline type##4_t type##4_unit(const type##4_t v) \
{ \
    const n_t  vv = v.x*v.x + v.y*v.y + v.z*v.z + v.w*v.w; \
    if(vv > 0.0) \
	{ \
        const n_t  d = sqrt(vv); \
		return type##4( v.x / d, v.y / d, v.z / d, v.w / d ); \
    } \
	return type##4(0.0, 0.0, 0.0, 0.0); \
} \
static inline type##3_t type##3_unit(const type##3_t v) \
{ \
    const n_t  vv = v.x*v.x + v.y*v.y + v.z*v.z; \
    if(vv > 0.0) \
	{ \
        const n_t  d = sqrt(vv); \
		return type##3( v.x / d, v.y / d, v.z / d ); \
    } \
	return type##3(0.0, 0.0, 0.0); \
} \
static inline type##2_t type##2_rotate(const type##2_t v, const n_t cosa, const n_t sina) \
{ \
	return type##2(v.x*cosa - v.y*sina, v.y*cosa + v.x*sina); \
} \
static inline type##3_t type##3_rotate(const type##3_t v, const type##3_t a, \
                                 const n_t cosa, const n_t sina) \
{ \
    const type##3_t  vxa = type##3_cross(v, a); \
    const n_t ca = (1 - cosa) * type##3_dot(v, a); \
	return type##3_sub( \
		type##3_add(type##3_scale(v, cosa), type##3_scale(a, ca)), \
		type##3_scale(vxa, sina) \
	); \
}


#define MAFS_DEFINE_TYPE(n_t, type, format, sqrt, pow, floor, round) \
	MAFS_DEFINE_STRUCTS(n_t, type) \
	MAFS_DEFINE_CONSTRUCTOR(n_t, type) \
	MAFS_DEFINE_VEC(n_t, type, 2, sqrt, pow, floor, round) \
	MAFS_DEFINE_VEC(n_t, type, 3, sqrt, pow, floor, round) \
	MAFS_DEFINE_VEC(n_t, type, 4, sqrt, pow, floor, round) \
	MAFS_DEFINE_VEC_PRINT(n_t, type, format, 2) \
	MAFS_DEFINE_VEC_PRINT(n_t, type, format, 3) \
	MAFS_DEFINE_VEC_PRINT(n_t, type, format, 4) \
	MAFS_DEFINE_SPECIFIC(n_t, type, sqrt, pow)

MAFS_DEFINE_STRUCTS(unsigned int, uvec)
MAFS_DEFINE_CONSTRUCTOR(unsigned int, uvec)

MAFS_DEFINE_TYPE(float, vec, "%f", sqrtf, powf, floorf, roundf)
MAFS_DEFINE_TYPE(double, d, "%lf", sqrt, pow, floor, round)

typedef struct mat4_t { union {
	struct { vec4_t a, b, c, d; };
	vec4_t _[4];
}; } mat4_t;

static inline mat4_t mat4(void)
{
	mat4_t M;
	int i, j;
	for(i=0; i<4; ++i)
		for(j=0; j<4; ++j)
			M._[i]._[j] = i==j ? 1.0f : 0.0f;
	return M;
}
static inline vec4_t mat4_row(mat4_t M, int i)
{
	vec4_t r;
	int k;
	/* return M._[i]; TODO? */
	for(k=0; k<4; ++k)
		r._[k] = M._[k]._[i];
	return r;
}
static inline vec4_t mat4_col(mat4_t M, int i)
{
	vec4_t r;
	int k;
	for(k=0; k<4; ++k)
		r._[k] = M._[i]._[k];
	return r;
}
static inline mat4_t mat4_transpose(mat4_t N)
{
	mat4_t M;
	int i, j;
	for(j=0; j<4; ++j)
		for(i=0; i<4; ++i)
			M._[i]._[j] = N._[j]._[i];
	return M;
}
static inline mat4_t mat4_add(mat4_t a, mat4_t b)
{
	mat4_t M;
	int i;
	for(i=0; i<4; ++i)
		M._[i] = vec4_add(a._[i], b._[i]);
	return M;
}
static inline mat4_t mat4_sub(mat4_t a, mat4_t b)
{
	mat4_t M;
	int i;
	for(i=0; i<4; ++i)
		M._[i] = vec4_sub(a._[i], b._[i]);
	return M;
}
static inline mat4_t mat4_scale(mat4_t a, n_t k)
{
	mat4_t M;
	int i;
	for(i=0; i<4; ++i)
		M._[i] = vec4_scale(a._[i], k);
	return M;
}
static inline mat4_t mat4_scale_aniso(mat4_t a, vec3_t s)
{
	mat4_t M;
	int i;
	M._[0] = vec4_scale(a._[0], s.x);
	M._[1] = vec4_scale(a._[1], s.y);
	M._[2] = vec4_scale(a._[2], s.z);
	for(i = 0; i < 4; ++i) {
		M._[3]._[i] = a._[3]._[i];
	}
	return M;
}
static inline void mat4_print(mat4_t M)
{
	printf("%f	%f	%f	%f\n", M._[0].x, M._[0].y, M._[0].z, M._[0].w);
	printf("%f	%f	%f	%f\n", M._[1].x, M._[1].y, M._[1].z, M._[1].w);
	printf("%f	%f	%f	%f\n", M._[2].x, M._[2].y, M._[2].z, M._[2].w);
	printf("%f	%f	%f	%f\n", M._[3].x, M._[3].y, M._[3].z, M._[3].w);
}
static inline mat4_t mat4_mul(mat4_t a, mat4_t b)
{
	mat4_t M;
	int k, r, c;
	for(c=0; c<4; ++c) for(r=0; r<4; ++r) {
		M._[c]._[r] = 0.f;
		for(k=0; k<4; ++k)
			M._[c]._[r] += a._[k]._[r] * b._[c]._[k];
	}
	return M;
}
static inline vec4_t mat4_mul_vec4(mat4_t M, vec4_t v)
{
	vec4_t r;
	int i, j;
	for(j=0; j<4; ++j) {
		r._[j] = 0.f;
		for(i=0; i<4; ++i)
			r._[j] += M._[i]._[j] * v._[i];
	}
	return r;
}
static inline mat4_t mat4_translate(vec3_t pos)
{
	mat4_t T = mat4();
	T._[3]._[0] = pos.x;
	T._[3]._[1] = pos.y;
	T._[3]._[2] = pos.z;
	return T;
}
static inline mat4_t mat4_translate_in_place(mat4_t M, vec3_t pos)
{
	vec4_t t = vec4(pos.x, pos.y, pos.z, 0);
	vec4_t r;
	int i;
	for (i = 0; i < 4; ++i) {
		r = mat4_row(M, i);
		M._[3]._[i] += vec4_dot(r, t);
	}
	return M;
}
static inline mat4_t mat4_from_vec3_mul_outer(vec3_t a, vec3_t b)
{
	mat4_t M;
	int i, j;
	for(i=0; i<4; ++i) for(j=0; j<4; ++j)
		M._[i]._[j] = i<3 && j<3 ? a._[i] * b._[j] : 0.f;
	return M;
}
static inline mat4_t mat4_rotate(mat4_t M, n_t x, n_t y, n_t z, n_t angle)
{
	n_t s = sinf(angle);
	n_t c = cosf(angle);
	vec3_t u = vec3(x, y, z);

	if(vec3_len(u) > 1e-4) {
		u = vec3_norm(u);
		mat4_t T = mat4_from_vec3_mul_outer(u, u);

		mat4_t S = {._={
			vec4(    0,  u.z, -u.y, 0),
			vec4(-u.z,     0,  u.x, 0),
			vec4( u.y, -u.x,     0, 0),
			vec4(    0,     0,     0, 0)
		}};
		S = mat4_scale(S, s);

		mat4_t C = mat4();
		C = mat4_sub(C, T);

		C = mat4_scale(C, c);

		T = mat4_add(T, C);
		T = mat4_add(T, S);

		T._[3]._[3] = 1.0f;		
		return mat4_mul(M, T);
	} else {
		return M;
	}
}
static inline mat4_t mat4_rotate_X(mat4_t M, n_t angle)
{
	n_t s = sinf(angle);
	n_t c = cosf(angle);
	mat4_t R = {._={
		vec4(1.f, 0.f, 0.f, 0.f),
		vec4(0.f,   c,   s, 0.f),
		vec4(0.f,  -s,   c, 0.f),
		vec4(0.f, 0.f, 0.f, 1.f)
	}};
	return mat4_mul(M, R);
}
static inline mat4_t mat4_rotate_Y(mat4_t M, n_t angle)
{
	n_t s = sinf(angle);
	n_t c = cosf(angle);
	mat4_t R = {._={
		vec4(   c, 0.f,   s, 0.f),
		vec4( 0.f, 1.f, 0.f, 0.f),
		vec4(  -s, 0.f,   c, 0.f),
		vec4( 0.f, 0.f, 0.f, 1.f)
	}};
	return mat4_mul(M, R);
}
static inline mat4_t mat4_rotate_Z(mat4_t Q, mat4_t M, n_t angle)
{
	n_t s = sinf(angle);
	n_t c = cosf(angle);
	mat4_t R = {._={
		vec4(   c,   s, 0.f, 0.f),
		vec4(  -s,   c, 0.f, 0.f),
		vec4( 0.f, 0.f, 1.f, 0.f),
		vec4( 0.f, 0.f, 0.f, 1.f)
	}};
	return mat4_mul(M, R);
}
static inline mat4_t mat4_invert(mat4_t M)
{
	mat4_t T;
	n_t s[6];
	n_t c[6];
	s[0] = M._[0]._[0]*M._[1]._[1] - M._[1]._[0]*M._[0]._[1];
	s[1] = M._[0]._[0]*M._[1]._[2] - M._[1]._[0]*M._[0]._[2];
	s[2] = M._[0]._[0]*M._[1]._[3] - M._[1]._[0]*M._[0]._[3];
	s[3] = M._[0]._[1]*M._[1]._[2] - M._[1]._[1]*M._[0]._[2];
	s[4] = M._[0]._[1]*M._[1]._[3] - M._[1]._[1]*M._[0]._[3];
	s[5] = M._[0]._[2]*M._[1]._[3] - M._[1]._[2]*M._[0]._[3];

	c[0] = M._[2]._[0]*M._[3]._[1] - M._[3]._[0]*M._[2]._[1];
	c[1] = M._[2]._[0]*M._[3]._[2] - M._[3]._[0]*M._[2]._[2];
	c[2] = M._[2]._[0]*M._[3]._[3] - M._[3]._[0]*M._[2]._[3];
	c[3] = M._[2]._[1]*M._[3]._[2] - M._[3]._[1]*M._[2]._[2];
	c[4] = M._[2]._[1]*M._[3]._[3] - M._[3]._[1]*M._[2]._[3];
	c[5] = M._[2]._[2]*M._[3]._[3] - M._[3]._[2]*M._[2]._[3];

	/* Assumes it is invertible */
	n_t idet = 1.0f/( s[0]*c[5]-s[1]*c[4]+s[2]*c[3]+s[3]*c[2]-s[4]*c[1]+s[5]*c[0] );

	T._[0]._[0] = ( M._[1]._[1] * c[5] - M._[1]._[2] * c[4] + M._[1]._[3] * c[3]) * idet;
	T._[0]._[1] = (-M._[0]._[1] * c[5] + M._[0]._[2] * c[4] - M._[0]._[3] * c[3]) * idet;
	T._[0]._[2] = ( M._[3]._[1] * s[5] - M._[3]._[2] * s[4] + M._[3]._[3] * s[3]) * idet;
	T._[0]._[3] = (-M._[2]._[1] * s[5] + M._[2]._[2] * s[4] - M._[2]._[3] * s[3]) * idet;

	T._[1]._[0] = (-M._[1]._[0] * c[5] + M._[1]._[2] * c[2] - M._[1]._[3] * c[1]) * idet;
	T._[1]._[1] = ( M._[0]._[0] * c[5] - M._[0]._[2] * c[2] + M._[0]._[3] * c[1]) * idet;
	T._[1]._[2] = (-M._[3]._[0] * s[5] + M._[3]._[2] * s[2] - M._[3]._[3] * s[1]) * idet;
	T._[1]._[3] = ( M._[2]._[0] * s[5] - M._[2]._[2] * s[2] + M._[2]._[3] * s[1]) * idet;

	T._[2]._[0] = ( M._[1]._[0] * c[4] - M._[1]._[1] * c[2] + M._[1]._[3] * c[0]) * idet;
	T._[2]._[1] = (-M._[0]._[0] * c[4] + M._[0]._[1] * c[2] - M._[0]._[3] * c[0]) * idet;
	T._[2]._[2] = ( M._[3]._[0] * s[4] - M._[3]._[1] * s[2] + M._[3]._[3] * s[0]) * idet;
	T._[2]._[3] = (-M._[2]._[0] * s[4] + M._[2]._[1] * s[2] - M._[2]._[3] * s[0]) * idet;

	T._[3]._[0] = (-M._[1]._[0] * c[3] + M._[1]._[1] * c[1] - M._[1]._[2] * c[0]) * idet;
	T._[3]._[1] = ( M._[0]._[0] * c[3] - M._[0]._[1] * c[1] + M._[0]._[2] * c[0]) * idet;
	T._[3]._[2] = (-M._[3]._[0] * s[3] + M._[3]._[1] * s[1] - M._[3]._[2] * s[0]) * idet;
	T._[3]._[3] = ( M._[2]._[0] * s[3] - M._[2]._[1] * s[1] + M._[2]._[2] * s[0]) * idet;
	return T;
}
static inline mat4_t mat4_orthonormalize(mat4_t M)
{
	mat4_t R = M;
	n_t s = 1.0f;

	R._[2].xyz = vec3_norm(R._[2].xyz);
	
	s = vec3_dot(R._[1].xyz, R._[2].xyz);
	R._[1].xyz = vec3_sub(R._[1].xyz, vec3_scale(R._[2].xyz, s));
	R._[2].xyz = vec3_norm(R._[2].xyz);

	s = vec3_dot(R._[1].xyz, R._[2].xyz);
	R._[1].xyz = vec3_sub(R._[1].xyz, vec3_scale(R._[2].xyz, s));
	R._[1].xyz = vec3_norm(R._[1].xyz);

	s = vec3_dot(R._[0].xyz, R._[1].xyz);
	R._[0].xyz = vec3_sub(R._[0].xyz, vec3_scale(R._[1].xyz, s));
	R._[0].xyz = vec3_norm(R._[0].xyz);
	return R;
}

static inline mat4_t mat4_frustum(n_t l, n_t r, n_t b, n_t t, n_t n, n_t f)
{
	mat4_t M;
	M._[0]._[0] = 2.f*n/(r-l);
	M._[0]._[1] = M._[0]._[2] = M._[0]._[3] = 0.f;
	
	M._[1]._[1] = 2.*n/(t-b);
	M._[1]._[0] =
		M._[1]._[2] =
		M._[1]._[3] = 0.0f;

	M._[2]._[0] = (r+l)/(r-l);
	M._[2]._[1] = (t+b)/(t-b);
	M._[2]._[2] = -(f+n)/(f-n);
	M._[2]._[3] = -1.f;
	
	M._[3]._[2] = -2.f*(f*n)/(f-n);
	M._[3]._[0] =
		M._[3]._[1] =
		M._[3]._[3] = 0.0f;
	return M;
}
static inline mat4_t mat4_ortho(n_t l, n_t r, n_t b, n_t t, n_t n, n_t f)
{
	mat4_t M;
	M._[0]._[0] = 2.f/(r-l);
	M._[0]._[1] =
		M._[0]._[2] =
		M._[0]._[3] = 0.0f;

	M._[1]._[1] = 2.f/(t-b);
	M._[1]._[0] =
		M._[1]._[2] =
		M._[1]._[3] = 0.0f;

	M._[2]._[2] = -2.f/(f-n);
	M._[2]._[0] =
		M._[2]._[1] =
		M._[2]._[3] = 0.0f;
	
	M._[3]._[0] = -(r+l)/(r-l);
	M._[3]._[1] = -(t+b)/(t-b);
	M._[3]._[2] = -(f+n)/(f-n);
	M._[3]._[3] = 1.f;
	return M;
}
static inline mat4_t mat4_perspective(n_t y_fov, n_t aspect, n_t n, n_t f)
{
	/* NOTE: Degrees are an unhandy unit to work with.
	 * mafs.h uses radians for everything! */

	mat4_t M;

	n_t const a = 1.f / tan(y_fov / 2.f);

	M._[0]._[0] = a / aspect;
	M._[0]._[1] = 0.f;
	M._[0]._[2] = 0.f;
	M._[0]._[3] = 0.f;

	M._[1]._[0] = 0.f;
	M._[1]._[1] = a;
	M._[1]._[2] = 0.f;
	M._[1]._[3] = 0.f;

	M._[2]._[0] = 0.f;
	M._[2]._[1] = 0.f;
	M._[2]._[2] = -((f + n) / (f - n));
	M._[2]._[3] = -1.f;

	M._[3]._[0] = 0.f;
	M._[3]._[1] = 0.f;
	M._[3]._[2] = -((2.f * f * n) / (f - n));
	M._[3]._[3] = 0.f;
	return M;
}

static inline mat4_t mat4_from_vecs(vec3_t X, vec3_t Y, vec3_t Z)
{
	mat4_t result;
	X = vec3_norm(X);
	Y = vec3_norm(Y);
	Z = vec3_norm(Z);

	result._[0]._[0] = X._[0];
	result._[0]._[1] = X._[1];
	result._[0]._[2] = X._[2];
	result._[0]._[3] = 0;

	result._[1]._[0] = Y._[0];
	result._[1]._[1] = Y._[1];
	result._[1]._[2] = Y._[2];
	result._[1]._[3] = 0;


	result._[2]._[0] = Z._[0];
	result._[2]._[1] = Z._[1];
	result._[2]._[2] = Z._[2];
	result._[2]._[3] = 0;


	result._[3]._[0] = 0;
	result._[3]._[1] = 0;
	result._[3]._[2] = 0;
	result._[3]._[3] = 1;
	return result;
}

static inline mat4_t mat4_look_at(vec3_t eye, vec3_t center, vec3_t up)
{
	/* Adapted from Android's OpenGL Matrix.java.                        */
	/* See the OpenGL GLUT documentation for gluLookAt for a description */
	/* of the algorithm. We implement it in a straightforward way:       */

	/* TODO: The negation of of can be spared by swapping the order of
	 *       operands in the following cross products in the right way. */

	mat4_t M;
	vec3_t f = vec3_norm(vec3_sub(center, eye));	
	
	vec3_t s = vec3_norm(vec3_cross(f, up));

	vec3_t t = vec3_cross(s, f);

	M._[0]._[0] =  s.x;
	M._[0]._[1] =  t.x;
	M._[0]._[2] = -f.x;
	M._[0]._[3] =   0.f;

	M._[1]._[0] =  s.y;
	M._[1]._[1] =  t.y;
	M._[1]._[2] = -f.y;
	M._[1]._[3] =   0.f;

	M._[2]._[0] =  s.z;
	M._[2]._[1] =  t.z;
	M._[2]._[2] = -f.z;
	M._[2]._[3] =   0.f;

	M._[3]._[0] =  0.f;
	M._[3]._[1] =  0.f;
	M._[3]._[2] =  0.f;
	M._[3]._[3] =  1.f;

	return mat4_translate_in_place(M, vec3_inv(eye));
}

static inline vec4_t quat(void)
{
	return vec4(0.0f, 0.0f, 0.0f, 1.0f);
}
static inline vec4_t quat_add(vec4_t a, vec4_t b)
{
	vec4_t r;
	int i;
	for(i=0; i<4; ++i)
		r._[i] = a._[i] + b._[i];
	return r;
}
static inline vec4_t quat_sub(vec4_t a, vec4_t b)
{
	vec4_t r;
	int i;
	for(i=0; i<4; ++i)
		r._[i] = a._[i] - b._[i];
	return r;
}
static inline vec4_t quat_scale(vec4_t v, n_t s)
{
	vec4_t r;
	int i;
	for(i=0; i<4; ++i)
		r._[i] = v._[i] * s;
	return r;
}
static inline n_t quat_inner_product(vec4_t a, vec4_t b)
{
	n_t p = 0.f;
	int i;
	for(i=0; i<4; ++i)
		p += b._[i]*a._[i];
	return p;
}
static inline vec4_t quat_conj(vec4_t q)
{
	vec4_t r;
	int i;
	for(i=0; i<3; ++i)
		r._[i] = -q._[i];
	r._[3] = q._[3];
	return r;
}
static inline vec3_t quat_mul_vec3(vec4_t q, vec3_t v)
{
/*
 * Method by Fabian 'ryg' Giessen (of Farbrausch)
t = 2 * cross(q.xyz, v)
v' = v + q.w * t + cross(q.xyz, t)
 */
	vec3_t t = vec3_scale(vec3_cross(q.xyz, v), 2);

	return vec3_add(vec3_add(v, vec3_scale(t, q.w)), vec3_cross(q.xyz, t));
}

static inline mat4_t quat_to_mat4(vec4_t q)
{
	mat4_t M;
	n_t qxx = q.x * q.x;
	n_t qyy = q.y * q.y;
	n_t qzz = q.z * q.z;
	n_t qxz = q.x * q.z;
	n_t qxy = q.x * q.y;
	n_t qyz = q.y * q.z;
	n_t qwx = q.w * q.x;
	n_t qwy = q.w * q.y;
	n_t qwz = q.w * q.z;

	M._[0]._[0] = 1.0f - 2.0f * (qyy +  qzz);
	M._[0]._[1] = 2.0f * (qxy + qwz);
	M._[0]._[2] = 2.0f * (qxz - qwy);
	M._[0]._[3] = 0;

	M._[1]._[0] = 2.0f * (qxy - qwz);
	M._[1]._[1] = 1.0f - 2.0f * (qxx +  qzz);
	M._[1]._[2] = 2.0f * (qyz + qwx);
	M._[1]._[3] = 0;

	M._[2]._[0] = 2.0f * (qxz + qwy);
	M._[2]._[1] = 2.0f * (qyz - qwx);
	M._[2]._[2] = 1.0f - 2.0f * (qxx +  qyy);
	M._[2]._[3] = 0;

	M._[3]._[0] = M._[3]._[1] = M._[3]._[2] = 0.f;
	M._[3]._[3] = 1.f;
	return M;
}
/* static inline mat4_t mat4_from_quat(vec4_t q) */
/* { */
/* 	mat4_t M; */
/* 	n_t a = q.w; */
/* 	n_t b = q.x; */
/* 	n_t c = q.y; */
/* 	n_t d = q.z; */
/* 	n_t a2 = a*a; */
/* 	n_t b2 = b*b; */
/* 	n_t c2 = c*c; */
/* 	n_t d2 = d*d; */
	
/* 	M._[0]._[0] = a2 + b2 - c2 - d2; */
/* 	M._[0]._[1] = 2.f*(b*c + a*d); */
/* 	M._[0]._[2] = 2.f*(b*d - a*c); */
/* 	M._[0]._[3] = 0.f; */

/* 	M._[1]._[0] = 2*(b*c - a*d); */
/* 	M._[1]._[1] = a2 - b2 + c2 - d2; */
/* 	M._[1]._[2] = 2.f*(c*d + a*b); */
/* 	M._[1]._[3] = 0.f; */

/* 	M._[2]._[0] = 2.f*(b*d + a*c); */
/* 	M._[2]._[1] = 2.f*(c*d - a*b); */
/* 	M._[2]._[2] = a2 - b2 - c2 + d2; */
/* 	M._[2]._[3] = 0.f; */

/* 	M._[3]._[0] = M._[3]._[1] = M._[3]._[2] = 0.f; */
/* 	M._[3]._[3] = 1.f; */
/* 	return M; */
/* } */

static inline vec3_t quat_to_euler(vec4_t q)
{
	vec3_t result;

	// roll (x-axis rotation)
	n_t sinr = +2.0 * (q.w * q.x + q.y * q.z);
	n_t cosr = +1.0 - 2.0 * (q.x * q.x + q.y * q.y);
	result.x = atan2f(sinr, cosr);

	// pitch (y-axis rotation)
	n_t sinp = +2.0 * (q.w * q.y - q.z * q.x);
	if (fabs(sinp) >= 1)
		result.y = copysignf(M_PI / 2.0f, sinp); // use 90 degrees if out of range
	else
		result.y = asin(sinp);

	// yaw (z-axis rotation)
	n_t siny = +2.0 * (q.w * q.z + q.x * q.y);
	n_t cosy = +1.0 - 2.0 * (q.y * q.y + q.z * q.z);  
	result.z = atan2f(siny, cosy);

	return result;
}

/* static inline vec4_t quat_mul(vec4_t p, vec4_t q) */
/* { */
/* 	vec4_t r; */
/* 	vec3_t w; */
/* 	r.xyz = vec3_cross(p.xyz, q.xyz); */
/* 	w = vec3_scale(p.xyz, q._[3]); */
/* 	r.xyz = vec3_add(r.xyz, w); */
/* 	w = vec3_scale(q.xyz, p._[3]); */
/* 	r.xyz = vec3_add(r.xyz, w); */
/* 	r.w = p.w*q.w - vec3_dot(p.xyz, q.xyz); */
/* 	return r; */
/* } */
static inline vec4_t quat_mul(vec4_t p, vec4_t q)
{
	return vec4(p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y,
			p.w * q.y + p.y * q.w + p.z * q.x - p.x * q.z,
			p.w * q.z + p.z * q.w + p.x * q.y - p.y * q.x,
			p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z);
}
static inline vec4_t quat_rotate(vec3_t axis, n_t angle)
{
	vec4_t r;
	n_t s, c;
	sincosf(angle * 0.5f, &s, &c);

	r.xyz = vec3_scale(axis, s);
	r.w = c;
	return r;
}

static inline vec4_t quat_from_euler(n_t yaw, n_t pitch, n_t roll)
{
	n_t yaw_sin, yaw_cos, pitch_sin, pitch_cos, roll_sin, roll_cos;
	yaw *= 0.5f;
	pitch *= 0.5f;
	roll *= 0.5f;
	sincosf(yaw, &yaw_sin, &yaw_cos);
	sincosf(pitch, &pitch_sin, &pitch_cos);
	sincosf(roll, &roll_sin, &roll_cos);
	return vec4(
			roll_cos * pitch_sin * yaw_cos + roll_sin * pitch_cos * yaw_sin,
			roll_cos * pitch_cos * yaw_sin - roll_sin * pitch_sin * yaw_cos,
			roll_sin * pitch_cos * yaw_cos - roll_cos * pitch_sin * yaw_sin,
			roll_cos * pitch_cos * yaw_cos + roll_sin * pitch_sin * yaw_sin);
}

static inline mat4_t mat4_mul_quat(mat4_t M, vec4_t q)
{
/*  XXX: The way this is written only works for othogonal matrices. */
/* TODO: Take care of non-orthogonal case. */
	mat4_t R;
	R._[0].xyz = quat_mul_vec3(q, M._[0].xyz);
	R._[1].xyz = quat_mul_vec3(q, M._[1].xyz);
	R._[2].xyz = quat_mul_vec3(q, M._[2].xyz);

	R._[3]._[0] = R._[3]._[1] = R._[3]._[2] = 0.f;
	R._[3]._[3] = 1.f;
	return R;
}

/* static inline vec4_t quat_from_mat4(mat4_t M) */
/* { */
/* 	vec4_t q; */
/* 	n_t r=0.f; */
/* 	int i; */

/* 	int perm[] = { 0, 1, 2, 0, 1 }; */
/* 	int *p = perm; */

/* 	for(i = 0; i<3; i++) { */
/* 		n_t m = M._[i]._[i]; */
/* 		if( m < r ) continue; */
/* 		m = r; */
/* 		p = &perm[i]; */
/* 	} */

/* 	r = sqrtf(1.f + M._[p[0]]._[p[0]] - M._[p[1]]._[p[1]] - M._[p[2]]._[p[2]]); */

/* 	if(r < 1e-6) { */
/* 		q._[0] = 1.f; */
/* 		q._[1] = q._[2] = q._[3] = 0.f; */
/* 		return q; */
/* 	} */

/* 	q.x = r/2.f; */
/* 	q.y = (M._[p[0]]._[p[1]] - M._[p[1]]._[p[0]])/(2.f*r); */
/* 	q.z = (M._[p[2]]._[p[0]] - M._[p[0]]._[p[2]])/(2.f*r); */
/* 	q.w = (M._[p[2]]._[p[1]] - M._[p[1]]._[p[2]])/(2.f*r); */
/* 	return q; */
/* } */
static inline vec4_t mat4_to_quat(mat4_t m)
{
	n_t fourXSquaredMinus1 = m._[0]._[0] - m._[1]._[1] - m._[2]._[2];
	n_t fourYSquaredMinus1 = m._[1]._[1] - m._[0]._[0] - m._[2]._[2];
	n_t fourZSquaredMinus1 = m._[2]._[2] - m._[0]._[0] - m._[1]._[1];
	n_t fourWSquaredMinus1 = m._[0]._[0] + m._[1]._[1] + m._[2]._[2];

	int biggestIndex = 0;
	n_t fourBiggestSquaredMinus1 = fourWSquaredMinus1;
	if(fourXSquaredMinus1 > fourBiggestSquaredMinus1)
	{
		fourBiggestSquaredMinus1 = fourXSquaredMinus1;
		biggestIndex = 1;
	}
	if(fourYSquaredMinus1 > fourBiggestSquaredMinus1)
	{
		fourBiggestSquaredMinus1 = fourYSquaredMinus1;
		biggestIndex = 2;
	}
	if(fourZSquaredMinus1 > fourBiggestSquaredMinus1)
	{
		fourBiggestSquaredMinus1 = fourZSquaredMinus1;
		biggestIndex = 3;
	}

	n_t biggestVal = sqrtf(fourBiggestSquaredMinus1 + 1.0f) * 0.5f;
	n_t mult = 0.25f / biggestVal;

	switch(biggestIndex)
	{
		case 0:
			return vec4((m._[1]._[2] - m._[2]._[1]) * mult, (m._[2]._[0] - m._[0]._[2]) * mult, (m._[0]._[1] - m._[1]._[0]) * mult, biggestVal);
		case 1:
			return vec4(biggestVal, (m._[0]._[1] + m._[1]._[0]) * mult, (m._[2]._[0] + m._[0]._[2]) * mult, (m._[1]._[2] - m._[2]._[1]) * mult);
		case 2:
			return vec4((m._[0]._[1] + m._[1]._[0]) * mult, biggestVal, (m._[1]._[2] + m._[2]._[1]) * mult, (m._[2]._[0] - m._[0]._[2]) * mult);
		case 3:
			return vec4((m._[2]._[0] + m._[0]._[2]) * mult, (m._[1]._[2] + m._[2]._[1]) * mult, biggestVal, (m._[0]._[1] - m._[1]._[0]) * mult);
		default:
			return vec4(0, 0, 0, 0);
	}
}

typedef struct mat3_t { union {
	struct { vec3_t a, b, c, d; };
	vec3_t _[3];
}; } mat3_t;


static inline mat3_t mat3(void)
{
	mat3_t M;
	int i, j;
	for(i=0; i<3; ++i)
		for(j=0; j<3; ++j)
			M._[i]._[j] = i==j ? 1.f : 0.f;
	return M;
}

static inline mat3_t mat3_of_mat4(mat4_t N)
{
	mat3_t M;
	int i, j;
	for(i=0; i<3; ++i)
		for(j=0; j<3; ++j)
			M._[i]._[j] = N._[i]._[j];
	return M;
}

static inline void float_clamp(float *n, float a, float b)
{
	if(*n < a) *n = a;
	if(*n > b) *n = b;
}

static inline float float_mix(float x, float y, float a)
{
	return x * (1.0f - a) + y * a; \
}

static inline vec2_t int_to_vec2(int id)
{
	id++;
	union {
		unsigned int i;
		struct{
			unsigned char r, g, b, a;
		};
	} convert = {.i = id};

	return vec2((float)convert.r / 255, (float)convert.g / 255);
}

#define Z3 (vec3(0.0f))
#define Z2 (vec2(0.0f))
/* CONST vec3_t Z3 = { .x = 0, .y = 0, .z = 0}; */
/* CONST vec2_t Z2 = { .x = 0, .y = 0}; */

#ifndef __OPENCL_C_VERSION__
#define MAFS_ARGNUM(...) \
    _MAFS_ARGNUM(_0, ##__VA_ARGS__, 4, 3, 2, 1, 0)

#define _MAFS_ARGNUM(_0, _1, _2, _3, _4, N, ...) N

#define CAT(a,b) a##b
#define CAT2(a,b) CAT(a,b)

#define _type(a, b) __builtin_types_compatible_p(typeof(a), b)
#define _if(c, a, b) __builtin_choose_expr(c, a, b)
/* #define CONSTIFY(x) (__builtin_constant_p(x)&&(x)) */

#define _vec2(v) (float)v.x, (float)v.y
#define _vec3(v) (float)v.x, (float)v.y, (float)v.z
#define _vec4(v) (float)v.x, (float)v.y, (float)v.z, (float)v.w
#define _d2(v) (double)v.x, (double)v.y
#define _d3(v) (double)v.x, (double)v.y, (double)v.z
#define _d4(v) (double)v.x, (double)v.y, (double)v.z, (double)v.w

#define __vec2_1(a)  \
			_if(_type(a, vec2_t), \
				a, \
				vec2_n(a) \
			)
#define __vec2_2(a, b)  \
			vec2_n_n(a, b)


#define __vec3_1(a)  \
			_if(_type(a, vec3_t), \
				a, \
				vec3_n(a) \
			)
#define __vec3_2(a, b)  \
			_if(_type(a, vec2_t), \
				vec3v2f(a, b), \
				vec3fv2(a, b) \
			)
#define __vec3_3(a, b, c)  \
			vec3_n_n_n(a, b, c)

#define __vec4_1(a)  \
			_if(_type(a, vec4_t), \
				a, \
				vec4_n(a) \
			)
#define __vec4_2(a, b)  \
			_if(_type(a, vec3_t), \
				vec4v3f(a, b), \
				vec4fv3(a, b) \
			)
#define __vec4_4(a, b, c, d)  \
			vec4_n_n_n_n(a, b, c, d)

#define vec2(...) CAT2(__vec2_, MAFS_ARGNUM(__VA_ARGS__))(__VA_ARGS__)
#define vec3(...) CAT2(__vec3_, MAFS_ARGNUM(__VA_ARGS__))(__VA_ARGS__)
#define vec4(...) CAT2(__vec4_, MAFS_ARGNUM(__VA_ARGS__))(__VA_ARGS__)

#define xy(vec) vec.xy
#define xz(vec) vec2(vec.x, vec.z)

#endif

#endif
