#include "timeline.h"
#include "../candle.h"
#include "../systems/keyboard.h"
#include "../components/spatial.h"
#include "../components/node.h"
#include <math.h>

struct frame_vec
{
	float t;
	vec3_t vec;
};

struct frame_quat
{
	float t;
	vec4_t quat;
};

struct frame_script
{
	float t;
};

void c_timeline_clear(c_timeline_t *self)
{
	vector_clear(self->keys_scale);
	vector_clear(self->keys_pos);
	vector_clear(self->keys_rot);
	vector_clear(self->keys_scripts);
}

void c_timeline_insert_scale(c_timeline_t *self,
		vec3_t scale, float time)
{
	struct frame_vec frame;
	frame.vec = scale;
	frame.t = time;
	vector_add(self->keys_scale, &frame);
}

void c_timeline_insert_pos(c_timeline_t *self,
		vec3_t pos, float time)
{
	struct frame_vec frame;
	frame.vec = pos;
	frame.t = time;
	vector_add(self->keys_pos, &frame);
}

void c_timeline_insert_rot(c_timeline_t *self,
		vec4_t rot, float time)
{
	struct frame_quat frame;
	frame.quat = rot;
	frame.t = time;
	vector_add(self->keys_rot, &frame);
}

static int c_timeline_find(c_timeline_t *self, vector_t *vec)
{
	int i;
	for(i = 1; i < vector_count(vec); i++)
	{
		float *t = vector_get(vec, i);
		if(self->t < *t)
		{
			return i-1;
		}
	}
	printf("Couldn't find timeline point\n");
	exit(1);
	return 0;
}

static void c_timeline_update_rot(c_timeline_t *self)
{
	float factor;
	int start_key;
	c_spatial_t *sc = c_spatial(self);
	struct frame_quat *start = NULL;
	struct frame_quat *end = NULL;
	int keys_num = vector_count(self->keys_rot);

	if(keys_num == 0) return;
	if(keys_num == 1)
	{
		start = vector_get(self->keys_rot, 0);
		sc->rot_quat = start->quat;
		sc->modified = 1;
		return;
	}
	start_key = c_timeline_find(self, self->keys_rot);
	start = vector_get(self->keys_rot, start_key);
	end = vector_get(self->keys_rot, start_key + 1);

	factor = (self->t - start->t) / (end->t - start->t);

	sc->rot_quat = quat_clerp(start->quat, end->quat, factor);
	sc->modified = 1;
	/* c_spatial_set_rot */ 
}


static void c_timeline_update_scale(c_timeline_t *self)
{
	int start_key;
	float factor;
	c_spatial_t *sc = c_spatial(self);
	struct frame_vec *start = NULL;
	struct frame_vec *end = NULL;
	int keys_num = vector_count(self->keys_scale);

	if(keys_num == 0) return;
	if(keys_num == 1)
	{
		start = vector_get(self->keys_scale, 0);
		c_spatial_set_scale(sc, start->vec);
		return;
	}
	start_key = c_timeline_find(self, self->keys_scale);
	start = vector_get(self->keys_scale, start_key);
	end = vector_get(self->keys_scale, start_key + 1);

	factor = (self->t - start->t) / (end->t - start->t);

	c_spatial_set_scale(sc, vec3_mix(start->vec, end->vec, factor));
}

static void c_timeline_update_pos(c_timeline_t *self)
{
	int start_key;
	float factor;
	c_spatial_t *sc = c_spatial(self);
	struct frame_vec *start = NULL;
	struct frame_vec *end = NULL;
	int keys_num = vector_count(self->keys_pos);

	if(keys_num == 0) return;
	if(keys_num == 1)
	{
		start = vector_get(self->keys_pos, 0);
		c_spatial_set_pos(sc, start->vec);
		return;
	}
	start_key = c_timeline_find(self, self->keys_pos);
	start = vector_get(self->keys_pos, start_key);
	end = vector_get(self->keys_pos, start_key + 1);

	factor = (self->t - start->t) / (end->t - start->t);

	c_spatial_set_pos(sc, vec3_mix(start->vec, end->vec, factor));
}

static int c_timeline_update(c_timeline_t *self, float *dt)
{
	if(self->playing)
	{
		c_spatial_t *sc;
		self->global_time += *dt;
		self->t = fmod(self->global_time * self->ticks_per_sec, self->duration);
		sc = c_spatial(self);
		c_spatial_lock(sc);
		c_timeline_update_scale(self);
		c_timeline_update_pos(self);
		c_timeline_update_rot(self);
		/* c_timeline_update_scripts(self); */
		c_spatial_unlock(sc);
	}
	return CONTINUE;
}

void ct_timeline(ct_t *self)
{
	ct_init(self, "timeline", sizeof(c_timeline_t));
	ct_add_dependency(self, ct_node);
	ct_add_listener(self, WORLD, 0, sig("world_update"), c_timeline_update);
}

c_timeline_t *c_timeline_new()
{
	c_timeline_t *self = component_new(ct_timeline);
	self->playing = 1;

	self->keys_pos = vector_new(sizeof(struct frame_vec),
			FIXED_ORDER, NULL, NULL);
	self->keys_scale = vector_new(sizeof(struct frame_vec),
			FIXED_ORDER, NULL, NULL);
	self->keys_rot = vector_new(sizeof(struct frame_quat),
			FIXED_ORDER, NULL, NULL);
	self->keys_scripts = vector_new(sizeof(struct frame_script),
			FIXED_ORDER, NULL, NULL);

	return self;
}

