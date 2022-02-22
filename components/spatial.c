#include "spatial.h"
#include "../utils/nk.h"
#include "../systems/editmode.h"

int c_spatial_menu(c_spatial_t *self, void *ctx);

void c_spatial_init(c_spatial_t *self)
{
	self->scale = vec3(1.0, 1.0, 1.0);
	self->rot = vec3(0.0, 0.0, 0.0);
	self->pos = vec3(0.0, 0.0, 0.0);

	self->model_matrix = mat4();
	self->rot_quat = quat();
}

vec3_t c_spatial_forward(c_spatial_t *self)
{
	return vec3_norm(quat_mul_vec3(self->rot_quat, vec3(1.0f, 0.0f, 0.0f)));
}

vec3_t c_spatial_sideways(c_spatial_t *self)
{
	return vec3_norm(quat_mul_vec3(self->rot_quat, vec3(0.0f, 0.0f, 1.0f)));
}

vec3_t c_spatial_upwards(c_spatial_t *self)
{
	return vec3_norm(quat_mul_vec3(self->rot_quat, vec3(0.0f, 1.0f, 0.0f)));
}

void ct_spatial(ct_t *self)
{
	ct_init(self, "spatial", sizeof(c_spatial_t));

	ct_set_init(self, (init_cb)c_spatial_init);
	ct_add_listener(self, WORLD, 0, sig("component_menu"), c_spatial_menu);
}

c_spatial_t *c_spatial_new()
{
	return component_new(ct_spatial);
}

void c_spatial_unlock(c_spatial_t *self)
{
	self->lock_count--;
	if(self->modified && self->lock_count == 0)
	{
		self->modified = 0;
		c_spatial_update_model_matrix(self);
	}
}

void c_spatial_lock(c_spatial_t *self)
{
	self->lock_count++;
}

void c_spatial_look_at(c_spatial_t *self, vec3_t eye, vec3_t up)
{
	mat4_t rot_matrix;
	c_spatial_lock(self);

	rot_matrix = mat4_look_at(vec3(0,0,0), eye, up);
	rot_matrix = mat4_invert(rot_matrix);
	self->rot_quat = mat4_to_quat(rot_matrix);

	self->update_id++;
	self->modified = 1;
	c_spatial_unlock(self);
}

void c_spatial_set_scale(c_spatial_t *self, vec3_t scale)
{
	c_spatial_lock(self);
	self->scale = scale;
	self->update_id++;
	self->modified = 1;
	c_spatial_unlock(self);
}

void c_spatial_scale(c_spatial_t *self, vec3_t scale)
{
	c_spatial_lock(self);
	self->scale = vec3_mul(self->scale, scale);
	self->update_id++;
	self->modified = 1;
	c_spatial_unlock(self);
}

void c_spatial_rotate_axis(c_spatial_t *self, vec3_t axis, float angle)
{
	vec4_t rot;
	c_spatial_lock(self);

	rot = quat_rotate(axis, angle);

	self->rot_quat = quat_mul(rot, self->rot_quat);

	self->update_id++;
	self->modified = 1;
	c_spatial_unlock(self);
}

void c_spatial_rotate_X(c_spatial_t *self, float angle)
{
	vec4_t rot;

	c_spatial_lock(self);

	rot = quat_rotate(c_spatial_forward(self), angle);

	self->rot_quat = quat_mul(rot, self->rot_quat);

	self->rot.x += angle;

	self->update_id++;
	self->modified = 1;
	c_spatial_unlock(self);
}

void c_spatial_rotate_Z(c_spatial_t *self, float angle)
{
	vec4_t rot;

	c_spatial_lock(self);

	rot = quat_rotate(c_spatial_sideways(self), angle);

	self->rot_quat = quat_mul(rot, self->rot_quat);

	self->rot.z += angle;

	self->update_id++;
	self->modified = 1;
	c_spatial_unlock(self);
}

void c_spatial_rotate_Y(c_spatial_t *self, float angle)
{
	vec4_t rot;

	c_spatial_lock(self);

	rot = quat_rotate(c_spatial_upwards(self), angle);

	self->rot_quat = quat_mul(rot, self->rot_quat);
	self->rot.y += angle;

	self->update_id++;
	self->modified = 1;
	c_spatial_unlock(self);
}

void c_spatial_set_rot(c_spatial_t *self, float x, float y, float z, float angle)
{
	mat4_t rot_matrix;
	float new_x = x * angle;
	float new_y = y * angle;
	float new_z = z * angle;
	c_spatial_lock(self);

	rot_matrix = mat4();
	if (x)
		rot_matrix = mat4_rotate(rot_matrix, x, 0, 0, new_x);
	if (z)
		rot_matrix = mat4_rotate(rot_matrix, 0, 0, z, new_z);
	if (y)
		rot_matrix = mat4_rotate(rot_matrix, 0, y, 0, new_y);

	self->rot_quat = mat4_to_quat(rot_matrix);

	if(x) self->rot.x = new_x;
	if(y) self->rot.y = new_y;
	if(z) self->rot.z = new_z;

	self->update_id++;
	self->modified = 1;
	c_spatial_unlock(self);
}

void c_spatial_assign(c_spatial_t *self, c_spatial_t *other)
{
	c_spatial_lock(self);
	self->pos = other->pos;
	self->rot = other->rot;
	self->scale = other->scale;
	self->rot_quat = other->rot_quat;
	self->model_matrix = other->model_matrix;
	self->modified = 1;
	self->update_id++;

#ifdef MESH4
	self->angle4 = other->angle4;
#endif
	c_spatial_unlock(self);
}

void c_spatial_set_model(c_spatial_t *self, mat4_t m)
{
	vec3_t v0, v1, v2, v3;

	c_spatial_lock(self);
	v0 = vec4_xyz(mat4_mul_vec4(m, vec4(1,0,0,0)));
	v1 = vec4_xyz(mat4_mul_vec4(m, vec4(0,1,0,0)));
	v2 = vec4_xyz(mat4_mul_vec4(m, vec4(0,0,1,0)));
	v3 = vec4_xyz(mat4_mul_vec4(m, vec4(0,0,0,1)));

	self->scale = vec3(vec3_len(v0), vec3_len(v1), vec3_len(v2));
	self->pos = v3;

	m._[0][0] /= self->scale.x;
	m._[1][0] /= self->scale.y;
	m._[2][0] /= self->scale.z;
	m._[3][0] = 0;

	m._[0][1] /= self->scale.x;
	m._[1][1] /= self->scale.y;
	m._[2][1] /= self->scale.z;
	m._[3][1] = 0;

	m._[0][2] /= self->scale.x;
	m._[1][2] /= self->scale.y;
	m._[2][2] /= self->scale.z;
	m._[3][2] = 0;

	self->rot_quat = vec4_norm(mat4_to_quat(m));
	self->rot = quat_to_euler(self->rot_quat);

	/* self->model_matrix = m; */
	self->update_id++;
	self->modified = 1;
	c_spatial_unlock(self);

}

void c_spatial_set_pos(c_spatial_t *self, vec3_t pos)
{
	c_spatial_lock(self);
	self->pos = pos;

	self->modified = 1;
	self->update_id++;
	c_spatial_unlock(self);
}

int c_spatial_menu(c_spatial_t *self, void *ctx)
{
	vec3_t tmp = self->pos;
	if(nk_tree_push(ctx, NK_TREE_NODE, "Position", NK_MINIMIZED))
	{
		/* nk_layout_row_dynamic(ctx, 0, 1); */
		nk_property_float(ctx, "#x:", -10000, &tmp.x, 10000, 0.1f, 0.05f);
		nk_property_float(ctx, "#y:", -10000, &tmp.y, 10000, 0.1f, 0.05f);
		nk_property_float(ctx, "#z:", -10000, &tmp.z, 10000, 0.1f, 0.05f);

		if(!vec3_equals(self->pos, tmp))
		{
			c_spatial_set_pos(self, tmp);
		}
		nk_tree_pop(ctx);
	}

	tmp = self->rot;
	if(nk_tree_push(ctx, NK_TREE_NODE, "Rotation", NK_MINIMIZED))
	{
#ifdef MESH4
		float tmpw = self->angle4;
#endif
		/* nk_layout_row_dynamic(ctx, 0, 1); */
		nk_property_float(ctx, "#x:", -1000, &tmp.x, 1000, 0.1, 0.01);
		nk_property_float(ctx, "#y:", -1000, &tmp.y, 1000, 0.1, 0.01);
		nk_property_float(ctx, "#z:", -1000, &tmp.z, 1000, 0.1, 0.01);
#ifdef MESH4
		nk_property_float(ctx, "#w:", 0, &tmpw, M_PI, 0.1, 0.01);
		if(self->angle4 != tmpw)
		{
			self->angle4 = tmpw;
			entity_signal_same(c_entity(self), sig("spatial_changed"),
					&c_entity(self), NULL);
		}
#endif

		if(self->rot.x != tmp.x)
		{
			c_spatial_rotate_X(self, tmp.x-self->rot.x);
		}
		if(self->rot.y != tmp.y)
		{
			c_spatial_rotate_Y(self, tmp.y-self->rot.y);
		}
		if(self->rot.z != tmp.z)
		{
			c_spatial_rotate_Z(self, tmp.z-self->rot.z);
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
			c_spatial_update_model_matrix(self);
		}
		nk_tree_pop(ctx);
	}

	return CONTINUE;
}


void c_spatial_update_model_matrix(c_spatial_t *self)
{
	mat4_t rot_matrix;
	self->model_matrix = mat4_translate(self->pos);

	rot_matrix = quat_to_mat4(vec4_norm(self->rot_quat));
	self->model_matrix = mat4_mul(self->model_matrix, rot_matrix);

	self->model_matrix = mat4_scale_aniso(self->model_matrix, self->scale);

	entity_signal_same(c_entity(self), sig("spatial_changed"), &c_entity(self), NULL);
}
