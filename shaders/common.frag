#ifndef FRAG_COMMON
#define FRAG_COMMON
#line 3

struct property_t
{
	float texture_blend;
	sampler2D texture;
	float texture_scale;
	vec4 color;
};

struct pass_t
{
	float brightness;
	sampler2D texture;
};

uniform vec2 output_size;

uniform vec3 ambient_light;

uniform property_t diffuse;
uniform property_t specular;
uniform property_t reflection;
uniform property_t normal;
uniform property_t transparency;
uniform mat4 V;
uniform mat4 MVP;

struct gbuffer_t
{
	sampler2D diffuse;
	sampler2D specular;
	sampler2D reflection;
	sampler2D normal;
	sampler2D transparency;
	sampler2D wposition;
	sampler2D cposition;
	sampler2D id;
};

uniform gbuffer_t gbuffer;

uniform float distortion;
uniform float scattering;

uniform samplerCube shadow_map;

uniform samplerCube ambient_map;
/* uniform samplerCube shadow_map_color; */

uniform sampler3D perlin_map;
//uniform samplerCubeShadow shadow_map;

uniform vec3 camera_pos;
uniform float exposure;
uniform vec3 light_pos;
uniform float light_intensity;
uniform float has_tex;

in vec3 tgspace_light_dir;
in vec3 tgspace_eye_dir;
in vec3 worldspace_position;
in vec3 cameraspace_vertex_pos;
in vec3 cameraspace_light_dir;

in vec3 cam_normal;
in vec3 cam_tangent;
in vec3 cam_bitangent;

in vec3 selection_id;

in vec3 vertex_normal;
in vec3 vertex_tangent;
in vec3 vertex_bitangent;

in vec2 texcoord;
in vec4 vertex_color;

in mat3 TM;
uniform mat4 projection;


vec4 pass_sample(pass_t pass, vec2 coord)
{
	return textureLod(pass.texture, coord, 0);
}

vec4 resolveProperty(property_t prop, vec2 coords)
{
	/* vec3 tex; */
	if(prop.texture_blend > 0.001)
	{
		return mix(
			prop.color,
			texture2D(prop.texture, prop.texture_scale * coords),
			/* textureLod(prop.texture, prop.texture_scale * coords, 3), */
			prop.texture_blend
		);
	}
	return prop.color;
}

float ambient = 0.2;
/* float ambient = 1.00; */

float lookup_single(vec3 shadowCoord)
{
	return texture(shadow_map, shadowCoord).a;
}

/* float prec = 0.05; */
float lookup(vec3 shadowCoord, vec3 offset)
{
	vec3 coord = shadowCoord + offset;
	/* float dist = length(shadowCoord); */
	float dist = length(coord);
	float dist2 = lookup_single(coord) - dist;
	return (dist2 > -0.05) ? 1.0 : 0.0;
}

vec3 get_normal()
{
	if(has_tex > 0.5)
	{
		vec3 normalColor = resolveProperty(normal, texcoord).rgb * 2.0 - 1.0;
		normalColor = vec3(normalColor.y, -normalColor.x, normalColor.z);
		return normalize(TM * normalColor);
	}
	return normalize(cam_normal);
}

vec3 get_light_dir()
{
	if(has_tex > 0.5)
	{
		return normalize(tgspace_light_dir);
	}
	return normalize(cameraspace_light_dir);
}

vec3 get_half_dir()
{
	if(has_tex > 0.5)
	{
		return normalize(tgspace_light_dir + tgspace_eye_dir);
	}
	return normalize(cameraspace_light_dir - cameraspace_vertex_pos);
}

vec3 get_eye_dir()
{
	if(has_tex > 0.5)
	{
		return normalize(tgspace_eye_dir);
	}
	return normalize(-cameraspace_vertex_pos);
}

float shadow_at_dist_no_tan(vec3 vec, float i)
{
	vec3 x = vec3(1.0, 0.0, 0.0) * i;
	vec3 y = vec3(0.0, 1.0, 0.0) * i;
	vec3 z = vec3(0.0, 0.0, 1.0) * i;
	
	if(lookup(-vec,  x) > 0.5) return 1.0;
	if(lookup(-vec,  y) > 0.5) return 1.0;
	if(lookup(-vec,  z) > 0.5) return 1.0;
	if(lookup(-vec, -x) > 0.5) return 1.0;
	if(lookup(-vec, -y) > 0.5) return 1.0;
	if(lookup(-vec, -z) > 0.5) return 1.0;


	return 0.0;
}

