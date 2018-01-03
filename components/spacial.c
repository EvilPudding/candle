#include "../ext.h"
#include "spacial.h"

unsigned long ct_spacial;
unsigned long spacial_changed;

void c_spacial_init(c_spacial_t *self)
{
	self->super = component_new(ct_spacial);

	self->scale = vec3(1.0, 1.0, 1.0);
	self->rotation = vec3(0.0, 0.0, 0.0);
	self->position = vec3(0.0, 0.0, 0.0);

	mat4_identity(self->model_matrix);
	mat4_identity(self->rotation_matrix);
}

c_spacial_t *c_spacial_new()
{
	c_spacial_t *self = malloc(sizeof *self);
	c_spacial_init(self);

	return self;
}

void c_spacial_register(ecm_t *ecm)
{
	ecm_register(ecm, &ct_spacial, sizeof(c_spacial_t), (init_cb)c_spacial_init, 0);
	spacial_changed = ecm_register_signal(ecm, sizeof(entity_t));
}

vec3_t c_spacial_up(c_spacial_t *self)
{
	return self->up;
}

void c_spacial_look_at(c_spacial_t *self, vec3_t eye, vec3_t center, vec3_t up)
{
	self->up = up;
	mat4_look_at(self->model_matrix, eye, center, up);
	entity_signal(self->super.entity, spacial_changed,
			&self->super.entity);
}

void c_spacial_scale(c_spacial_t *self, vec3_t scale)
{
	self->scale = vec3_mul(self->scale, scale);
	c_spacial_update_model_matrix(self);

	entity_signal(self->super.entity, spacial_changed,
			&self->super.entity);
}

void c_spacial_set_rot(c_spacial_t *self, float x, float y, float z, float angle)
{
	float new_x = x * angle;
	float new_y = y * angle;
	float new_z = z * angle;

	mat4_rotate(self->rotation_matrix, self->rotation_matrix, x, 0, 0,
			new_x - self->rotation.x);
	mat4_rotate(self->rotation_matrix, self->rotation_matrix, 0, 0, z,
			new_z - self->rotation.z);
	mat4_rotate(self->rotation_matrix, self->rotation_matrix, 0, y, 0,
			new_y - self->rotation.y);

	if(x) self->rotation.x = new_x;
	if(y) self->rotation.y = new_y;
	if(z) self->rotation.z = new_z;

	c_spacial_update_model_matrix(self);

	entity_signal(self->super.entity, spacial_changed,
			&self->super.entity);
}

void c_spacial_set_pos(c_spacial_t *self, vec3_t pos)
{
	self->position = pos;

	c_spacial_update_model_matrix(self);

	entity_signal(self->super.entity, spacial_changed,
			&self->super.entity);
}

void c_spacial_update_model_matrix(c_spacial_t *self)
{
	mat4_translate(self->model_matrix, self->position.x, self->position.y,
			self->position.z);
	mat4_scale_aniso(self->model_matrix, self->model_matrix, self->scale.x,
			self->scale.y, self->scale.z);
	mat4_mul(self->model_matrix, self->model_matrix, self->rotation_matrix);
}
