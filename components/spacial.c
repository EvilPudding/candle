#include "../ext.h"
#include "spacial.h"
#include <nk.h>

unsigned long ct_spacial;
unsigned long spacial_changed;

int c_spacial_menu(c_spacial_t *self, void *ctx);

void c_spacial_init(c_spacial_t *self)
{
	self->super = component_new(ct_spacial);

	self->scale = vec3(1.0, 1.0, 1.0);
	self->rot = vec3(0.0, 0.0, 0.0);
	self->pos = vec3(0.0, 0.0, 0.0);

	self->forward = vec3(1.0, 0.0, 0.0);
	self->sideways = vec3(0.0, 0.0, 1.0);
	self->upwards = vec3(0.0, 1.0, 0.0);

	self->model_matrix = mat4();
	self->rot_matrix = mat4();
}

c_spacial_t *c_spacial_new()
{
	c_spacial_t *self = malloc(sizeof *self);
	c_spacial_init(self);

	return self;
}

void c_spacial_register(ecm_t *ecm)
{
	ct_t *ct = ecm_register(ecm, &ct_spacial, sizeof(c_spacial_t),
			(init_cb)c_spacial_init, 0);

	spacial_changed = ecm_register_signal(ecm, sizeof(entity_t));

	ct_register_listener(ct, WORLD, component_menu, (signal_cb)c_spacial_menu);
}

vec3_t c_spacial_up(c_spacial_t *self)
{
	return self->up;
}

void c_spacial_look_at(c_spacial_t *self, vec3_t eye, vec3_t center, vec3_t up)
{
	self->up = up;
	self->model_matrix = mat4_look_at(eye, center, up);
	entity_signal(self->super.entity, spacial_changed,
			&self->super.entity);
}

void c_spacial_scale(c_spacial_t *self, vec3_t scale)
{
	self->scale = vec3_mul(self->scale, scale);
	c_spacial_update_model_matrix(self);
}
void c_spacial_set_rot2(c_spacial_t *self, vec3_t rot)
{
	self->rot_matrix = mat4();

	self->rot_matrix = mat4_rotate(self->rot_matrix, 1, 0, 0,
			rot.x);
	self->rot_matrix = mat4_rotate(self->rot_matrix, 0, 0, 1,
			rot.z);
	self->rot_matrix = mat4_rotate(self->rot_matrix, 0, 1, 0,
			rot.y);

	self->rot = rot;

	c_spacial_update_model_matrix(self);
}

void c_spacial_rotate_X(c_spacial_t *self, float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);

	self->sideways = vec3_rotate(self->sideways, self->forward, c, s);
	self->upwards = vec3_rotate(self->upwards, self->forward, c, s);

	self->rot_matrix = mat4_from_vecs(self->forward, self->upwards, self->sideways);

	self->rot.x += angle;

	c_spacial_update_model_matrix(self);
}

void c_spacial_rotate_Z(c_spacial_t *self, float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);

	self->forward = vec3_rotate(self->forward, self->sideways, c, s);
	self->upwards = vec3_rotate(self->upwards, self->sideways, c, s);

	self->rot_matrix = mat4_from_vecs(self->forward, self->upwards, self->sideways);

	self->rot.z += angle;

	c_spacial_update_model_matrix(self);
}

void c_spacial_rotate_Y(c_spacial_t *self, float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);

	self->sideways = vec3_rotate(self->sideways, self->upwards, c, s);
	self->forward = vec3_rotate(self->forward, self->upwards, c, s);

	self->rot_matrix = mat4_from_vecs(self->forward, self->upwards, self->sideways);
	self->rot.y += angle;

	c_spacial_update_model_matrix(self);
}

void c_spacial_set_rot(c_spacial_t *self, float x, float y, float z, float angle)
{
	float new_x = x * angle;
	float new_y = y * angle;
	float new_z = z * angle;

	self->rot_matrix = mat4_rotate(self->rot_matrix, x, 0, 0,
			new_x - self->rot.x);
	self->rot_matrix = mat4_rotate(self->rot_matrix, 0, 0, z,
			new_z - self->rot.z);
	self->rot_matrix = mat4_rotate(self->rot_matrix, 0, y, 0,
			new_y - self->rot.y);

	if(x) self->rot.x = new_x;
	if(y) self->rot.y = new_y;
	if(z) self->rot.z = new_z;

	c_spacial_update_model_matrix(self);
}

void c_spacial_set_pos(c_spacial_t *self, vec3_t pos)
{
	self->pos = pos;

	c_spacial_update_model_matrix(self);

}

int c_spacial_menu(c_spacial_t *self, void *ctx)
{

	vec3_t tmp;
	if(nk_tree_push(ctx, NK_TREE_TAB, "Spacial", NK_MINIMIZED))
	{
		if(nk_tree_push(ctx, NK_TREE_NODE, "Position", NK_MINIMIZED))
		{
			tmp = self->pos;
			nk_layout_row_dynamic(ctx, 22, 1);
			nk_property_float(ctx, "x:", -10000, &tmp.x, 10000, 0.1, 0.05);
			nk_property_float(ctx, "y:", -10000, &tmp.y, 10000, 0.1, 0.05);
			nk_property_float(ctx, "z:", -10000, &tmp.z, 10000, 0.1, 0.05);

			if(!vec3_equals(self->pos, tmp))
			{
				c_spacial_set_pos(self, tmp);
			}
			nk_tree_pop(ctx);
		}

		if(nk_tree_push(ctx, NK_TREE_NODE, "Rotation", NK_MINIMIZED))
		{
			nk_layout_row_dynamic(ctx, 22, 1);
			tmp = self->rot;
			nk_property_float(ctx, "x:", -1000, &tmp.x, 1000, 0.1, 0.01);
			nk_property_float(ctx, "y:", -1000, &tmp.y, 1000, 0.1, 0.01);
			nk_property_float(ctx, "z:", -1000, &tmp.z, 1000, 0.1, 0.01);

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

		if(nk_tree_push(ctx, NK_TREE_NODE, "Scale", NK_MINIMIZED))
		{
			nk_layout_row_dynamic(ctx, 22, 1);
			tmp = self->scale;
			nk_property_float(ctx, "x:", -1000, &tmp.x, 1000, 0.1, 0.01);
			nk_property_float(ctx, "y:", -1000, &tmp.y, 1000, 0.1, 0.01);
			nk_property_float(ctx, "z:", -1000, &tmp.z, 1000, 0.1, 0.01);

			if(!vec3_equals(self->scale, tmp))
			{
				self->scale = tmp;
				c_spacial_update_model_matrix(self);
			}
			nk_tree_pop(ctx);
		}



		nk_tree_pop(ctx);
	}

	return 1;
}


void c_spacial_update_model_matrix(c_spacial_t *self)
{
	self->model_matrix = mat4_translate(self->pos.x, self->pos.y,
			self->pos.z);
	self->model_matrix = mat4_mul(self->model_matrix, self->rot_matrix);
	self->model_matrix = mat4_scale_aniso(self->model_matrix, self->scale.x,
			self->scale.y, self->scale.z);

	entity_signal(self->super.entity, spacial_changed,
			&self->super.entity);
}