/* float shadow_at_dist(vec3 vec, float i) */
/* { */
/* 	vec3 x = vertex_tangent * i; */
/* 	vec3 y = vertex_bitangent * i; */
	
/* 	if(lookup(-vec,  x + 0) > 0.5) return 1.0; */
/* 	if(lookup(-vec,  0 + y) > 0.5) return 1.0; */
/* 	if(lookup(-vec, -x + 0) > 0.5) return 1.0; */
/* 	if(lookup(-vec,  0 - y) > 0.5) return 1.0; */

/* 	if(lookup(-vec,  x - y) > 0.5) return 1.0; */
/* 	if(lookup(-vec,  x + y) > 0.5) return 1.0; */
/* 	if(lookup(-vec, -x - y) > 0.5) return 1.0; */
/* 	if(lookup(-vec, -x + y) > 0.5) return 1.0; */

/* 	return 0.0; */
/* } */

float get_shadow(vec3 vec, float point_to_light, float dist_to_eye)
{
	float ocluder_to_light = lookup_single(-vec);

	float sd = ((ocluder_to_light - point_to_light) > -0.05) ? 0.0 : 1.0;

	float breaked = 0.0;
	if(sd > 0.5)
	{
		/* sd = 0.5; */
		float shadow_len = min(0.8 * max(0.05, point_to_light / ocluder_to_light - 1), 10);
		if(shadow_len > 0.0001)
		{
			float i;

			if(shadow_at_dist_no_tan(vec, shadow_len) < 0.5) return 1.0f;

			float count = 1;
			float iters = floor(clamp(shadow_len / (dist_to_eye *
							dist_to_eye / 385), 2, 16));
			float inc = shadow_len / iters;

			for (i = inc; i <= shadow_len; i += inc)
			{
				if(shadow_at_dist_no_tan(vec, i) > 0.5) break;

				count++;
			}
			return count / (iters + 1);
		}
	}
	sd = 0.0;
	return sd;
}

/* float get_shadow_bak(vec3 vec, float point_to_light) */
/* { */
/* 	float ocluder_to_light = lookup_single(-vec); */

/* 	float sd = ((ocluder_to_light - point_to_light) > -0.05) ? 0.0 : 1.0; */

/* 	float breaked = 0.0; */
/* 	if(sd > 0.5) */
/* 	{ */
/* 		/1* sd = 0.5; *1/ */
/* 		float shadow_len = min(0.8 * max(0.05, point_to_light / ocluder_to_light - 1), 10); */
/* 		if(shadow_len > 0.0001) */
/* 		{ */
/* 			float i; */

/* 			float dist_to_eye = length(cameraspace_vertex_pos); */

/* 			float count = 1; */
/* 			float iters = floor(clamp(shadow_len / (dist_to_eye * */
/* 							dist_to_eye / 385), 2, 8)); */
/* 			float inc = shadow_len / iters; */

/* 			float frac = 0.0; */
/* 			for (i = shadow_len ; i >= inc; i -= inc) */
/* 			{ */
/* 				float pl = 0; */
/* 				vec3 x = vertex_tangent * i; */
/* 				vec3 y = vertex_bitangent * i; */
/* 				pl += lookup(-vec, x + 0); */
/* 				pl += lookup(-vec, 0 + y); */
/* 				pl += lookup(-vec, (x + y)); */
/* 				pl += lookup(-vec, (x - y)); */

/* 				pl += lookup(-vec, -x + 0); */
/* 				pl += lookup(-vec, 0 - y); */
/* 				pl += lookup(-vec, (-x - y)); */
/* 				pl += lookup(-vec, (-x + y)); */
/* 				count += 4; */

/* 				if(pl < 1) */
/* 				{ */
/* 					breaked += inc * 2; */
/* 					/1* break; *1/ */
/* 				} */
/* 				if(breaked >= 1.0) */
/* 				{ */
/* 					break; */
/* 				} */
/* 				frac += pl; */
/* 			} */
/* 			frac /= count; */
/* 			sd = clamp(sd - frac, 0.0, 1.0); */
/* 		} */
/* 	} */
/* 	return sd; */
/* } */

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

