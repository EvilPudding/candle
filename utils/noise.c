#include "noise.h"
#include <math.h>

/*  Classic Perlin 3D Noise  */
/*  by Stefan Gustavson */
/* */
static vec4_t permute(vec4_t vec)
{
	vec4_t ret;
	vec_oper(ret, fmod(((((float*)&vec)[x] * 34.0) + 1.0) * ((float*)&vec)[x], 289.0));
	return ret;
}

static vec4_t taylorInvSqrt(vec4_t vec)
{
	vec4_t ret;
	vec_oper(ret, 1.79284291400159 - 0.85373472095314 * ((float*)&vec)[x]);
	return ret;
}

static vec3_t fade(vec3_t vec)
{
	vec3_t ret;
	vec_oper(ret, ((float*)&vec)[x] * ((float*)&vec)[x] * ((float*)&vec)[x]
			* (((float*)&vec)[x] * (((float*)&vec)[x] * 6.0-15.0)+10.0));
	return ret;
}

float cnoise(vec3_t P)
{
	vec3_t Pf0, Pf1;
	vec4_t ix, iy, iz0, iz1;
	vec4_t ixy, ixy0, ixy1;
	vec4_t gx0, gy0;
	vec4_t gz0, sz0;
	vec4_t gx1, gy1;
	vec4_t gz1, sz1;
	vec3_t g000, g100, g010, g110, g001, g101, g011, g111;
	vec4_t norm0, norm1;
	float n000, n100, n010, n110, n001, n101, n011, n111;
	vec3_t fade_xyz;
	vec4_t n_z;
	vec2_t n_yz;
	float n_xyz;

	vec3_t Pi0 = vec3_floor(P); /* Integer part for indexing */
	vec3_t Pi1 = vec3_add_number(Pi0, 1.0); /* Integer part + 1 */

	vec_oper(Pi0, fmod(((float*)&Pi0)[x], 289.0));
	vec_oper(Pi1, fmod(((float*)&Pi1)[x], 289.0));

	Pf0 = vec3_fract(P); /* Fractional part for interpolation */
	Pf1 = vec3_sub_number(Pf0, 1.0); /* Fractional part - 1.0 */

	ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
	iy = vec4(Pi0.y, Pi0.y, Pi1.y, Pi1.y);
	iz0 = vec4(Pi0.z, Pi0.z, Pi0.z, Pi0.z);
	iz1 = vec4(Pi1.z, Pi1.z, Pi1.z, Pi1.z);

	ixy = permute(vec4_add(permute(ix), iy));
	ixy0 = permute(vec4_add(ixy, iz0));
	ixy1 = permute(vec4_add(ixy, iz1));

	gx0 = vec4_div_number(ixy0, 7.0);
	gy0 = vec4_sub_number(vec4_fract(vec4_div_number(vec4_floor(gx0), 7.0)), 0.5);
	gx0 = vec4_fract(gx0);
	gz0 = vec4_sub(vec4_sub(vec4(0.5, 0.5, 0.5, 0.5), vec4_abs(gx0)), vec4_abs(gy0));
	sz0 = vec4_step(gz0, vec4(0.0, 0.0, 0.0, 0.0));
	gx0 = vec4_sub(gx0, vec4_mul(sz0, vec4_sub_number(vec4_step_number(0.0, gx0), 0.5)));
	gy0 = vec4_sub(gy0, vec4_mul(sz0, vec4_sub_number(vec4_step_number(0.0, gy0), 0.5)));

	gx1 = vec4_div_number(ixy1, 7.0);
	gy1 = vec4_sub_number(vec4_fract(vec4_div_number(vec4_floor(gx1), 7.0)), 0.5);
	gx1 = vec4_fract(gx1);
	gz1 = vec4_sub(vec4_sub(vec4(0.5, 0.5, 0.5, 0.5), vec4_abs(gx1)), vec4_abs(gy1));
	sz1 = vec4_step(gz1, vec4(0.0, 0.0, 0.0, 0.0));
	gx1 = vec4_sub(gx1, vec4_mul(sz1, vec4_sub_number(vec4_step_number(0.0, gx1), 0.5)));
	gy1 = vec4_sub(gy1, vec4_mul(sz1, vec4_sub_number(vec4_step_number(0.0, gy1), 0.5)));

	g000 = vec3(gx0.x,gy0.x,gz0.x);
	g100 = vec3(gx0.y,gy0.y,gz0.y);
	g010 = vec3(gx0.z,gy0.z,gz0.z);
	g110 = vec3(gx0.w,gy0.w,gz0.w);
	g001 = vec3(gx1.x,gy1.x,gz1.x);
	g101 = vec3(gx1.y,gy1.y,gz1.y);
	g011 = vec3(gx1.z,gy1.z,gz1.z);
	g111 = vec3(gx1.w,gy1.w,gz1.w);

	norm0 = taylorInvSqrt(vec4(
				vec3_dot(g000, g000),
				vec3_dot(g010, g010),
				vec3_dot(g100, g100),
				vec3_dot(g110, g110)));
	g000 = vec3_scale(g000, norm0.x);
	g010 = vec3_scale(g010, norm0.y);
	g100 = vec3_scale(g100, norm0.z);
	g110 = vec3_scale(g110, norm0.w);
	norm1 = taylorInvSqrt(vec4(vec3_dot(g001, g001),
				vec3_dot(g011, g011),
				vec3_dot(g101, g101),
				vec3_dot(g111, g111)));

	g001 = vec3_scale(g001, norm1.x);
	g011 = vec3_scale(g011, norm1.y);
	g101 = vec3_scale(g101, norm1.z);
	g111 = vec3_scale(g111, norm1.w);

	n000 = vec3_dot(g000, Pf0);
	n100 = vec3_dot(g100, vec3(Pf1.x, Pf0.y, Pf0.z));
	n010 = vec3_dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
	n110 = vec3_dot(g110, vec3(Pf1.x, Pf1.y, Pf0.z));
	n001 = vec3_dot(g001, vec3(Pf0.x, Pf0.y, Pf1.z));
	n101 = vec3_dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
	n011 = vec3_dot(g011, vec3(Pf0.x, Pf1.y, Pf1.z));
	n111 = vec3_dot(g111, Pf1);


	fade_xyz = fade(Pf0);
	n_z = vec4_mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
	n_yz = vec2_mix(vec2(n_z.x, n_z.y), vec2(n_z.z, n_z.w), fade_xyz.y);
	n_xyz = float_mix(n_yz.x, n_yz.y, fade_xyz.x); 
	return 2.2 * n_xyz;
}

