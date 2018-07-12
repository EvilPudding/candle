#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <candle.h>
#include "physics.h"
#include <components/spacial.h>
#include <components/force.h>
#include <components/velocity.h>
#include <components/rigid_body.h>
#include <components/light.h>
#include <components/node.h>

static vec3_t c_physics_handle_forces(c_physics_t *self, vec3_t vel, float *dt)
{
	khiter_t k;

	ct_t *forces = ecm_get(ref("force"));

	for(k = kh_begin(forces->cs); k != kh_end(forces->cs); ++k)
	{
		if(!kh_exist(forces->cs, k)) continue;
		c_force_t *fc = (c_force_t*)kh_value(forces->cs, k);

		if(fc->active) vel = vec3_add(vel, vec3_scale(fc->force, *dt));
	}

	return vel;
}

vec3_t handle_dirs(c_t *c, collider_cb cb, vec3_t old_pos, vec3_t new_pos)
{
	vec3_t res = vec3(1.0, 1.0, 1.0);
	if(cb(c, vec3(new_pos.x, old_pos.y, new_pos.z)) < 0)
	{
		/* XZ */
		res.y = 0;
	}
	else if(cb(c, vec3(old_pos.x, new_pos.y, new_pos.z)) < 0)
	{
		/* YZ */
		res.x = 0;
	}
	else if(cb(c, vec3(new_pos.x, new_pos.y, old_pos.z)) < 0)
	{
		/* XY */
		res.z = 0;
	}
	else if(cb(c, vec3(new_pos.x, old_pos.y, old_pos.z)) < 0)
	{
		/* X */
		res.y = 0;
		res.z = 0;
	}
	else if(cb(c, vec3(old_pos.x, old_pos.y, new_pos.z)) < 0)
	{
		/* Z */
		res.y = 0;
		res.x = 0;
	}
	else if(cb(c, vec3(old_pos.x, new_pos.y, old_pos.z)) < 0)
	{
		/* Y */
		res.x = 0;
		res.z = 0;
	}
	else
	{
		res.x = 0;
		res.y = 0;
		res.z = 0;
	}
	return res;
}

static float handle_cols_for_offset(c_t *c, c_t *projectile, collider_cb cb,
		vec3_t offset, vec3_t *new_pos, const vec3_t old_pos)
{
	c_node_t *node = c_node(projectile);
	if(node)
	{
		c_node_update_model(node);
		offset = mat4_mul_vec4(node->model,
				vec4(_vec3(offset), 0.0)).xyz;
		/* offset = vec3_sub(offset, old_pos); */
		/* vec3_print(offset); */
	}
	vec3_t o_new_pos = vec3_add(offset, *new_pos);
	vec3_t o_old_pos = vec3_add(offset, old_pos);
	float f = cb(c, o_new_pos);
	if(f >= 0)
	{
		vec3_t delta = handle_dirs(c, cb, o_old_pos, o_new_pos);
		if(!delta.x) new_pos->x = old_pos.x;
		if(!delta.y) new_pos->y = old_pos.y;
		if(!delta.z) new_pos->z = old_pos.z;
		return f;
	}
	return 0.0;
}

static void c_physics_handle_collisions(c_rigid_body_t *c1,
		c_rigid_body_t *c2)
{
	collider_cb cb1 = (collider_cb)c1->costum;
	collider_cb cb2 = (collider_cb)c2->costum;

	c_t *c = NULL;
	c_t *d = NULL;
	collider_cb cb = NULL;
	if(cb1)
	{
		c = (c_t*)c1;
		d = (c_t*)c2;
		cb = cb1;
	}
	else if(cb2)
	{
		c = (c_t*)c2;
		d = (c_t*)c1;
		cb = cb2;
	}

	if(cb)
	{
		const float width = 0.20;
		c_velocity_t *vc = c_velocity(d);
		c_rigid_body_t *rb = c_rigid_body(d);
		/* if(!vc) return; */

		vec3_t offsets[] = {
			vec3( width, 0.0,  width),
			vec3( width, 0.0, -width),
			vec3(-width, 0.0,  width),
			vec3(-width, 0.0, -width),
			vec3( width, rb->offset,  width),
			vec3( width, rb->offset, -width),
			vec3(-width, rb->offset,  width),
			vec3(-width, rb->offset, -width)
		};
		int o;
		float friction = 0.0;
		for(o = 0; o < sizeof(offsets) / sizeof(*offsets); o++)
		{
			friction = fmax(handle_cols_for_offset(c, d, cb, offsets[o],
						&vc->computed_pos, vc->pre_movement_pos), friction);

		}
		if(vc->computed_pos.x == vc->pre_movement_pos.x) vc->velocity.x = 0;
		if(vc->computed_pos.y == vc->pre_movement_pos.y) vc->velocity.y = 0;
		if(vc->computed_pos.z == vc->pre_movement_pos.z) vc->velocity.z = 0;
		/* if(friction) */
		/* { */
		/*	 vec3_t dec = vec3_scale(*new_vel, friction); */
		/*	 *new_vel = vec3_sub(*new_vel, vec3_scale(dec, *dt)); */
		/* } */
	}
	else
	{
		contact_t contact;
		if(c_rigid_body_intersects(c1, c2, &contact))
		{
			c_velocity_t *v;
			if((v = c_velocity(c1)))
			{
				v->velocity = vec3(0.0);
				v->computed_pos = v->pre_movement_pos;
			}
			if((v = c_velocity(c2)))
			{
				v->velocity = vec3(0.0);
				v->computed_pos = v->pre_movement_pos;
			}
		}
	}
}

static int c_physics_update(c_physics_t *self, float *dt)
{
	khiter_t k, k2;

	ct_t *vels = ecm_get(ref("velocity"));
	ct_t *bodies = ecm_get(ref("rigid_body"));

	for(k = kh_begin(vels->cs); k != kh_end(vels->cs); ++k)
	{
		if(!kh_exist(vels->cs, k)) continue;
		c_velocity_t *vc = (c_velocity_t*)kh_value(vels->cs, k);

		if(!entity_exists(c_entity(vc))) continue;

		c_spacial_t *sc = c_spacial(vc);

		vc->pre_movement_pos = sc->pos;
		vc->velocity = c_physics_handle_forces(self, vc->velocity, dt);

		vc->pre_collision_pos = vc->computed_pos =
			vec3_add(sc->pos, vec3_scale(vc->velocity, *dt));
	}

	for(k = kh_begin(bodies->cs); k != kh_end(bodies->cs); ++k)
	{
		if(!kh_exist(bodies->cs, k)) continue;
		c_rigid_body_t *c1 = (c_rigid_body_t*)kh_value(bodies->cs, k);
		if(!entity_exists(c_entity(c1))) continue;
		c_velocity_t *v1 = c_velocity(c1);

		for(k2 = k + 1; k2 != kh_end(bodies->cs); ++k2)
		{
			if(!kh_exist(bodies->cs, k2)) continue;
			c_rigid_body_t *c2 = (c_rigid_body_t*)kh_value(bodies->cs, k2);

			if(!entity_exists(c_entity(c2))) continue;
			c_velocity_t *v2 = c_velocity(c2);

			if(!v1 && !v2) continue;

			c_physics_handle_collisions(c1, c2);
		}
	}

	for(k = kh_begin(vels->cs); k != kh_end(vels->cs); ++k)
	{
		if(!kh_exist(vels->cs, k)) continue;
		c_velocity_t *vc = (c_velocity_t*)kh_value(vels->cs, k);

		c_spacial_t *sc = c_spacial(vc);

		vc->normal = vec3_sub(vc->computed_pos, vc->pre_collision_pos);
		if(vc->normal.x != vc->normal.x) c_velocity_set_normal(vc, vec3(0.0f));

		c_spacial_set_pos(sc, vc->computed_pos);
	}

	return CONTINUE;
}

c_physics_t *c_physics_new()
{
	c_physics_t *self = component_new("physics");

	return self;
}

REG()
{
	ct_t *ct = ct_new("physics", sizeof(c_physics_t), NULL, NULL, 0);

	ct_listener(ct, WORLD, sig("world_update"), c_physics_update);

	signal_init(sig("collider_callback"), 0);
}