float doAmbientOcclusion(vec2 tcoord, vec2 uv, vec3 p, vec3 cnorm)
{
    float scale = 1.2, bias = 0.01, intensity = 5; 
    vec3 diff = textureLod(gbuffer.wposition, tcoord + uv, 0).xyz - p;
    vec3 v = normalize(diff);
	float dist = length(diff);
	/* if(dist > 0.7) return 0.0f; */

    float d = dist * scale;
    return max(0.0, dot(cnorm, v) - bias) * (1.0 / (1.0 + d)) * intensity;
}


float ambientOcclusion(vec3 p, vec3 n, float dist_to_eye)
{
	vec2 rnd = normalize(vec2(rand(p.xy), rand(n.xy)));

	float ao = 0.0f;
	float rad = 0.4f / dist_to_eye;

	/* vec2 vec[8]; */ 
	vec2 vec[4]; 
	vec[0] = vec2(1.0, 0.0); 
	vec[1] = vec2(-1.0, 0.0); 
	vec[2] = vec2(0.0, 1.0); 
	vec[3] = vec2(0.0, -1.0);

	/* vec[4] = vec2(1.0, 1.0); */ 
	/* vec[5] = vec2(-1.0, 1.0); */ 
	/* vec[6] = vec2(-1.0, 1.0); */ 
	/* vec[7] = vec2(-1.0, -1.0); */

	int iterations = 4;
	for (int j = 0; j < iterations; ++j)
	{
		vec2 coord1 = reflect(vec[j], rnd) * rad;
		vec2 coord2 = vec2(coord1.x * 0.707 - coord1.y * 0.707,
				coord1.x * 0.707 + coord1.y * 0.707);

		ao += doAmbientOcclusion(texcoord,coord1 * 0.25, p, n);
		ao += doAmbientOcclusion(texcoord,coord2 * 0.5, p, n);
		ao += doAmbientOcclusion(texcoord,coord1 * 0.75, p, n);
		ao += doAmbientOcclusion(texcoord,coord2, p, n);
	}
	ao /= float(iterations) * 4.0f;
	return 1.0f - ao * 0.7f; 
}


// Consts should help improve performance
const float maxSteps = 20;
const float searchDist = 20;
const float searchDistInv = 1.0f / searchDist;

/* in mat4 inv_projection; */

vec3 calcViewPosition(in vec2 TexCoord)
{
	return textureLod(gbuffer.cposition, TexCoord, 0).xyz;
	/* vec3 rawPosition                = vec3(TexCoord, texture(gbuffer.cposition, TexCoord).z); */

    /* vec4 ScreenSpacePosition        = vec4( rawPosition * 2 - 1, 1); */

    /* vec4 ViewPosition               = inv_projection * ScreenSpacePosition; */

    /* return                          ViewPosition.xyz / ViewPosition.w; */
}

vec3 get_proj_coord(vec3 hitCoord) // z = hitCoord.z - depth
{
	vec4 projectedCoord     = projection * vec4(hitCoord, 1.0); \
	projectedCoord.xy      /= projectedCoord.w; \
	projectedCoord.xy       = projectedCoord.xy * 0.5 + 0.5;  \
	float depth             = calcViewPosition(projectedCoord.xy).z; \
	return vec3(projectedCoord.xy, hitCoord.z - depth);
}

vec3 BinarySearch(vec3 dir, inout vec3 hitCoord)
{
    float depth;
 
 
	vec3 pc;
    for(int i = 0; i < 16; i++)
    {
		pc = get_proj_coord(hitCoord);
		if(pc.x > 1.0 || pc.y > 1.0 || pc.x < 0.0 || pc.y < 0.0) break;
		if(abs(pc.z) <= 0.01f)
		{
			return vec3(pc.xy, 1.0f);
		}
 
        dir *= 0.5;
        hitCoord -= dir;    
    }
 
    return vec3(pc.xy, 1.0f);
}

vec3 RayCast(vec3 dir, inout vec3 hitCoord)
{
    dir *= 0.1f;  

    for(int i = 0; i < 64; ++i) {
        hitCoord               += dir; 
		dir *= 1.1f;

		vec3 pc = get_proj_coord(hitCoord);
		if(pc.x > 1.0 || pc.y > 1.0 || pc.x < 0.0 || pc.y < 0.0) break;

		if(pc.z < -2.0f) break;
		if(pc.z < 0.0f)
		{
			return vec3(pc.xy, 1.0f);
			/* return BinarySearch(dir, hitCoord); */
		}


    }

    return vec3(0.0f);
}

float roughnessToSpecularPower(float r)
{
  return (2.0f / (pow(r, 4.0f)) - 2.0f);
}

#define CNST_MAX_SPECULAR_EXP 64

float specularPowerToConeAngle(float specularPower)
{
    // based on phong distribution model
    if(specularPower >= exp2(CNST_MAX_SPECULAR_EXP))
    {
        return 0.0f;
    }
    const float xi = 0.244f;
    float exponent = 1.0f / (specularPower + 1.0f);
    return acos(pow(xi, exponent));
}

float isoscelesTriangleOpposite(float adjacentLength, float coneTheta)
{
    // simple trig and algebra - soh, cah, toa - tan(theta) = opp/adj, opp = tan(theta) * adj, then multiply * 2.0f for isosceles triangle base
    return 2.0f * tan(coneTheta) * adjacentLength;
}

float isoscelesTriangleInRadius(float a, float h)
{
    float a2 = a * a;
    float fh2 = 4.0f * h * h;
    return (a * (sqrt(a2 + fh2) - a)) / (4.0f * h);
}

vec4 ssr(sampler2D screen)
{
	vec3 nor = textureLod(gbuffer.normal, texcoord, 0).xyz;
	vec3 pos = textureLod(gbuffer.cposition, texcoord, 0).xyz;
	vec3 camPos = calcViewPosition(texcoord);

	vec4 specularAll = textureLod(gbuffer.specular, texcoord, 0);
	if(specularAll.a > 0.9) return vec4(0.0);

	vec3 w_pos = textureLod(gbuffer.wposition, texcoord, 0).rgb;
	vec3 c_pos = camera_pos - w_pos;
	vec3 eye_dir = normalize(-c_pos);

	float coef = clamp(1.5 * pow(1.0 + dot(eye_dir, nor), 1), 0.0, 1.0);
	if(coef < 0.01f) return vec4(0.0f);


	// Reflection vector
	vec3 view_normal = (V * vec4(nor, 0.0)).xyz;
	vec3 reflected = normalize(reflect(pos, view_normal));

	// Ray cast
	vec3 hitPos = camPos.xyz; // + vec3(0.0, 0.0, rand(camPos.xz) * 0.2 - 0.1);


	vec3 coords = RayCast(reflected, hitPos);

	vec2 dCoords = abs(vec2(0.5, 0.5) - coords.xy) * 2;
	/* vec2 dCoords = abs((vec2(0.5, 0.5) - texcoord.xy) * 2 ); */
	float screenEdgefactor = (clamp(1.0 - (pow(dCoords.x, 4) + pow(dCoords.y, 4)), 0.0, 1.0));

	/* return vec5(vec3(screenEdgefactor), 1.0); */



    /* vec3 deltaP = hitPos - camPos; */
    vec2 deltaP = coords.xy - texcoord;
    float adjacentLength = length(deltaP);
    float specularPower = roughnessToSpecularPower(specularAll.a);
    float coneTheta = specularPowerToConeAngle(specularPower);
	float oppositeLength = isoscelesTriangleOpposite(adjacentLength, coneTheta);
	float incircleSize = isoscelesTriangleInRadius(oppositeLength, adjacentLength);
	float mipChannel = clamp(log2(incircleSize * 60), 0.0f, 7.0f);
	/* return vec4(vec3(log2(incircleSize * 60)), 1.0f); */
	/* return vec4(vec3(incircleSize * 50), 1.0f); */
	/* return vec4(vec3(mipChannel / 7.0f), 1.0f); */
	/* mipChannel = 0; */
	/* return vec4(reflected.xy, 0.0f, 1.0f); */

	vec3 reflect_color = textureLod(screen, coords.xy, mipChannel).rgb;

	vec3 fallback_color = texture(ambient_map, reflect(eye_dir, nor)).rgb / 2;
	/* vec3 fallback_color = vec3(0.0f); */


	float col = screenEdgefactor * clamp((searchDist - adjacentLength) * searchDistInv,
			0.0, 1.0);
	/* return vec4(vec3(col), coef); */

	if(coords.z <= 0.0f)
		return vec4(specularAll.rgb * fallback_color, coef);


	return vec4(mix(fallback_color, reflect_color, col) * specularAll.rgb, coef);

	/* return vec4(reflect_color, col); */
	/* return vec4(fallback_color * (1.0f - col) + reflect_color * col, 1.0); */

	/* return vec4(mix(fallback_color, reflect_color, col), coef); */

}

#endif

// vim: set ft=c:
