#include "node.h"
#include "name.h"
#include "spatial.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void c_node_init(c_node_t *self)
{
	self->children = NULL;
	self->children_size = 0;
	self->model = mat4();
	self->cached = false;
	self->inherit_scale = true;
	self->parent = entity_null;
	self->inherit_transform = true;
}

int32_t c_node_created(c_node_t *self)
{
	if (c_entity(self) != SYS)
	{
		c_node_add(c_node(&SYS), 1, c_entity(self));
	}
	return CONTINUE;
}

extern int g_update_id;
int c_node_changed(c_node_t *self)
{
	uint32_t i;
	entity_signal_same(c_entity(self), sig("node_changed"), NULL, NULL);
	/* if (self->cached) */
	{
		self->cached = false;
		for(i = 0; i < self->children_size; i++)
		{
			c_node_changed(c_node(&self->children[i]));
		}
	}

	return CONTINUE;
}

int32_t c_node_propagate(c_node_t *self, node_cb cb, void *usrptr)
{
	uint32_t i;
	int32_t res = cb(c_entity(self), usrptr);
	if (res != CONTINUE) return res;

	for(i = 0; i < self->children_size; i++)
	{
		c_node_t *child_node;
		int32_t res;
		entity_t child = self->children[i];
		if (!entity_exists(child)) continue;
		child_node = c_node(&child);
		if (!child_node) continue;
		res = c_node_propagate(child_node, cb, usrptr);
		if (res != CONTINUE) return res;
	}
	return CONTINUE;
}

void c_node_pack(c_node_t *self, int unpacked)
{
	self->unpacked = unpacked;
	c_node_changed(self);
}


entity_t c_node_get_by_name(c_node_t *self, uint32_t hash)
{
	uint32_t i;
	for(i = 0; i < self->children_size; i++)
	{
		entity_t child = self->children[i];
		c_name_t *child_name = c_name(&child);
		c_node_t *child_node;

		if (child_name && child_name->hash == hash)
		{
			return child;
		}

		child_node = c_node(&child);
		if (child_node)
		{
			entity_t response = c_node_get_by_name(child_node, hash);
			if (entity_exists(response))
			{
				return response;
			}
		}
	}
	return entity_null;
}

void c_node_unparent(c_node_t *self, int inherit_transform)
{
	if (entity_exists(self->parent))
	{
		c_node_t *parent = c_node(&self->parent);
		if (inherit_transform)
		{
			int prev_inheritance = self->inherit_scale;
			self->inherit_scale = false;
			self->cached = false;
			c_node_update_model(self);

			c_spatial_set_model(c_spatial(self), self->model);
			self->inherit_scale = prev_inheritance;
		}
		c_node_remove(parent, c_entity(self));
	}
}

void c_node_remove(c_node_t *self, entity_t child)
{
	int i;
	for(i = 0; i < self->children_size; i++)
	{
		if (self->children[i] == child)
		{
			c_node_t *cn;
			if (!entity_exists(self->children[i])) continue;
			cn = c_node(&child);
			cn->cached = false;
			cn->parent = entity_null;
			if (--self->children_size)
			{
				self->children[i] = self->children[self->children_size];
			}
		}

	}
}

void c_node_add(c_node_t *self, int num, ...)
{
	va_list children;
	va_start(children, num);
	while(num--)
	{
		int32_t i;
		entity_t child = va_arg(children, entity_t);
		c_node_t *child_node = c_node(&child);
		if (!child_node)
		{
			entity_add_component(child, c_node_new());
			child_node = c_node(&child);
		}
		else if (entity_exists(child_node->parent))
		{
			if (child_node->parent == c_entity(self))
			{
				continue;
			}
			c_node_remove(c_node(&child_node->parent), child);
		}

		i = self->children_size++;
		self->children = realloc(self->children,
				(sizeof *self->children) * self->children_size);
		self->children[i] = child;
		child_node->parent = c_entity(self);

		c_node_changed(child_node);
	}
	va_end(children);
}

static void c_node_destroy(c_node_t *self)
{
	int i;
	c_node_unparent(self, 0);
	for (i = 0; i < self->children_size; i++)
	{
		entity_t child = self->children[i];
		c_node_t *child_node = c_node(&child);
		if (!child_node) continue;
		child_node->parent = entity_null;
		child_node->cached = false;
		self->children[i] = entity_null;
		if (!child_node->ghost)
		{
			entity_destroy(child);
		}
	}
	self->children_size = 0;
	free(self->children);
}

void c_node_disable_inherit_transform(c_node_t *self)
{
	c_spatial_t *sc;
	c_node_t *parent_node = entity_exists(self->parent) ? c_node(&self->parent) : NULL;
	if (!parent_node) return;
	c_node_update_model(parent_node);
	sc = c_spatial(self);
	c_spatial_lock(sc);
	self->inherit_transform = false;
	self->cached = false;

	sc->pos = vec4_xyz(mat4_mul_vec4(parent_node->model, vec4(_vec3(sc->pos), 1.0f)));
	sc->rot_quat = quat_mul(parent_node->rot, sc->rot_quat);

	c_spatial_unlock(sc);
}

bool_t c_node_update_model(c_node_t *self)
{
	c_node_t *parent_node;
	c_spatial_t *sc;
	if (self->cached) return false;
	self->cached = true;

	sc = c_spatial(self);
	if (!sc) return false;

	parent_node = entity_exists(self->parent) ? c_node(&self->parent) : NULL;
	if (parent_node && self->inherit_transform)
	{
		c_node_update_model(parent_node);
		self->ghost_inheritance = self->ghost || parent_node->ghost_inheritance;

		self->unpack_inheritance = (parent_node->unpacked || self->unpacked) ? c_entity(self) :
			parent_node->unpack_inheritance;
		if (self->ghost_inheritance)
		{
			self->unpack_inheritance = c_entity(self);
		}

		self->rot = quat_mul(parent_node->rot, sc->rot_quat);
#ifdef MESH4
		self->angle4 = parent_node->angle4 + sc->angle4;
#endif

		if (self->inherit_scale)
		{
			self->model = mat4_mul(parent_node->model, sc->model_matrix);
		}
		else
		{
			vec3_t pos = vec4_xyz(mat4_mul_vec4(parent_node->model, vec4(_vec3(sc->pos), 1.0f)));

			mat4_t model = mat4_translate(pos);
			model = mat4_mul(model, quat_to_mat4(parent_node->rot));

			self->model = mat4_mul(model, quat_to_mat4(sc->rot_quat));
		}
	}
	else
	{
		self->unpack_inheritance = self->unpacked ? c_entity(self) : entity_null;
		self->ghost_inheritance = self->ghost;
		self->model = sc->model_matrix;
		self->rot = sc->rot_quat;
#ifdef MESH4
		self->angle4 = sc->angle4;
#endif
	}
	return true;
}

vec3_t c_node_pos_to_local(c_node_t *self, vec3_t vec)
{
	mat4_t inv;
	c_node_update_model(self);
	inv = mat4_invert(self->model);
	return vec4_xyz(mat4_mul_vec4(inv, vec4(_vec3(vec), 1.0)));
}

vec3_t c_node_dir_to_local(c_node_t *self, vec3_t vec)
{
	mat4_t inv;
	c_node_update_model(self);
	inv = mat4_invert(self->model);
	return vec4_xyz(mat4_mul_vec4(inv, vec4(_vec3(vec), 0.0)));
}

vec4_t c_node_rot_to_local(c_node_t *self, vec4_t rot)
{
	vec4_t inv;
	c_node_update_model(self);
	inv = quat_invert(self->rot);
	return quat_mul(inv, rot);
}

vec3_t c_node_pos_to_global(c_node_t *self, vec3_t vec)
{
	c_node_update_model(self);
	return vec4_xyz(mat4_mul_vec4(self->model, vec4(_vec3(vec), 1.0)));
}

vec3_t c_node_dir_to_global(c_node_t *self, vec3_t vec)
{
	c_node_update_model(self);
	return vec4_xyz(mat4_mul_vec4(self->model, vec4(_vec3(vec), 0.0)));
}

vec4_t c_node_rot_to_global(c_node_t *self, vec4_t rot)
{
	c_node_update_model(self);
	return quat_mul(self->rot, rot);
}

void ct_node(ct_t *self)
{
	ct_init(self, "node", sizeof(c_node_t));
	ct_set_init(self, (init_cb)c_node_init);
	ct_set_destroy(self, (destroy_cb)c_node_destroy);
	ct_add_dependency(self, ct_spatial);

	ct_add_listener(self, ENTITY, 0, ref("entity_created"), c_node_created);
	ct_add_listener(self, ENTITY, 0, ref("spatial_changed"), c_node_changed);
}

c_node_t *c_node_new()
{
	c_node_t *self = component_new(ct_node);
	return self;
}

