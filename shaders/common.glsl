#ifndef FRAG_COMMON
#define FRAG_COMMON
#line 3

#include "uniforms.glsl"

#define _CAT(a, b) a ## b
#define CAT(a, b) _CAT(a,b)
#define BUFFER uniform struct 

const float M_PI = 3.141592653589793;
const float c_MinRoughness = 0.04;


/* uniform gbuffer_t gbuffer; */
/* uniform sbuffer_t sbuffer; */

/* uniform samplerCube ambient_map; */
/* uniform sampler3D perlin_map; */


in vec2 poly_id;
in vec3 poly_color;
in flat vec3 obj_pos;
in vec3 vertex_position;

in vec2 texcoord;

in mat4 model;
in mat3 TM;

in flat uint matid;
in flat uvec2 id;

vec2 pixel_pos()
{
	/* return gl_SamplePosition.xy; */
	return gl_FragCoord.xy / screen_size;
}


vec4 resolveProperty(property_t prop, vec2 coords)
{
	/* vec3 tex; */
	if(prop.blend > 0.001)
	{
		coords *= prop.scale;
		return mix(
			prop.color,
			texture2D(prop.texture, vec2(coords.x, 1.0f-coords.y)),
			/* textureLod(prop.texture, prop.scale * coords, 3), */
			prop.blend
		);
	}
	return prop.color;
}

/* float ambient = 0.08; */
/* float ambient = 1.00; */

float lookup_single(vec3 shadowCoord)
{
	return texture(light(shadow_map), shadowCoord).a;
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
    float p = sqrt(n.z * 8.0f + 8.0f);
    return vec2(n.xy/p + 0.5f);
}

vec3 decode_normal(vec2 enc)
{
    vec2 fenc = enc * 4.0f - 2.0f;
    float f = dot(fenc,fenc);
    float g = sqrt(1.0f - f / 4.0f);
    vec3 n;
    n.xy = fenc * g;
    n.z = 1.0f - f / 2.0f;
    return n;
}

/* ------------------- */

vec4 get_emissive()
{
	return resolveProperty(mat(emissive), texcoord);
}

float get_metalness()
{
	return resolveProperty(mat(metalness), texcoord).r;
}

float get_roughness()
{
	return resolveProperty(mat(roughness), texcoord).r;
}


vec4 get_transparency()
{
	return resolveProperty(mat(transparency), texcoord);
}


vec3 get_position(sampler2D depth)
{
	vec2 pos = pixel_pos();
	vec3 raw_pos = vec3(pos, textureLod(depth, pos, 0).r);
	vec4 clip_pos = vec4(raw_pos * 2.0 - 1.0, 1.0);
	vec4 view_pos = camera(inv_projection) * clip_pos;
	return view_pos.xyz / view_pos.w;
}

vec3 get_position(sampler2D depth, vec2 pos)
{
	vec3 raw_pos = vec3(pos, textureLod(depth, pos, 0).r);
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
	if(has_tex == 1)
	{
		vec3 texcolor = resolveProperty(mat(normal), tc).rgb * 2.0f - 1.0f;
		return normalize(TM * texcolor);

	}
	return normalize(TM[2]);
}
vec3 get_normal()
{
	if(has_tex == 1)
	{
		vec3 texcolor = resolveProperty(mat(normal), texcoord).rgb * 2.0f - 1.0f;
		return normalize(TM * texcolor);

	}
	return normalize(TM[2]);
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


	return 0;
}

float linearize(float depth)
{
    return 2.0 * 0.1f * 100.0f / (100.0f + 0.1f - (2.0 * depth - 1.0) * (100.0f - 0.1f));
}
float unlinearize(float depth)
{
	return 100.0f * (1.0f - (0.1f / depth)) / (100.0f - 0.1f);
}

