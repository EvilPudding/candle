#ifndef FRAG_COMMON
#define FRAG_COMMON
#include "uniforms.glsl"
#line 5


#define _CAT(a, b) a ## b
#define CAT(a, b) _CAT(a,b)
#define BUFFER uniform struct 

const float M_PI = 3.141592653589793;
const float c_MinRoughness = 0.04;


/* uniform gbuffer_t gbuffer; */
/* uniform sbuffer_t sbuffer; */

/* uniform samplerCube ambient_map; */
/* uniform sampler3D perlin_map; */


flat in vec3 obj_pos;
flat in uvec2 id;
flat in mat4 model;
flat in uint matid;
flat in uvec2 poly_id;

in vec3 poly_color;
in vec3 vertex_position;
in vec2 texcoord;
in mat3 TM;

vec2 sampleCube(const vec3 v, out int faceIndex)
{
	vec3 vAbs = abs(v);
	float ma;
	vec2 uv;
	if(vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)
	{
		faceIndex = v.z < 0.0 ? 5 : 4;
		ma = 0.5 / vAbs.z;
		uv = vec2(v.z < 0.0 ? -v.x : v.x, -v.y);
	}
	else if(vAbs.y >= vAbs.x)
	{
		faceIndex = v.y < 0.0 ? 3 : 2;
		ma = 0.5 / vAbs.y;
		uv = vec2(v.x, v.y < 0.0 ? -v.z : v.z);
	}
	else
	{
		faceIndex = v.x < 0.0 ? 1 : 0;
		ma = 0.5 / vAbs.x;
		uv = vec2(v.x < 0.0 ? v.z : -v.z, -v.y);
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
#define MAX_MIPS 9
float mip_map_level(in vec2 texture_coordinate) // in texel units
{
	return clamp(0.5 * log2(mip_map_scalar(texture_coordinate)), 0.0, float(MAX_MIPS - 1));
}

#define g_indir_w 256
#define g_indir_h 64
#define g_cache_w 64
#define g_cache_h 32

vec4 solveMip(const property_t prop, int mip, vec2 coords, bool draw)
{
	int tiles_per_row = int(ceil(float(prop.size.x) / 128.0));
	int tiles_per_col = int(ceil(float(prop.size.y) / 128.0));
	int offset = prop.layer;

	for (int i = 0; i < MAX_MIPS && i < mip; i++)
	{
		offset += tiles_per_row * tiles_per_col;
		tiles_per_row = int(ceil(0.5 * float(tiles_per_row)));
		tiles_per_col = int(ceil(0.5 * float(tiles_per_col)));
		coords.x /= 2.0;
		coords.y /= 2.0;
	}
	vec2 intile_coords = mod(vec2(coords), 128.0) + 0.5f;

	int x = int(floor(coords.x / 128.0));
	int y = int(floor(coords.y / 128.0));
	int tex_tile = y * tiles_per_row + x + offset;
	/* int tex_tile = offset; */

	vec3 info = texelFetch(g_indir, ivec2(tex_tile % g_indir_w, tex_tile / g_indir_w), 0).rgb;
	int cache_tile = int(info.r * 255.0) + int(info.g * (256.0 * 255.0));

	ivec2 cache_coords = ivec2(cache_tile % g_cache_w, cache_tile / g_cache_w) * 129;
	/* if ( draw && ( intile_coords.x >= 126.0 || intile_coords.y >= 126.0 || */
	    /* intile_coords.x <= 1.0 || intile_coords.y <= 1.0)) */
		/* return vec4(0.0, 1.0, 0.0, 1.0); */
	const vec2 g_cache_size = vec2(g_cache_w * 129, g_cache_h * 129);
	return textureLod(g_cache,
			(vec2(cache_coords) + intile_coords) / g_cache_size, 0.0);
	/* return texelFetch(g_cache, cache_coords + ivec2(floor(intile_coords)), 0); */
}

vec4 textureSVT(const property_t prop, vec2 coords, bool draw)
{
	coords.y = 1.0 - coords.y;
	coords *= prop.scale;
	vec2 rcoords = fract(coords) * vec2(prop.size);
	float mip = mip_map_level(coords * vec2(prop.size));
	int mip0 = int(floor(mip));
	int mip1 = int(ceil(mip));

	return mix(solveMip(prop, mip0, rcoords, draw), solveMip(prop, mip1, rcoords, draw), fract(mip));
	/* return solveMip(prop, 0, rcoords, draw); */
}

vec4 resolveProperty(property_t prop, vec2 coords, bool draw)
{
	/* vec3 tex; */
	if(prop.blend > 0.001)
	{
		return mix(
			prop.color,
			textureSVT(prop, coords, draw),
			prop.blend
		);
	}
	return prop.color;
}

/* float ambient = 0.08; */
/* float ambient = 1.00; */
float linearize(float depth)
{
    return 2.0 * 0.1 * 100.0 / (100.0 + 0.1 - (2.0 * depth - 1.0) * (100.0 - 0.1));
}
float unlinearize(float depth)
{
	return 100.0 * (1.0 - (0.1 / depth)) / (100.0 - 0.1);
}


float lookup_single(vec3 shadowCoord)
{
	int size = 1024 / int(pow(2.0, float(light(lod))));
	int cube_layer;
	ivec2 tc = ivec2(floor(sampleCube(shadowCoord, cube_layer) * float(size)));
	ivec2 pos = ivec2(cube_layer % 2, cube_layer / 2) * size;
	return texelFetch(g_probes, tc + pos + light(pos), 0).r * 90.0f;
}

/* float prec = 0.05; */
float lookup(vec3 coord)
{
	float dist = length(coord);
	float dist2 = lookup_single(coord) - dist;
	return (dist2 > -0.01) ? 1.0 : 0.0;
}

/* SPHEREMAP TRANSFORM */
/* https://aras-p.info/texts/CompactNormalStorage.html */

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

float get_metalness()
{
	return resolveProperty(mat(metalness), texcoord, false).r;
}

vec3 get_position(sampler2D depth)
{
	vec2 pos = pixel_pos();
	vec3 raw_pos = vec3(pos, textureLod(depth, pos, 0.0).r);
	vec4 clip_pos = vec4(raw_pos * 2.0 - 1.0, 1.0);
	vec4 view_pos = camera(inv_projection) * clip_pos;
	return view_pos.xyz / view_pos.w;
}

vec3 get_position(sampler2D depth, vec2 pos)
{
	vec3 raw_pos = vec3(pos, textureLod(depth, pos, 0.0).r);
	vec4 clip_pos = vec4(raw_pos * 2.0 - 1.0, 1.0);
	vec4 view_pos = camera(inv_projection) * clip_pos;
	return view_pos.xyz / view_pos.w;
}

vec3 get_normal(sampler2D buffer)
{
	return decode_normal(texelFetch(buffer, ivec2(int(gl_FragCoord.x),
					int(gl_FragCoord.y)), 0).rg);
}
vec3 get_normal(vec2 tc)
{
	vec3 norm;
	if(has_tex)
	{
		vec3 texcolor = resolveProperty(mat(normal), tc, false).rgb * 2.0 - 1.0;
		norm = TM * texcolor;

	}
	else
	{
		norm = TM[2];
	}
	if(!gl_FrontFacing)
	{
		norm = -norm;
	}

	return normalize(norm);
}
vec3 get_normal()
{
	return get_normal(texcoord);
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

vec2 fTaps_Poisson[] = vec2[](
	vec2(-.326,-.406),
	vec2(-.840,-.074),
	vec2(-.696, .457),
	vec2(-.203, .621),
	vec2( .962,-.195),
	vec2( .473,-.480),
	vec2( .519, .767),
	vec2( .185,-.893),
	vec2( .507, .064),
	vec2( .896, .412),
	vec2(-.322,-.933),
	vec2(-.792,-.598)
);

float shadow_at_dist_no_tan(vec3 vec, float i)
{
	/* vec3 rnd = (vec3(rand(vec.xy), rand(vec.yz), rand(vec.xz)) - 0.5) * i; */
	/* if(lookup(-vec,  rnd) > 0.5) return 1.0; */
	/* return 0.0; */

	vec3 x = vec3(1.0, 0.0, 0.0) * i;
	vec3 y = vec3(0.0, 1.0, 0.0) * i;
	vec3 z = vec3(0.0, 0.0, 1.0) * i;

	if(lookup(-vec + x) > 0.5) return 1.0;
	if(lookup(-vec + y) > 0.5) return 1.0;
	if(lookup(-vec + z) > 0.5) return 1.0;
	if(lookup(-vec - x) > 0.5) return 1.0;
	if(lookup(-vec - y) > 0.5) return 1.0;
	if(lookup(-vec - z) > 0.5) return 1.0;


	return 0.0;
}

float get_shadow(vec3 vec, float point_to_light, float dist_to_eye, float depth)
{
	float ocluder_to_light = lookup_single(-vec);

	float sd = ((ocluder_to_light - point_to_light) > -0.05) ? 0.0 : 1.0;
	if(sd > 0.5)
	{

		/* sd = 0.5; */
		float shadow_len = min(0.4 * (point_to_light / ocluder_to_light - 1.0), 10.0);
		float p = 0.01 * linearize(depth);
		if(shadow_len > 0.001)
		{
			float i;

			float count = 1.0;
			float inc = p;
			float iters = 1.0 + min(round(shadow_len / inc), 20.0);
			inc = shadow_len / iters;

			for (i = inc; count < iters; i += inc)
			{
				if(shadow_at_dist_no_tan(vec, i) > 0.5) break;

				count++;
			}
			if(count == 1.0) return 0.0;
			return (count / iters);
		}
	}
	return 0.0;
}

float doAmbientOcclusion(sampler2D depth, vec2 tcoord, vec2 uv, vec3 p, vec3 cnorm)
{
    float scale = 1.2, bias = 0.01, intensity = 5.0; 
    vec3 diff = get_position(depth, tcoord + uv) - p;
    vec3 v = normalize(diff);
	float dist = length(diff);
	/* if(dist > 0.7) return 0.0; */

    float d = dist * scale;
    return max(0.0, (dot(cnorm, v) - bias) * (1.0 / (1.0 + d)) * intensity);
}

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

float ambientOcclusion(sampler2D depth, vec3 p, vec3 n, float dist_to_eye)
{
	vec2 rnd = normalize(vec2(rand(p.xy), rand(n.xy)));

	float ao = 0.0;
	float rad = 0.4 / dist_to_eye;

	/* vec2 vec[8]; */ 
	/* vec2 vec[4]; */ 
	/* vec[0] = vec2(1.0, 0.0); */ 
	/* vec[1] = vec2(-1.0, 0.0); */ 
	/* vec[2] = vec2(0.0, 1.0); */ 
	/* vec[3] = vec2(0.0, -1.0); */

	/* vec[4] = vec2(1.0, 1.0); */ 
	/* vec[5] = vec2(-1.0, 1.0); */ 
	/* vec[6] = vec2(-1.0, 1.0); */ 
	/* vec[7] = vec2(-1.0, -1.0); */

	int iterations = 12;
	for (int j = 0; j < iterations; ++j)
	{
		vec2 coord1 = reflect(fTaps_Poisson[j], rnd) * rad;
		vec2 coord2 = vec2(coord1.x * 0.707 - coord1.y * 0.707,
				coord1.x * 0.707 + coord1.y * 0.707);

		ao += doAmbientOcclusion(depth, texcoord, coord1 * 0.25, p, n);
		ao += doAmbientOcclusion(depth, texcoord, coord2 * 0.5, p, n);
		ao += doAmbientOcclusion(depth, texcoord, coord1 * 0.75, p, n);
		ao += doAmbientOcclusion(depth, texcoord, coord2, p, n);
	}
	ao /= float(iterations);
	ao = 1.0 - ao * 0.1;
	return clamp(ao, 0.0, 1.0); 
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
    for(int i = 0; i < 16; i++)
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

    for(int i = 0; i < 64; ++i) {
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

float roughnessToSpecularPower(float r)
{
  return (2.0 / (pow(r, 4.0)) - 2.0);
}

#define CNST_1DIVPI 0.31830988618
#define CNST_MAX_SPECULAR_EXP 64.0

float specularPowerToConeAngle(float specularPower)
{
    // based on phong distribution model
    if(specularPower >= exp2(CNST_MAX_SPECULAR_EXP))
    {
        return 0.0;
    }
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

#define saturate(val) (clamp(val, 0.0, 1.0))

vec4 coneSampleWeightedColor(sampler2D screen, vec2 samplePos,
		float mipChannel, float gloss)
{
    vec3 sampleColor = textureLod(screen, samplePos, mipChannel).rgb;
    return vec4(sampleColor * gloss, gloss);
}

vec4 ssr2(sampler2D depth, sampler2D screen, vec4 base_color,
		vec2 metallic_roughness, vec3 nor)
{
	vec2 tc = pixel_pos();
	vec3 pos = get_position(depth, pixel_pos());

	float perceptualRoughness = metallic_roughness.y;
	float metallic = metallic_roughness.x;

	perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);
	metallic = clamp(metallic, 0.0, 1.0);

	if(perceptualRoughness > 0.95) return vec4(0.0);

    float gloss = 1.0 - perceptualRoughness;
    float specularPower = roughnessToSpecularPower(perceptualRoughness);

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

    float coneTheta = specularPowerToConeAngle(specularPower) * 0.5;

    // P1 = pos, P2 = raySS, adjacent length = ||P2 - P1||
    vec2 deltaP = coords.xy - texcoord;
    float adjacentLength = length(deltaP);
    vec2 adjacentUnit = normalize(deltaP);

	int numMips = MAX_MIPS;
    vec4 reflect_color = vec4(0.0);
    float remainingAlpha = 1.0;
    float maxMipLevel = 3.0;
    float glossMult = gloss;

    // cone-tracing using an isosceles triangle to approximate a cone in screen space
    for(int i = 0; i < 1; ++i)
    {
        float oppositeLength = 2.0 * tan(coneTheta) * adjacentLength;
        float incircleSize = isoscelesTriangleInRadius(oppositeLength, adjacentLength);
        vec2 samplePos = texcoord + adjacentUnit * (adjacentLength - incircleSize);
		float mipChannel = clamp(log2(incircleSize), 0.0, maxMipLevel);

        vec4 newColor = coneSampleWeightedColor(screen, samplePos, mipChannel, glossMult);

        remainingAlpha -= newColor.a;
        if(remainingAlpha < 0.0)
        {
            newColor.rgb *= (1.0 - abs(remainingAlpha));
        }
        reflect_color += newColor;

        if(reflect_color.a >= 1.0)
        {
            break;
        }

        adjacentLength = adjacentLength - (incircleSize * 2.0);
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
	float fade = screenEdgefactor * fadeOnRoughness * (1.0 - saturate(remainingAlpha));

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
    float alpha_roughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
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
    float r = pbrInputs.alpha_roughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

float microfacetDistribution(PBRInfo pbrInputs)
{
    float roughnessSq = pbrInputs.alpha_roughness * pbrInputs.alpha_roughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (M_PI * f * f);
}

vec4 pbr(vec4 base_color, vec2 metallic_roughness,
		vec4 light_color, vec3 light_dir, vec3 c_pos,
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
    float alpha_roughness = perceptualRoughness * perceptualRoughness;

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
    /* vec3 reflection = -normalize(reflect(v, c_nor)); */

    float NdotL = clamp(dot(c_nor, l), 0.001, 1.0);
    float NdotV = clamp(abs(dot(c_nor, v)), 0.001, 1.0);
    float NdotH = clamp(dot(c_nor, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

	PBRInfo pbrInputs = PBRInfo( NdotL, NdotV, NdotH, LdotH, VdotH,
			perceptualRoughness, metallic, specularEnvironmentR0,
			specularEnvironmentR90, alpha_roughness, diffuse_color,
			specularColor);

    // Calculate the shading terms for the microfacet specular shading model
    vec3 F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution
    vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
    vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    vec3 color = NdotL * light_color.xyz * (diffuseContrib + specContrib);

    // Apply optional PBR terms for additional (optional) shading
/* #ifdef HAS_OCCLUSIONMAP */
    /* float ao = texture(u_OcclusionSampler, v_UV).r; */
    /* color = mix(color, color * ao, u_OcclusionStrength); */
/* #endif */

/* #ifdef HAS_EMISSIVEMAP */
    /* vec3 emissive = SRGBtoLINEAR(texture(u_EmissiveSampler, v_UV)).rgb * u_EmissiveFactor; */
    /* color += emissive; */
/* #endif */

    return vec4(pow(color,vec3(1.0/2.2)), base_color.a);
}

#endif

// vim: set ft=c:
