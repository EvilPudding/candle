#ifndef FRAG_COMMON
#define FRAG_COMMON
#line 3
#extension GL_ARB_texture_query_levels : enable
#extension GL_EXT_shader_texture_lod: enable
#extension GL_OES_standard_derivatives : enable

#define _CAT(a, b) a ## b
#define CAT(a, b) _CAT(a,b)
#define BUFFER uniform struct 
struct property_t
{
	float texture_blend;
	sampler2D texture;
	float texture_scale;
	vec4 color;
};

uniform vec2 output_size;

const float M_PI = 3.141592653589793;
const float c_MinRoughness = 0.04;

uniform property_t albedo;
uniform property_t roughness;
uniform property_t metalness;
uniform property_t transparency;
uniform property_t normal;
uniform property_t emissive;


/* uniform gbuffer_t gbuffer; */
/* uniform sbuffer_t sbuffer; */

uniform float distortion;
uniform float scattering;

uniform samplerCube ambient_map;

uniform sampler3D perlin_map;

struct camera_t
{
	vec3 pos;
	float exposure;
	mat4 projection;
	mat4 inv_projection;
	mat4 view;
	mat4 model;
};

uniform camera_t camera;

uniform vec3 light_pos;
uniform float light_radius;
uniform vec4 light_color;
uniform samplerCube light_shadow_map;

uniform float has_tex;
uniform float cameraspace_normals;

in vec3 tgspace_light_dir;
in vec3 tgspace_eye_dir;
in vec3 worldspace_position;
in vec3 cameraspace_light_dir;

in vec2 object_id;
in vec2 poly_id;
in vec3 poly_color;

in vec3 vertex_position;

in vec2 texcoord;
in vec4 vertex_color;

in mat4 model;
in mat3 TM;

uniform vec2 screen_size;

vec2 pixel_pos()
{
	return gl_FragCoord.xy / screen_size;
}


vec4 resolveProperty(property_t prop, vec2 coords)
{
	/* vec3 tex; */
	if(prop.texture_blend > 0.001)
	{
		coords *= prop.texture_scale;
		return mix(
			prop.color,
			texture2D(prop.texture, vec2(coords.x, 1.0f-coords.y)),
			/* textureLod(prop.texture, prop.texture_scale * coords, 3), */
			prop.texture_blend
		);
	}
	return prop.color;
}

/* float ambient = 0.08; */
/* float ambient = 1.00; */

float lookup_single(vec3 shadowCoord)
{
	return texture(light_shadow_map, shadowCoord).a;
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
	return resolveProperty(emissive, texcoord);
}

float get_metalness()
{
	return resolveProperty(metalness, texcoord).r;
}

float get_roughness()
{
	return resolveProperty(roughness, texcoord).r;
}


vec4 get_transparency()
{
	return resolveProperty(transparency, texcoord);
}


vec3 get_position(sampler2D depth)
{
	vec2 pos = pixel_pos();
	vec3 raw_pos = vec3(pos, textureLod(depth, pos, 0).r);
	vec4 clip_pos = vec4(raw_pos * 2.0 - 1.0, 1.0);
	vec4 view_pos = camera.inv_projection * clip_pos;
	return view_pos.xyz / view_pos.w;
}

vec3 get_position(sampler2D depth, vec2 pos)
{
	vec3 raw_pos = vec3(pos, textureLod(depth, pos, 0).r);
	vec4 clip_pos = vec4(raw_pos * 2.0 - 1.0, 1.0);
	vec4 view_pos = camera.inv_projection * clip_pos;
	return view_pos.xyz / view_pos.w;
}

vec3 get_normal(sampler2D buffer)
{
	return decode_normal(textureLod(buffer, pixel_pos(), 0).rg);
}
vec3 get_normal()
{
	if(has_tex > 0.5)
	{
		vec3 texcolor = resolveProperty(normal, texcoord).rgb * 2.0f - 1.0f;
		return normalize(TM * texcolor);

	}
	return normalize(TM[2]);
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

float shadow_at_dist_no_tan(vec3 vec, float i)
{
	/* vec3 rnd = (vec3(rand(vec.xy), rand(vec.yz), rand(vec.xz)) - 0.5) * i; */
	/* if(lookup(-vec,  rnd) > 0.5) return 1.0; */
	/* return 0.0; */

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
							dist_to_eye / 385), 2, 16)) * 2;
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


float ambientOcclusion(sampler2D depth, vec3 p, vec3 n, float dist_to_eye)
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

		ao += doAmbientOcclusion(depth, texcoord,coord1 * 0.25, p, n);
		ao += doAmbientOcclusion(depth, texcoord,coord2 * 0.5, p, n);
		ao += doAmbientOcclusion(depth, texcoord,coord1 * 0.75, p, n);
		ao += doAmbientOcclusion(depth, texcoord,coord2, p, n);
	}
	ao /= float(iterations) * 4.0f;
	return clamp(1.0f - ao * 0.7f, 0.0f, 1.0f); 
}


// Consts should help improve performance
const float maxSteps = 20;
const float searchDist = 20;
const float searchDistInv = 1.0f / searchDist;

vec3 get_proj_coord(sampler2D depthmap, vec3 hitCoord) // z = hitCoord.z - depth
{
	vec4 projectedCoord     = camera.projection * vec4(hitCoord, 1.0); \
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

vec4 coneSampleWeightedColor(sampler2D screen, vec2 samplePos, float mipChannel, float gloss)
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

	vec3 w_pos = (camera.model * vec4(pos, 1.0f)).xyz;
	vec3 w_nor = (camera.model * vec4(nor, 0.0f)).xyz;
	vec3 c_pos = camera.pos - w_pos;
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
    float maxMipLevel = float(numMips) - 1.0f;
    float glossMult = gloss;

    // cone-tracing using an isosceles triangle to approximate a cone in screen space
    for(int i = 0; i < 14; ++i)
    {
        float oppositeLength = 2.0f * tan(coneTheta) * adjacentLength;
        float incircleSize = isoscelesTriangleInRadius(oppositeLength, adjacentLength);
        vec2 samplePos = texcoord + adjacentUnit * (adjacentLength - incircleSize);
		float mipChannel = clamp(log2(incircleSize * max(screen_size.x,
						screen_size.y)), 0.0f, maxMipLevel);

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
	vec3 specularColor = mix(f0, base_color.rgb, metallic);

	/* return vec4(specularColor, 1); */
    specularColor *= fresnelSchlick(vec3(0.04f), abs(dot(w_nor, eye_dir))) * CNST_1DIVPI;
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

// vim: set ft=c:
