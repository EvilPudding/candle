#include "spacial.h"
#include <utils/nk.h>
#include <systems/editmode.h>

int c_spacial_menu(c_spacial_t *self, void *ctx);

void c_spacial_init(c_spacial_t *self)
{
	self->scale = vec3(1.0, 1.0, 1.0);
	self->rot = vec3(0.0, 0.0, 0.0);
	self->pos = vec3(0.0, 0.0, 0.0);

	self->forward = vec3(1.0, 0.0, 0.0);
	self->sideways = vec3(0.0, 0.0, 1.0);
	self->upwards = vec3(0.0, 1.0, 0.0);

	self->model_matrix = mat4();
	self->rot_matrix = mat4();
	self->rot_quat = quat();
}

c_spacial_t *c_spacial_new()
{
	c_spacial_t *self = component_new("spacial");

	return self;
}

REG()
{
	ct_t *ct = ct_new("spacial", sizeof(c_spacial_t), c_spacial_init, NULL, 0);

	signal_init(sig("spacial_changed"), sizeof(entity_t));

	ct_listener(ct, WORLD, sig("component_menu"), c_spacial_menu);
}

void c_spacial_unlock(c_spacial_t *self)
{
	self->lock_count--;
	if(self->modified && self->lock_count == 0)
	{
		self->modified = 0;
		c_spacial_update_model_matrix(self);
		entity_signal(self->super.entity, sig("spacial_changed"),
				&self->super.entity);
	}
}

void c_spacial_lock(c_spacial_t *self)
{
	self->lock_count++;
}

vec3_t c_spacial_up(c_spacial_t *self)
{
	return self->up;
}

void c_spacial_look_at(c_spacial_t *self, vec3_t eye, vec3_t center, vec3_t up)
{
	c_spacial_lock(self);
	self->up = up;
	self->pos = eye;

	self->rot_matrix = mat4_look_at(vec3(0,0,0), vec3_sub(center, eye), up);
	self->rot_quat = quat_from_mat4(self->rot_matrix);

	self->modified = 1;
	c_spacial_unlock(self);
}

void c_spacial_set_scale(c_spacial_t *self, vec3_t scale)
{
	self->scale = scale;
	c_spacial_update_model_matrix(self);
}

void c_spacial_scale(c_spacial_t *self, vec3_t scale)
{
	self->scale = vec3_mul(self->scale, scale);
	c_spacial_update_model_matrix(self);
}

void c_spacial_rotate_axis(c_spacial_t *self, vec3_t axis, float angle)
{
	c_spacial_lock(self);
	/* float s = sinf(angle); */
	/* float c = cosf(angle); */

	vec4_t rot = quat_rotate(axis, angle);

	self->sideways = quat_mul_vec3(rot, self->sideways);
	self->upwards = quat_mul_vec3(rot, self->upwards);
	self->forward = quat_mul_vec3(rot, self->forward);

	/* self->sideways = vec3_unit(vec3_rotate(self->sideways, axis, c, s)); */
	/* self->upwards = vec3_unit(vec3_rotate(self->upwards, axis, c, s)); */
	/* self->forward = vec3_unit(vec3_rotate(self->forward, axis, c, s)); */

	self->rot_matrix = mat4_from_vecs(self->forward, self->upwards, self->sideways);
	/* self->rot_quat = quat_mul(self->rot_quat, rot); */
	self->rot_quat = quat_from_mat4(self->rot_matrix);

	self->modified = 1;
	c_spacial_unlock(self);
}

void c_spacial_rotate_X(c_spacial_t *self, float angle)
{
	c_spacial_lock(self);

	vec4_t rot = quat_rotate(self->forward, angle);
	self->sideways = quat_mul_vec3(rot, self->sideways);
	self->upwards = quat_mul_vec3(rot, self->upwards);

	self->rot_matrix = mat4_from_vecs(self->forward, self->upwards, self->sideways);
	self->rot_quat = quat_from_mat4(self->rot_matrix);

	self->rot.x += angle;

	self->modified = 1;
	c_spacial_unlock(self);
}

void c_spacial_rotate_Z(c_spacial_t *self, float angle)
{
	c_spacial_lock(self);

	vec4_t rot = quat_rotate(self->sideways, angle);
	self->forward = quat_mul_vec3(rot, self->forward);
	self->upwards = quat_mul_vec3(rot, self->upwards);

	self->rot_matrix = mat4_from_vecs(self->forward, self->upwards, self->sideways);
	self->rot_quat = quat_from_mat4(self->rot_matrix);

	self->rot.z += angle;

	self->modified = 1;
	c_spacial_unlock(self);
}

void c_spacial_rotate_Y(c_spacial_t *self, float angle)
{
	c_spacial_lock(self);


	vec4_t rot = quat_rotate(self->upwards, angle);
	self->sideways = quat_mul_vec3(rot, self->sideways);
	self->forward = quat_mul_vec3(rot, self->forward);

	self->rot_matrix = mat4_from_vecs(self->forward, self->upwards, self->sideways);
	self->rot_quat = quat_from_mat4(self->rot_matrix);
	self->rot.y += angle;

	self->modified = 1;
	c_spacial_unlock(self);
}

void c_spacial_set_rot(c_spacial_t *self, float x, float y, float z, float angle)
{
	float new_x = x * angle;
	float new_y = y * angle;
	float new_z = z * angle;
	c_spacial_lock(self);

	self->rot_matrix = mat4_rotate(self->rot_matrix, x, 0, 0,
			new_x - self->rot.x);
	self->rot_matrix = mat4_rotate(self->rot_matrix, 0, 0, z,
			new_z - self->rot.z);
	self->rot_matrix = mat4_rotate(self->rot_matrix, 0, y, 0,
			new_y - self->rot.y);
	self->rot_quat = quat_from_mat4(self->rot_matrix);

	if(x) self->rot.x = new_x;
	if(y) self->rot.y = new_y;
	if(z) self->rot.z = new_z;

	self->modified = 1;
	c_spacial_unlock(self);
}

void c_spacial_set_model(c_spacial_t *self, mat4_t m)
{
	c_spacial_lock(self);
	vec3_t v0 = mat4_mul_vec4(m, vec4(1,0,0,0)).xyz;
	vec3_t v1 = mat4_mul_vec4(m, vec4(0,1,0,0)).xyz;
	vec3_t v2 = mat4_mul_vec4(m, vec4(0,0,1,0)).xyz;
	vec3_t v3 = mat4_mul_vec4(m, vec4(0,0,0,1)).xyz;

	self->scale = vec3(vec3_len(v0), vec3_len(v1), vec3_len(v2));
	self->pos = v3;

	m._[0]._[0] /= self->scale.x;
	m._[1]._[0] /= self->scale.y;
	m._[2]._[0] /= self->scale.z;
	m._[3]._[0] = 0;

	m._[0]._[1] /= self->scale.x;
	m._[1]._[1] /= self->scale.y;
	m._[2]._[1] /= self->scale.z;
	m._[3]._[1] = 0;

	m._[0]._[2] /= self->scale.x;
	m._[1]._[2] /= self->scale.y;
	m._[2]._[2] /= self->scale.z;
	m._[3]._[2] = 0;

	self->rot_quat = quat_from_mat4(m);
	self->rot = quat_to_euler(self->rot_quat);

	self->rot_matrix = m;

	self->upwards = mat4_mul_vec4(m, vec4(0, 1, 0, 0)).xyz;
	self->forward = mat4_mul_vec4(m, vec4(1, 0, 0, 0)).xyz;
	self->sideways = mat4_mul_vec4(m, vec4(0, 0, 1, 0)).xyz;

	/* self->model_matrix = m; */
	self->modified = 1;
	c_spacial_unlock(self);

}

void c_spacial_set_pos(c_spacial_t *self, vec3_t pos)
{
	c_spacial_lock(self);
	self->pos = pos;

	self->modified = 1;
	c_spacial_unlock(self);

}

int c_spacial_menu(c_spacial_t *self, void *ctx)
{
	vec3_t tmp = self->pos;
	if(nk_tree_push(ctx, NK_TREE_NODE, "Position", NK_MINIMIZED))
	{
		/* nk_layout_row_dynamic(ctx, 0, 1); */
		nk_property_float(ctx, "#x:", -10000, &tmp.x, 10000, 0.1, 0.05);
		nk_property_float(ctx, "#y:", -10000, &tmp.y, 10000, 0.1, 0.05);
		nk_property_float(ctx, "#z:", -10000, &tmp.z, 10000, 0.1, 0.05);

		if(!vec3_equals(self->pos, tmp))
		{
			c_spacial_set_pos(self, tmp);
		}
		nk_tree_pop(ctx);
	}

	tmp = self->rot;
	if(nk_tree_push(ctx, NK_TREE_NODE, "Rotation", NK_MINIMIZED))
	{
		/* nk_layout_row_dynamic(ctx, 0, 1); */
		nk_property_float(ctx, "#x:", -1000, &tmp.x, 1000, 0.1, 0.01);
		nk_property_float(ctx, "#y:", -1000, &tmp.y, 1000, 0.1, 0.01);
		nk_property_float(ctx, "#z:", -1000, &tmp.z, 1000, 0.1, 0.01);
#ifdef MESH4
		float tmpw = self->angle4;
		nk_property_float(ctx, "#w:", 0, &tmpw, M_PI, 0.1, 0.01);
		if(self->angle4 != tmpw)
		{
			self->angle4 = tmpw;
			entity_signal(c_entity(self), sig("spacial_changed"),
					&c_entity(self));
		}
#endif

		if(self->rot.x != tmp.x)
		{
			c_spacial_rotate_X(self, tmp.x-self->rot.x);
		}
		if(self->rot.y != tmp.y)
		{
			c_spacial_rotate_Y(self, tmp.y-self->rot.y);
		}
		if(self->rot.z != tmp.z)
		{
			c_spacial_rotate_Z(self, tmp.z-self->rot.z);
		}
		nk_tree_pop(ctx);
	}

	tmp = self->scale;
	if(nk_tree_push(ctx, NK_TREE_NODE, "Scale", NK_MINIMIZED))
	{
		/* nk_layout_row_dynamic(ctx, 0, 1); */
		nk_property_float(ctx, "#x:", -1000, &tmp.x, 1000, 0.1, 0.01);
		nk_property_float(ctx, "#y:", -1000, &tmp.y, 1000, 0.1, 0.01);
		nk_property_float(ctx, "#z:", -1000, &tmp.z, 1000, 0.1, 0.01);

		if(!vec3_equals(self->scale, tmp))
		{
			self->scale = tmp;
			c_spacial_update_model_matrix(self);
		}
		nk_tree_pop(ctx);
	}

	return CONTINUE;
}


void c_spacial_update_model_matrix(c_spacial_t *self)
{
	self->model_matrix = mat4_translate(self->pos);

	/* self->rot_quat = quat_from_mat4(self->rot_matrix); */
	/* self->rot_matrix = mat4_from_quat(self->rot_quat); */
	self->model_matrix = mat4_mul(self->model_matrix, self->rot_matrix);
	/* self->model_matrix = mat4_mul_quat(self->model_matrix, self->rot_quat); */

	self->model_matrix = mat4_scale_aniso(self->model_matrix, self->scale);

	entity_signal(c_entity(self), sig("spacial_changed"), &c_entity(self));
}