float get_shadow(vec3 vec, float point_to_light, float dist_to_eye, float depth)
{
	float ocluder_to_light = lookup_single(-vec);

	float sd = ((ocluder_to_light - point_to_light) > -0.05) ? 0.0 : 1.0;
	if(sd > 0.5)
	{

		/* sd = 0.5; */
		float shadow_len = min(0.8 * (point_to_light / ocluder_to_light - 1), 10);
		float p = 0.01 * linearize(depth);
		if(shadow_len > 0.001)
		{
			float i;

			float count = 1;
			float inc = p;
			float iters = 1 + min(round(shadow_len / inc), 20);
			inc = shadow_len / iters;

			for (i = inc; count < iters; i += inc)
			{
				if(shadow_at_dist_no_tan(vec, i) > 0.5) break;

				count++;
			}
			if(count == 1) return 0;
			return (count / iters);
		}
	}
	return 0.0f;
}

float doAmbientOcclusion(sampler2D depth, vec2 tcoord, vec2 uv, vec3 p, vec3 cnorm)
{
    float scale = 1.2, bias = 0.01, intensity = 5; 
    vec3 diff = get_position(depth, tcoord + uv) - p;
    vec3 v = normalize(diff);
	float dist = length(diff);
	/* if(dist > 0.7) return 0.0f; */

    float d = dist * scale;
    return max(0.0, dot(cnorm, v) - bias) * (1.0 / (1.0 + d)) * intensity;
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

	float ao = 0.0f;
	float rad = 0.4f / dist_to_eye;

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
	ao /= float(iterations) * 4.0f;
	return clamp(1.0f - ao * 0.4f, 0.0f, 1.0f); 
}


// Consts should help improve performance
const float maxSteps = 20;
const float searchDist = 20;
const float searchDistInv = 1.0f / searchDist;

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
		if(abs(pc.z) <= 0.01f)
		{
			return vec3(pc.xy, 1.0f);
		}
 
        dir *= 0.5;
        hitCoord -= dir;    
    }
 
    return vec3(pc.xy, 1.0f);
}

vec3 RayCast(sampler2D depth, vec3 dir, inout vec3 hitCoord)
{
    dir *= 0.1f;  

    for(int i = 0; i < 64; ++i) {
        hitCoord               += dir; 
		dir *= 1.1f;

		vec3 pc = get_proj_coord(depth, hitCoord);
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

float isoscelesTriangleInRadius(float a, float h)
{
    float a2 = a * a;
    float fh2 = 4.0f * h * h;
    return (a * (sqrt(a2 + fh2) - a)) / (4.0f * h);
}

vec3 fresnelSchlick(vec3 F0, float cosTheta)
{
	return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
}

#define saturate(val) (clamp(val, 0.0f, 1.0f))

vec4 coneSampleWeightedColor(sampler2D screen, vec2 samplePos,
		float mipChannel, float gloss)
{
    vec3 sampleColor = textureLod(screen, samplePos, mipChannel).rgb;
    return vec4(sampleColor * gloss, gloss);
}

#define CNST_1DIVPI 0.31830988618
#define CNST_MAX_SPECULAR_EXP 64
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

    float gloss = 1.0f - perceptualRoughness;
    float specularPower = roughnessToSpecularPower(perceptualRoughness);

	vec3 w_pos = (camera(model) * vec4(pos, 1.0f)).xyz;
	vec3 w_nor = (camera(model) * vec4(nor, 0.0f)).xyz;
	vec3 c_pos = camera(pos) - w_pos;
	vec3 eye_dir = normalize(-c_pos);

	// Reflection vector
	vec3 reflected = normalize(reflect(pos, nor));

	// Ray cast
	vec3 hitPos = pos.xyz; // + vec3(0.0, 0.0, rand(camPos.xz) * 0.2 - 0.1);

	vec3 coords = RayCast(depth, reflected, hitPos);

	vec2 dCoords = abs(vec2(0.5, 0.5) - coords.xy) * 2;
	/* vec2 dCoords = abs((vec2(0.5, 0.5) - texcoord.xy) * 2 ); */
	float screenEdgefactor = (clamp(1.0 - (pow(dCoords.x, 4) + pow(dCoords.y, 4)), 0.0, 1.0));

	vec3 fallback_color = vec3(0.0f);
	/* vec3 fallback_color = texture(ambient_map, reflect(eye_dir, nor)).rgb / (mipChannel+1); */

    float coneTheta = specularPowerToConeAngle(specularPower) * 0.5f;

    // P1 = pos, P2 = raySS, adjacent length = ||P2 - P1||
    vec2 deltaP = coords.xy - texcoord;
    float adjacentLength = length(deltaP);
    vec2 adjacentUnit = normalize(deltaP);

	int numMips = textureQueryLevels(screen);
    vec4 reflect_color = vec4(0.0f);
    float remainingAlpha = 1.0f;
    float maxMipLevel = 3.0f;
    float glossMult = gloss;

    // cone-tracing using an isosceles triangle to approximate a cone in screen space
    for(int i = 0; i < 14; ++i)
    {
        float oppositeLength = 2.0f * tan(coneTheta) * adjacentLength;
        float incircleSize = isoscelesTriangleInRadius(oppositeLength, adjacentLength);
        vec2 samplePos = texcoord + adjacentUnit * (adjacentLength - incircleSize);
		float mipChannel = clamp(log2(incircleSize), 0.0f, maxMipLevel);

        vec4 newColor = coneSampleWeightedColor(screen, samplePos, mipChannel, glossMult);

        remainingAlpha -= newColor.a;
        if(remainingAlpha < 0.0f)
        {
            newColor.rgb *= (1.0f - abs(remainingAlpha));
        }
        reflect_color += newColor;

        if(reflect_color.a >= 1.0f)
        {
            break;
        }

        adjacentLength = adjacentLength - (incircleSize * 2.0f);
        glossMult *= gloss;
    }
	vec3 f0 = vec3(0.04);

	/* return vec4(specularColor, 1); */
	vec3 specularColor = mix(f0, base_color.rgb, metallic);
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    float NdotL = clamp(dot(w_nor, -eye_dir), 0.001, 1.0);

    specularColor = fresnelSchlick(vec3(reflectance), NdotL) * CNST_1DIVPI;
    /* float fadeOnRoughness = saturate(gloss * 4.0f); */
    float fadeOnRoughness = 1;
	float fade = screenEdgefactor * fadeOnRoughness * (1.0f - saturate(remainingAlpha));

	return vec4(mix(fallback_color,
				reflect_color.xyz * specularColor, fade), 1.0f);
}

#endif

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
    /* float ao = texture2D(u_OcclusionSampler, v_UV).r; */
    /* color = mix(color, color * ao, u_OcclusionStrength); */
/* #endif */

/* #ifdef HAS_EMISSIVEMAP */
    /* vec3 emissive = SRGBtoLINEAR(texture2D(u_EmissiveSampler, v_UV)).rgb * u_EmissiveFactor; */
    /* color += emissive; */
/* #endif */

    return vec4(pow(color,vec3(1.0/2.2)), base_color.a);
}

/* FROM */
/* https://developer.nvidia.com/sites/default/files/akamai/gamedev/docs/PCSS_Integration.pdf */

/* #define BLOCKER_SEARCH_NUM_SAMPLES 16 */ 
/* #define PCF_NUM_SAMPLES 16 */ 
/* #define NEAR_PLANE 9.5 */ 
/* #define LIGHT_WORLD_SIZE .5 */ 
/* #define LIGHT_FRUSTUM_WIDTH 3.75 */ 
/* // Assuming that LIGHT_FRUSTUM_WIDTH == LIGHT_FRUSTUM_HEIGHT */ 
/* #define LIGHT_SIZE_UV (LIGHT_WORLD_SIZE / LIGHT_FRUSTUM_WIDTH) */ 
/* vec2 poissonDisk[16] = vec2[] ( */ 
/* 	vec2( -0.94201624, -0.39906216 ), */ 
/* 	vec2( 0.94558609, -0.76890725 ), */ 
/* 	vec2( -0.094184101, -0.92938870 ), */ 
/* 	vec2( 0.34495938, 0.29387760 ), */ 
/* 	vec2( -0.91588581, 0.45771432 ), */ 
/* 	vec2( -0.81544232, -0.87912464 ), */ 
/* 	vec2( -0.38277543, 0.27676845 ), */ 
/* 	vec2( 0.97484398, 0.75648379 ), */ 
/* 	vec2( 0.44323325, -0.97511554 ), */ 
/* 	vec2( 0.53742981, -0.47373420 ), */ 
/* 	vec2( -0.26496911, -0.41893023 ), */ 
/* 	vec2( 0.79197514, 0.19090188 ), */ 
/* 	vec2( -0.24188840, 0.99706507 ), */ 
/* 	vec2( -0.81409955, 0.91437590 ), */ 
/* 	vec2( 0.19984126, 0.78641367 ), */ 
/* 	vec2( 0.14383161, -0.14100790 ) */ 
/* ); */ 

/* float PenumbraSize( float zReceiver, float zBlocker) */ 
/* { */ 
/* 	return (zReceiver - zBlocker) / zBlocker; */ 
/* } */ 

/* void FindBlocker( out float avgBlockerDepth,  out float numBlockers, vec2 uv, float zReceiver ) */ 
/* { */ 
/* 	//This uses similar triangles to compute what */  
/* 	//area of the shadow map we should search */ 
/* 	float searchWidth = LIGHT_SIZE_UV * (zReceiver - NEAR_PLANE) / zReceiver; */ 
/* 	float blockerSum = 0; */ 
/* 	numBlockers = 0; */ 
/* 	for( int i = 0; i < BLOCKER_SEARCH_NUM_SAMPLES; ++i ) */ 
/* 	{ */ 
/* 		float shadowMapDepth = texture( light_shadow_map,  uv + poissonDisk[i] * searchWidth); */ 
/* 		if ( shadowMapDepth < zReceiver ) { */ 
/* 			blockerSum += shadowMapDepth; */ 
/* 			numBlockers++; */ 
/* 		} */ 
/* 	} */ 
/* 	avgBlockerDepth = blockerSum / numBlockers; */ 
/* } */ 

/* float PCF_Filter( */ 
/* 		vec2 */
/* 		uv, float zReceiver, */ 
/* 		float filterRadiusUV ) */ 
/* { */ 
/* 	float sum = 0.0f; */ 
/* 	for ( int i = 0; i < PCF_NUM_SAMPLES; ++i ) */ 
/* 	{ */ 
/* 		vec2 offset = poissonDisk[i] * filterRadiusUV; */ 
/* 		sum += tDepthMap.SampleCmpLevelZero(PCF_Sampler, uv + offset, zReceiver); */ 
/* 	} */ 
/* 	return */
/* 		sum / PCF_NUM_SAMPLES; */ 
/* } */ 
/* float PCSS ( Texture2D shadowMapTex, vec3 coords  ) */ 
/* { */ 
/* 	vec2 uv = coords.xy; */ 
/* 	float zReceiver = coords.z; */ 
/* 	// Assumed to be eye-space z in this code */
/* 	// STEP 1: blocker search */ 
/* 	float avgBlockerDepth = 0; */ 
/* 	float numBlockers = 0; */ 
/* 	FindBlocker( avgBlockerDepth, numBlockers, uv, zReceiver ); */ 
/* 	if( numBlockers < 1 ) */   
/* 		//There are no occluders so early out (this saves filtering) */ 
/* 		return */
/* 			1.0f; */ 
/* 	// STEP 2: penumbra size */ 
/* 	float penumbraRatio = PenumbraSize(zReceiver, avgBlockerDepth); */     
/* 	float filterRadiusUV = penumbraRatio * LIGHT_SIZE_UV * NEAR_PLANE / coords.z; */ 
/* 	// STEP 3: filtering */ 
/* 	return */
/* 		PCF_Filter( uv, zReceiver, filterRadiusUV ); */ 
/* } */



// vim: set ft=c:
