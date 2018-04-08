#include "aabb.h"
#include "model.h"
#include "spacial.h"
#include "node.h"


static void c_aabb_init(c_aabb_t *self)
{
	self->min = vec3(1, 1, 1);
	self->max = vec3(-1, -1, -1);
	self->rot = vec3(-1000.0, -1000.0, -1000.0);
	self->sca = vec3(-1000.0, -1000.0, -1000.0);
	self->ready = 0;
}

c_aabb_t *c_aabb_new()
{
	c_aabb_t *self = component_new("aabb");

	return self;
}

void c_aabb_update(c_aabb_t *self)
{
	mat4_t inv;

	c_model_t *mc = c_model(self);

	c_spacial_t *sc = c_spacial(self);
	if(vec3_equals(sc->rot, self->rot) &&
			vec3_equals(sc->scale, self->sca)) return;
	/* c_node_update_model(nc); */

	mesh_t *mesh = mc->mesh;

	if(!mesh) return;

	inv = mat4_invert(sc->rot_matrix);

	vec3_t dir_x = mat4_mul_vec4(inv, vec4(1.0, 0.0, 0.0, 0.0)).xyz;
	vec3_t dir_y = mat4_mul_vec4(inv, vec4(0.0, 1.0, 0.0, 0.0)).xyz;
	vec3_t dir_z = mat4_mul_vec4(inv, vec4(0.0, 0.0, 1.0, 0.0)).xyz;

	vec3_t max_x = XYZ(mesh_farthest(mesh,          dir_x )->pos);
	vec3_t min_x = XYZ(mesh_farthest(mesh, vec3_inv(dir_x))->pos);

	vec3_t max_y = XYZ(mesh_farthest(mesh,          dir_y )->pos);
	vec3_t min_y = XYZ(mesh_farthest(mesh, vec3_inv(dir_y))->pos);

	vec3_t max_z = XYZ(mesh_farthest(mesh,          dir_z )->pos);
	vec3_t min_z = XYZ(mesh_farthest(mesh, vec3_inv(dir_z))->pos);

	self->max.x = mat4_mul_vec4(sc->model_matrix, vec4(_vec3(max_x), 0.0)).x;
	self->max.y = mat4_mul_vec4(sc->model_matrix, vec4(_vec3(max_y), 0.0)).y;
	self->max.z = mat4_mul_vec4(sc->model_matrix, vec4(_vec3(max_z), 0.0)).z;
	self->min.x = mat4_mul_vec4(sc->model_matrix, vec4(_vec3(min_x), 0.0)).x;
	self->min.y = mat4_mul_vec4(sc->model_matrix, vec4(_vec3(min_y), 0.0)).y;
	self->min.z = mat4_mul_vec4(sc->model_matrix, vec4(_vec3(min_z), 0.0)).z;

	self->max = vec3_add_number(self->max, mesh_get_margin(mesh));
	self->min = vec3_sub_number(self->min, mesh_get_margin(mesh));

	self->rot = sc->rot;
	self->sca = sc->scale;
}

int c_aabb_on_mesh_change(c_aabb_t *self)
{
	c_aabb_update(self);
	return CONTINUE;
}

int c_aabb_spacial_changed(c_aabb_t *self)
{
	c_aabb_update(self);
	return CONTINUE;
}

int c_aabb_intersects(c_aabb_t *self, c_aabb_t *other)
{
	c_spacial_t *sc1 = c_spacial(self);
	c_spacial_t *sc2 = c_spacial(other);

	vec3_t min1 = vec3_add(self->min, sc1->pos);
	vec3_t max1 = vec3_add(self->max, sc1->pos);
	vec3_t min2 = vec3_add(other->min, sc2->pos);
	vec3_t max2 = vec3_add(other->max, sc2->pos);
	return
		(min1.x <= max2.x && max1.x >= min2.x) &&
		(min1.y <= max2.y && max1.y >= min2.y) &&
		(min1.z <= max2.z && max1.z >= min2.z);
}

REG()
{
	ct_t *ct = ct_new("aabb", sizeof(c_aabb_t),
			c_aabb_init, NULL, 1, ref("spacial"));

	ct_listener(ct, WORLD, sig("mesh_changed"), c_aabb_on_mesh_change);
	ct_listener(ct, ENTITY, sig("spacial_changed"), c_aabb_spacial_changed);

	/* ct_listener(ct, WORLD, collider_callback, c_grid_collider); */
}
