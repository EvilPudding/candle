#include "common.glsl"

BUFFER {
	sampler2D depth;
	sampler2D nmr;
} gbuffer;

layout (location = 0) out float FragColor;

vec2 hemicircle[] = vec2[](
	vec2(1.0, 0.0),
	vec2(0.92388, 0.38268),
	vec2(0.70711, 0.70711),
	vec2(0.38268, 0.92388),
	vec2(0.0, 1.0),
	vec2(-0.38268, 0.92388),
	vec2(-0.70711, 0.70711),
	vec2(-0.92388, 0.38268)
);

uniform float power;

void main(void)
{
	float ao = 0.0;

	vec2 tc = texcoord;
	vec3 n = decode_normal(textureLod(gbuffer.nmr, tc, 0.0).rg);
	float D = texelFetch(gbuffer.depth, ivec2(gl_FragCoord.xy), 0).r;
	float d0 = linearize(D);

	float ditherValue = ditherPattern[(int(gl_FragCoord.x) % 4) * 4 + (int(gl_FragCoord.y) % 4)];
	/* ditherValue = 0.0; */
	float rad = (0.8 / d0);
	vec2 rnd = normalize(vec2(rand(tc), ditherValue));
	float z = clamp((n.z + 0.5), 0.0, 1.0);

	uint taps = 8u;
	float iterations = 8.0;
	/* uint j = 2u; */
	for (uint j = 0u; j < taps; ++j)
	/* for (float j = 0.0; j < M_PI; j += M_PI / 40.0) */
	{
		float angle1 = -M_PI;
		float angle2 = -M_PI;
		float angle = M_PI * 2.0;

		/* vec2 hemi = vec2(cos(j), sin(j)); */
		/* vec2 offset = reflect(hemi, rnd) * rad; */
		vec2 offset = reflect(hemicircle[j], rnd) * rad;
		vec2 norm = normalize(n.xy);
		float d = (norm.x * offset.x + norm.y * offset.y);
		norm = norm * d;
		offset = offset - norm + norm * (1.0 - (1.0 - z));

		for (float i = 0.0; i < iterations; ++i)
		{
			float c0 = pow((i + ditherValue) / (iterations - 1.0), 2.0) + 0.001;
			vec2 coord1 = offset * c0;

			float d1 = linearize(textureLod(gbuffer.depth, tc + coord1, 0.0).r);
			float d2 = linearize(textureLod(gbuffer.depth, tc - coord1, 0.0).r);
			float c1 = d0 - d1;
			float c2 = d0 - d2;
			if (abs(c1) < 1.0)
				angle1 = atan(c1, c0);
			else if (abs(c1) < 2.0)
				angle1 += (atan(c1, c0) - angle1) * (1.0 - (abs(c1) - 1.0));

			if (abs(c2) < 1.0)
				angle2 = atan(c2, c0);
			else if (abs(c2) < 2.0)
				angle2 += (atan(c2, c0) - angle2) * (1.0 - (abs(c2) - 1.0));

			angle = min(angle, M_PI - angle1 - angle2);

			/* if (i > 0.1) */
			{
				float falloff = (1.0 + max(abs(c1), abs(c2)));
				/* ao += clamp((angle1 + angle2) / falloff, 0.0, 1.0) * 3.0; */
				ao += clamp(sin(angle) / falloff - 0.1, 0.0, 1.0);
			}
		}
	}
	ao /= float(taps) * iterations;
	ao = 1.0 - ao * power;
	FragColor = clamp(ao, 0.0, 1.0); 
}

// vim: set ft=c:
