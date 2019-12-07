#ifndef FRAG_COMMON
#define FRAG_COMMON
#include "uniforms.glsl"


#define BUFFER uniform struct 

const float M_PI = 3.141592653589793;
const float c_MinRoughness = 0.04;


/* uniform gbuffer_t gbuffer; */
/* uniform sbuffer_t sbuffer; */

/* uniform samplerCube ambient_map; */
/* uniform sampler3D perlin_map; */


flat in uvec2 id;
flat in uint matid;
flat in vec2 object_id;
flat in uvec2 poly_id;
flat in vec3 obj_pos;
flat in mat4 model;

in vec3 poly_color;
in vec3 vertex_position;
in vec3 vertex_tangent;
in vec3 vertex_normal;
in vec3 vertex_world_position;
in float vertex_distance;
in vec2 texcoord;

vec2 sampleCube(const vec3 v, out uint faceIndex, out float z)
{
	vec3 vAbs = abs(v);
	float ma;
	vec2 uv;
	if(vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)
	{
		faceIndex = v.z < 0.0 ? 5u : 4u;
		ma = 0.5 / vAbs.z;
		uv = vec2(v.z < 0.0 ? -v.x : v.x, -v.y);
		z = vAbs.z;
	}
	else if(vAbs.y >= vAbs.x)
	{
		faceIndex = v.y < 0.0 ? 3u : 2u;
		ma = 0.5 / vAbs.y;
		uv = vec2(v.x, v.y < 0.0 ? -v.z : v.z);
		z = vAbs.y;
	}
	else
	{
		faceIndex = v.x < 0.0 ? 1u : 0u;
		ma = 0.5 / vAbs.x;
		uv = vec2(v.x < 0.0 ? v.z : -v.z, -v.y);
		z = vAbs.x;
	}
	return uv * ma + 0.5;
}

vec2 pixel_pos()
{
	/* return gl_SamplePosition.xy; */
	return gl_FragCoord.xy / screen_size;
}

// Does not take into account GL_TEXTURE_MIN_LOD/GL_TEXTURE_MAX_LOD/GL_TEXTURE_LOD_BIAS,
// nor implementation-specific flexibility allowed by OpenGL spec
float mip_map_scalar(in vec2 texture_coordinate) // in texel units
{
    vec2 dx_vtc = dFdx(texture_coordinate);
    vec2 dy_vtc = dFdy(texture_coordinate);
    return max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));
}
#define MAX_MIPS 9u
float mip_map_level(in vec2 texture_coordinate) // in texel units
{
	return clamp(0.5 * log2(mip_map_scalar(texture_coordinate)), 0.0, float(MAX_MIPS - 1u));
}

#define g_indir_w 256u
#define g_indir_h 64u
#define g_cache_w 64u
#define g_cache_h 32u

uint round_power_of_two(uint v)
{
	v--;
	v |= v >> 1u;
	v |= v >> 2u;
	v |= v >> 4u;
	v |= v >> 8u;
	v |= v >> 16u;
	return v + 1u;
}

vec4 solveMip(uint tiles_per_row, uint base_tile, uint mip, vec2 coords,
              out uint tile_out)
{
	float max_mips = log2(float(tiles_per_row));
	float mm = min(max_mips, float(mip));

	uint map_tiles = uint(exp2(2. * (max_mips + 1. - mm)) * (exp2(mm * 2.) - 1.)) / 3u;
	map_tiles += uint(max(0., float(mip) - max_mips));

	uint offset = base_tile + map_tiles;
	uint mip_tpr = tiles_per_row >> mip;

	uvec2 indir_coords = uvec2(floor(coords / (exp2(float(mip)) * 128.0)));
	uint indir_tile = indir_coords.y * mip_tpr + indir_coords.x + offset;
	tile_out = indir_tile;

	vec3 info = texelFetch(g_indir,
	                       ivec2(indir_tile % g_indir_w,
	                             indir_tile / g_indir_w), 0).rgb * 255.0;
	float actual_mip = info.b;

	const vec2 g_cache_size = vec2(g_cache_w, g_cache_h);

	vec2 actual_coords = coords / exp2(actual_mip);
	vec2 intile_coords = mod(actual_coords, 128.0) + 0.5f;
	/* vec2 intile_coords = fract(coords * exp2(float(mip_tpr))) * 128. + 0.5; */

	return textureLod(g_cache,
			(info.xy + intile_coords / 129.) / g_cache_size, 0.0);
}

vec4 textureSVT(uvec2 size, uint base_tile, vec2 coords, out uint tile_out,
                float mip_scale)
{
	coords.y = 1.0 - coords.y;
	vec2 rcoords = fract(coords) * vec2(size);

	uint max_dim = uint(ceil(float(max(size.x, size.y)) / 128.0));
	uint tiles_per_row = round_power_of_two(max_dim);
	/* rcoords *= vec2(size) / vec2(tiles_per_row * 128u); */

	float mip = mip_map_level(coords * vec2(size) * mip_scale);
	uint mip0 = uint(floor(mip));
	uint mip1 = uint(ceil(mip));

	uint ignore;
	return mix(solveMip(tiles_per_row, base_tile, mip0, rcoords, tile_out),
	           solveMip(tiles_per_row, base_tile, mip1, rcoords, ignore), fract(mip));
	/* return solveMip(prop, 0, rcoords, draw); */
}

#define NEAR 0.1
#define FAR 100.0
float linearize(float depth)
{
    return (2.0 * NEAR * FAR) / ((FAR + NEAR) - (2.0 * depth - 1.0) * (FAR - NEAR));
}

float unlinearize(float depth)
{
	return FAR * (1.0 - (NEAR / depth)) / (FAR - NEAR);
}

const vec4 bitSh = vec4(256. * 256. * 256., 256. * 256., 256., 1.);
const vec4 bitMsk = vec4(0.,vec3(1./256.0));
const vec4 bitShifts = vec4(1.) / bitSh;

vec4 encode_float_rgba_unit( float v ) {
	vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * v;
	enc = fract(enc);
	enc -= enc.yzww * vec4(1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0, 0.0);
	return enc;
}

float decode_float_rgba_unit( vec4 rgba ) {
	return dot(rgba, vec4(1.0, 1.0 / 255.0, 1.0 / 65025.0, 1.0 / 16581375.0) );
}

vec4 encode_float_rgba (float value) {
	value /= 256.0;
    vec4 comp = fract(value * bitSh);
    comp -= comp.xxyz * bitMsk;
    return comp;
}

float decode_float_rgba (vec4 color) {
    return dot(color, bitShifts) * (256.0);
}
/* -------------------------------------------------------------------------- */

float lookup_single(vec3 shadowCoord, out float z)
{
	uint size = 1024u / uint(pow(2.0, float(light(lod))));
	uint cube_layer;
	uvec2 tc = uvec2(floor(sampleCube(shadowCoord, cube_layer, z) * float(size)));
	uvec2 pos = uvec2(cube_layer % 2u, cube_layer / 2u) * size;
	vec4 distance = texelFetch(g_probes, ivec2(tc + pos + light(pos)), 0);
	return linearize(decode_float_rgba(distance));
}

/* float prec = 0.05; */
float lookup(vec3 coord)
{
	/* float dist = length(coord); */
	float z;
	float dist = lookup_single(coord, z);
	return (dist > z - 0.08) ? 1.0 : 0.0;
}

/* SPHEREMAP TRANSFORM */
/* https://aras-p.info/texts/CompactNormalStorage.html */

/* vec2 encode_normal(vec3 n) */
/* { */
/*     vec2 enc = normalize(n.xy) * (sqrt(-n.z*0.5+0.5)); */
/*     enc = enc*0.5+0.5; */
/*     return enc; */
/* } */
/* vec3 decode_normal(vec2 enc) */
/* { */
/*     vec4 nn = vec4(enc, 0.0, 0.0) * vec4(2.0, 2.0, 0.0, 0.0) + vec4(-1.0, -1.0, 1.0, -1.0); */
/*     float l = dot(nn.xyz, -nn.xyw); */
/*     nn.z = l; */
/*     nn.xy *= sqrt(l); */
/*     return nn.xyz * 2.0 + vec3(0.0, 0.0, -1.0); */
/* } */

vec2 encode_normal(vec3 n)
{
    float p = sqrt(n.z * 8.0 + 8.0);
    return vec2(n.xy/p + 0.5);
}

vec3 decode_normal(vec2 enc)
{
    vec2 fenc = enc * 4.0 - 2.0;
    float f = dot(fenc,fenc);
    float g = sqrt(1.0 - f / 4.0);
    vec3 n;
    n.xy = fenc * g;
    n.z = 1.0 - f / 2.0;
    return n;
}

/* ------------------- */

vec3 get_position(sampler2D depth)
{
	vec2 pos = pixel_pos();
	vec3 raw_pos = vec3(pos, textureLod(depth, pos, 0.0).r);
	vec4 clip_pos = vec4(raw_pos * 2.0 - 1.0, 1.0);
	vec4 view_pos = camera(inv_projection) * clip_pos;
	return view_pos.xyz / view_pos.w;
}

vec3 get_position(float depth, vec2 pos)
{
	vec3 raw_pos = vec3(pos, depth);
	vec4 clip_pos = vec4(raw_pos * 2.0 - 1.0, 1.0);
	vec4 view_pos = camera(inv_projection) * clip_pos;
	return view_pos.xyz / view_pos.w;
}

vec3 get_position(sampler2D depth, vec2 pos)
{
	return get_position(textureLod(depth, pos, 0.0).r, pos);
}

vec3 get_normal(sampler2D buffer)
{
	return decode_normal(texelFetch(buffer, ivec2(gl_FragCoord.x,
					gl_FragCoord.y), 0).rg);
}

mat3 TM()
{
	vec3 vertex_bitangent = cross(normalize(vertex_tangent), normalize(vertex_normal));
	return mat3(vertex_tangent, vertex_bitangent, vertex_normal);
}
vec3 get_normal(vec3 color)
{

	vec3 norm;
	if(has_tex)
	{
		vec3 texcolor = color * 2.0 - 1.0;
		norm = TM() * texcolor;
	}
	else
	{
		norm = vertex_normal;
	}
	if(!gl_FrontFacing)
	{
		norm = -norm;
	}

	return normalize(norm);
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

float ditherPattern[16] = float[16](0.0f, 0.5f, 0.125f, 0.625f,
									0.75f, 0.22f, 0.875f, 0.375f,
									0.1875f, 0.6875f, 0.0625f, 0.5625,
									0.9375f, 0.4375f, 0.8125f, 0.3125);

vec3 ppp[] = vec3[](
		vec3(-0.6025853861145103, 0.4154745183350379, 0.6813749166615211),
		vec3(0.17785620075237496, 0.721257801464502, 0.6694433177502961),
		vec3(0.40907339758770134, 0.8591826554711636, 0.30735015848958713),
		vec3(0.8016505389371444, -0.387637413020948, -0.45507543270123113),
		vec3(0.9967060392294212, -0.08048051062902849, 0.009997938411999658),
		vec3(0.11242268603816562, 0.9348998373902656, -0.33663546116180154),
		vec3(-0.6735495643495322, 0.313093085567789, -0.669554855209188),
		vec3(0.516552787451761, 0.321101595974388, 0.7937675874199681),
		vec3(-0.6653981859746977, -0.03128467474575039, 0.7458327716235285),
		vec3(0.043770034870918025, -0.979515190833735, -0.19655578081895841),
		vec3(0.14642092069816068, 0.5859713297143408, -0.7969934220147054),
		vec3(0.968975404819239, -0.07683527155447646, 0.234910633860074));


vec3 fTaps_Poisson[] = vec3[](
	vec3(0.068824, -0.326151,   0.3),
	vec3(0.248043, 0.222679,   0.3),
	vec3(-0.316867, 0.103472,   0.3),
	vec3(-0.525182, 0.410644,   0.6),
	vec3(-0.618219, -0.249499,   0.6),
	vec3(-0.093037, -0.660143,   0.6),
	vec3(0.525182, -0.410644,   0.6),
	vec3(0.618219, 0.249499,   0.6),
	vec3(0.093037, 0.660143,   0.6),
	vec3(0.536822, -0.843695,   1.0),
	vec3(0.930210, -0.367028,   1.0),
	vec3(0.968289, 0.249832,   1.0),
	vec3(0.636515, 0.771264,   1.0),
	vec3(0.061613, 0.998100,   1.0),
	vec3(-0.536822, 0.843695,   1.0),
	vec3(-0.930210, 0.367028,   1.0),
	vec3(-0.968289, -0.249832,   1.0),
	vec3(-0.636515, -0.771264,   1.0),
	vec3(-0.061613, -0.998100,   1.0)
);


float get_shadow(vec3 vec, float point_to_light, float dist_to_eye, float depth)
{
	float z;
	float ocluder_to_light = lookup_single(-vec, z);
	if (ocluder_to_light > z - 0.08) return 0.0;

	ocluder_to_light = min(z, ocluder_to_light);
	float ditherValue = ditherPattern[(int(gl_FragCoord.x) % 4) * 4 + (int(gl_FragCoord.y) % 4)];

	float shadow_len = min(0.8 * (point_to_light / ocluder_to_light - 1.0), 10.0);
	if(shadow_len > 0.001)
	{
		vec3 normal = normalize(vec);
		vec3 tangent = cross(normal, vec3(0.0, 1.0, 0.0));
		vec3 bitangent = cross(normal, tangent);
		float min_dist = 1.0;
		for (uint j = 0u; j < 19u; j++)
		{
			vec3 offset = fTaps_Poisson[j] * ((4.0/3.0) * ditherValue);
			if(lookup(-vec + (offset.x * tangent + offset.y * bitangent) * shadow_len) > 0.5)
			{
				min_dist = offset.z;
				break;
			}
		}
		return min_dist;
	}
	return 0.0;
}


// Consts should help improve performance
const float searchDist = 20.0;
const float searchDistInv = 1.0 / searchDist;

vec3 get_proj_coord(sampler2D depthmap, vec3 hitCoord)
	// z = hitCoord.z - depth
{
	vec4 projectedCoord     = camera(projection) * vec4(hitCoord, 1.0); \
	projectedCoord.xy      /= projectedCoord.w; \
	projectedCoord.xy       = projectedCoord.xy * 0.5 + 0.5;  \
	float depth             = get_position(depthmap, projectedCoord.xy).z; \
	return vec3(projectedCoord.xy, hitCoord.z - depth);
}

vec3 BinarySearch(sampler2D depthmap, vec3 dir, inout vec3 hitCoord)
{
    float depth;
	vec3 pc;
    for(uint i = 0u; i < 16u; i++)
    {
		pc = get_proj_coord(depthmap, hitCoord);
		if(pc.x > 1.0 || pc.y > 1.0 || pc.x < 0.0 || pc.y < 0.0) break;
		if(abs(pc.z) <= 0.01)
		{
			return vec3(pc.xy, 1.0);
		}
 
        dir *= 0.5;
        hitCoord -= dir;    
    }
 
    return vec3(pc.xy, 1.0);
}

vec3 RayCast(sampler2D depth, vec3 dir, inout vec3 hitCoord)
{
    dir *= 0.1;  

    for(uint i = 0u; i < 64u; ++i) {
        hitCoord               += dir; 
		dir *= 1.1;

		vec3 pc = get_proj_coord(depth, hitCoord);
		if(pc.x > 1.0 || pc.y > 1.0 || pc.x < 0.0 || pc.y < 0.0) break;

		if(pc.z < -2.0) break;
		if(pc.z < 0.0)
		{
			return vec3(pc.xy, 1.0);
			/* return BinarySearch(dir, hitCoord); */
		}
    }

    return vec3(0.0);
}

#define CNST_1DIVPI 0.31830988618
#define CNST_MAX_SPECULAR_EXP 64.0


float roughnessToSpecularPower(float r)
{
  return pow(1.0 - r, 2.0);
  /* return 2.0 / pow(r, 4.0) - 2.0; */
}
float specularPowerToConeAngle(float specularPower)
{
    // based on phong distribution model
    /* if(specularPower >= exp2(CNST_MAX_SPECULAR_EXP)) */
    /* { */
        /* return 0.0; */
    /* } */
    const float xi = 0.244;
    float exponent = 1.0 / (specularPower + 1.0);
    return acos(pow(xi, exponent));
}

float isoscelesTriangleInRadius(float a, float h)
{
    float a2 = a * a;
    float fh2 = 4.0 * h * h;
    return (a * (sqrt(a2 + fh2) - a)) / (4.0 * h);
}

vec3 fresnelSchlick(vec3 F0, float cosTheta)
{
	return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec4 ssr2(sampler2D depth, sampler2D screen, vec4 base_color,
		vec2 metallic_roughness, vec3 nor)
{
	vec2 tc = pixel_pos();
	vec3 pos = get_position(depth, pixel_pos());

	float perceptualRoughness = metallic_roughness.y;
	float metallic = metallic_roughness.x;

	perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);

	if(perceptualRoughness > 0.95) return vec4(0.0);

    float gloss = 1.0 - perceptualRoughness;
    float specularPower = roughnessToSpecularPower(perceptualRoughness);
	perceptualRoughness *= 0.3;

	vec3 w_pos = (camera(model) * vec4(pos, 1.0)).xyz;
	vec3 w_nor = (camera(model) * vec4(nor, 0.0)).xyz;
	vec3 c_pos = camera(pos) - w_pos;
	vec3 eye_dir = normalize(-c_pos);

	// Reflection vector
	vec3 reflected = normalize(reflect(pos, nor));

	// Ray cast
	vec3 hitPos = pos.xyz; // + vec3(0.0, 0.0, rand(camPos.xz) * 0.2 - 0.1);

	vec3 coords = RayCast(depth, reflected, hitPos);

	vec2 dCoords = abs(vec2(0.5, 0.5) - coords.xy) * 2.0;
	/* vec2 dCoords = abs((vec2(0.5, 0.5) - texcoord.xy) * 2 ); */
	float screenEdgefactor = (clamp(1.0 -
				(pow(dCoords.x, 4.0) + pow(dCoords.y, 4.0)), 0.0, 1.0));

	vec3 fallback_color = vec3(0.0);
	/* vec3 fallback_color = texture(ambient_map, reflect(eye_dir, nor)).rgb / (mipChannel+1); */

    // P1 = pos, P2 = raySS, adjacent length = ||P2 - P1||
    vec2 deltaP = coords.xy - texcoord;
    float adjacentLength = length(deltaP);
    vec2 adjacentUnit = normalize(deltaP);

	uint numMips = MAX_MIPS;
    vec4 reflect_color = vec4(0.0);
    float maxMipLevel = 3.0;
    float glossMult = gloss;

    // cone-tracing using an isosceles triangle to approximate a cone in screen space
    for(int i = -4; i <= 4; ++i)
    {
		float len = adjacentLength * (float(i) / 4.0) * perceptualRoughness * 0.3;
        vec2 samplePos = coords.xy + vec2(adjacentUnit.y, -adjacentUnit.x) * len;
		float mipChannel = perceptualRoughness * adjacentLength * 40.0;

		glossMult = 1.0;
		vec4 newColor = textureLod(screen, samplePos, mipChannel).rgba;

        reflect_color += clamp(newColor / 15.0, 0.0, 1.0);

        /* if(reflect_color.a >= 1.0) */
        /* { */
            /* break; */
        /* } */

        glossMult *= gloss;
    }
	vec3 f0 = vec3(0.04);

	/* return vec4(specularColor, 1); */
	vec3 specularColor = mix(f0, base_color.rgb, metallic);
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    float NdotL = clamp(dot(w_nor, -eye_dir), 0.001, 1.0);

    specularColor = fresnelSchlick(vec3(reflectance), NdotL) * CNST_1DIVPI;
    /* float fadeOnRoughness = saturate(gloss * 4.0); */
    float fadeOnRoughness = 1.0;
	float fade = screenEdgefactor * fadeOnRoughness;

	return vec4(mix(fallback_color,
				reflect_color.xyz * specularColor, fade), 1.0);
}

/* * * * * * * * * * * * * * * * * * * * * PBR * * * * * * * * * * * * * * * * * * * * */
//     FROM https://github.com/KhronosGroup/glTF-WebGL-PBR

struct PBRInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector
    float VdotH;                  // cos angle between view direction and half vector
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    float metalness;              // metallic value at the surface
    vec3 reflectance0;            // full reflectance color (normal incidence angle)
    vec3 reflectance90;           // reflectance color at grazing angle
    float alpha_roughness_sq;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 diffuse_color;            // color contribution from diffuse lighting
    vec3 specularColor;           // color contribution from specular lighting
};

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
    #ifdef MANUAL_SRGB
    #ifdef SRGB_FAST_APPROXIMATION
    vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
    #else //SRGB_FAST_APPROXIMATION
    vec3 bLess = step(vec3(0.04045),srgbIn.xyz);
    vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );
    #endif //SRGB_FAST_APPROXIMATION
    return vec4(linOut,srgbIn.w);;
    #else //MANUAL_SRGB
    return srgbIn;
    #endif //MANUAL_SRGB
}

vec3 diffuse(PBRInfo pbrInputs)
{
    return pbrInputs.diffuse_color / M_PI;
}

vec3 specularReflection(PBRInfo pbrInputs)
{
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

float geometricOcclusion(PBRInfo pbrInputs)
{
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alpha_roughness_sq;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r + (1.0 - r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r + (1.0 - r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

float microfacetDistribution(PBRInfo pbrInputs)
{
    float roughnessSq = pbrInputs.alpha_roughness_sq;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (M_PI * f * f);
}

vec4 pbr(vec4 base_color, vec2 metallic_roughness,
		vec3 light_color, vec3 light_dir, vec3 c_pos,
		vec3 c_nor)
{
    // Metallic and Roughness material properties are packed together
    // In glTF, these factors can be specified by fixed scalar values
    // or from a metallic-roughness map
    float perceptualRoughness = metallic_roughness.y;
    float metallic = metallic_roughness.x;

    perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);
    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    float alpha_roughness_sq = perceptualRoughness * perceptualRoughness;
	alpha_roughness_sq = alpha_roughness_sq * alpha_roughness_sq;

    // The albedo may be defined from a base texture or a flat color

    vec3 f0 = vec3(0.04);
    vec3 diffuse_color = base_color.rgb * (vec3(1.0) - f0);
    diffuse_color *= 1.0 - metallic;
    vec3 specularColor = mix(f0, base_color.rgb, metallic);

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    vec3 v = normalize(-c_pos);        // Vector from surface point to camera
    vec3 l = normalize(light_dir);             // Vector from surface point to light
    vec3 h = normalize(l+v);                          // Half vector between both l and v

    float NdotL = clamp(dot(c_nor, l), 0.001, 1.0);
    float NdotV = clamp(abs(dot(c_nor, v)), 0.001, 1.0);
    float NdotH = clamp(dot(c_nor, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

	PBRInfo pbrInputs = PBRInfo( NdotL, NdotV, NdotH, LdotH, VdotH,
			perceptualRoughness, metallic, specularEnvironmentR0,
			specularEnvironmentR90, alpha_roughness_sq, diffuse_color,
			specularColor);

    // Calculate the shading terms for the microfacet specular shading model
    vec3  F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution
    vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
    vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    vec3 color = NdotL * light_color * (diffuseContrib + specContrib);

    return vec4(pow(color,vec3(1.0/2.2)), base_color.a);
}

#endif

// vim: set ft=c:
