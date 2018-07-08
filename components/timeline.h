#ifndef TIMELINE_H
#define TIMELINE_H

#include <ecm.h>

typedef struct
{
	c_t super; /* extends c_t */

	vector_t *keys_pos;
	vector_t *keys_scale;
	vector_t *keys_rot;
	vector_t *keys_scripts;

	float duration;
	float ticks_per_sec;

	float global_time;
	float t;
	int playing;
} c_timeline_t;

DEF_CASTER("timeline", c_timeline, c_timeline_t)

c_timeline_t *c_timeline_new(void);

void c_timeline_clear(c_timeline_t *self);

void c_timeline_insert_scale(c_timeline_t *self,
		vec3_t scale, float time);
void c_timeline_insert_pos(c_timeline_t *self,
		vec3_t pos, float time);
void c_timeline_insert_rot(c_timeline_t *self,
		vec4_t rot, float time);

#endif /* !TIMELINE_H */
