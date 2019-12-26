#include "axis.h"
#include <utils/drawable.h>
#include "model.h"
#include "node.h"
#include "spatial.h"
#include "light.h"
#include <utils/nk.h>
#include <utils/shader.h>
#include <systems/editmode.h>
#include <systems/window.h>
#include <candle.h>

static mesh_t *g_rot_axis_mesh = NULL;

static void c_axis_init(c_axis_t *self)
{
	if(!g_rot_axis_mesh)
	{
		g_rot_axis_mesh = mesh_torus(0.6f, 0.03f, 32, 3);
		g_rot_axis_mesh->has_texcoords = 0;
	}
}

int c_axis_press(c_axis_t *self, model_button_data *event)
{
	c_mouse_t *mouse;
	if(event->button != SDL_BUTTON_LEFT) return CONTINUE;
	mouse = c_mouse(self);
	c_mouse_visible(mouse, false);
	c_mouse_activate(mouse);
	return STOP;
}

int c_axis_release(c_axis_t *self, mouse_button_data *event)
{
	c_mouse_t *mouse = c_mouse(self);
	if(c_mouse_active(mouse))
	{
		c_mouse_deactivate(mouse);
	}
	return CONTINUE;
}

int c_axis_mouse_move(c_axis_t *self, mouse_move_data *event)
{
	entity_t arrows = c_node(self)->parent;
	entity_t target = c_node(&arrows)->parent;
	if(target)
	{
		entity_t parent = c_node(&target)->parent;
		float amount = -event->sy * 0.01;

		c_spatial_t *sc = c_spatial(&target);

		vec3_t dir = self->dir;
		if(parent)
		{
			c_node_t *nc = c_node(&parent);
			vec3_t world_dir = c_node_dir_to_global(c_node(&target), dir);
			world_dir = vec3_norm(world_dir);
			dir = c_node_dir_to_local(nc, world_dir);
		}
		else
		{
			dir = quat_mul_vec3(sc->rot_quat, dir);
		}
		dir = vec3_scale(dir, amount);

		if(self->type == 0)
		{
			c_spatial_set_pos(sc, vec3_add(dir, sc->pos));
		}
		else if(self->type == 1)
		{
			if(self->dir.x > 0.0f)
			{
				c_spatial_rotate_X(sc, amount);
			}
			else if(self->dir.y > 0.0f)
			{
				c_spatial_rotate_Y(sc, amount);
			}
			else if(self->dir.z > 0.0f)
			{
				c_spatial_rotate_Z(sc, amount);
			}
		}
		else if(self->type == 2)
		{
			c_spatial_set_scale(sc, vec3_add(vec3_scale(self->dir, amount), sc->scale));
		}
	}

	return CONTINUE;
}

int c_axis_created(c_axis_t *self)
{
	entity_signal_same(c_entity(self), sig("mesh_changed"), NULL, NULL);
	return CONTINUE;
}

void ct_axis(ct_t *self)
{
	ct_init(self, "axis", sizeof(c_axis_t));
	ct_set_init(self, (init_cb)c_axis_init);
	ct_dependency(self, ct_node);
	ct_dependency(self, ct_mouse);

	ct_listener(self, ENTITY, 0, sig("entity_created"), c_axis_created);
	ct_listener(self, ENTITY, 0, sig("model_press"), c_axis_press);
	ct_listener(self, ENTITY, 0, sig("mouse_release"), c_axis_release);
	ct_listener(self, ENTITY, 0, sig("mouse_move"), c_axis_mouse_move);
}

c_axis_t *c_axis_new(int type, vecN_t dir)
{
	c_axis_t *self = component_new(ct_axis);

	mat_t *m = mat_new("m", "default");

	mat4f(m, ref("emissive.color"), vec4(_vec3(dir), 0.8f));
	mat4f(m, ref("albedo.color"), vec4(_vec3(dir), 0.8f));
#ifdef MESH4
	if(dir.w)
	{
		mat4f(m, ref("emissive.color"), vec4(1.0f, 0.0f, 0.9f, 0.8f));
		mat4f(m, ref("albedo.color"), vec4(1.0f, 0.0f, 0.9f, 0.8f));
	}
#endif
	/* mat4f(m, ref("albedo.color"), vec4(1.0f, 1.0f, 1.0f, 1.0f)); */
	self->type = type;

	if(type == 0)
	{
		self->dir_mesh = mesh_new();
		mesh_lock(self->dir_mesh);
		mesh_circle(self->dir_mesh, 0.03f, 8, dir);
		mesh_extrude_edges(self->dir_mesh, 1, vecN_(scale)(dir, 0.5), 1.0f, NULL, NULL, NULL);
		mesh_extrude_edges(self->dir_mesh, 1, ZN, 1.70f, NULL, NULL, NULL);
		mesh_extrude_edges(self->dir_mesh, 1, vecN_(scale)(dir, 0.1), 0.01f, NULL, NULL, NULL);
		mesh_unlock(self->dir_mesh);
	}
	else if(type == 1)
	{
		self->dir_mesh = g_rot_axis_mesh;
	}
	else if(type == 2)
	{
		vecN_t point;
		self->dir_mesh = mesh_new();
		mesh_lock(self->dir_mesh);
		point = vecN_(scale)(dir, 0.5);
		mesh_circle(self->dir_mesh, 0.03f, 8, dir);
		mesh_extrude_edges(self->dir_mesh, 1, point, 1.0f, NULL, NULL, NULL);
		mesh_translate(self->dir_mesh, vecN_xyz(point));
		mesh_cube(self->dir_mesh, 0.1, 1.0f);
		mesh_unlock(self->dir_mesh);
	}

	entity_add_component(c_entity(self), c_model_new(self->dir_mesh, m, 0, 0));
	c_model_set_groups(c_model(self), ref("widget"), ~0, ~0, ref("selectable"));
	c_model_set_xray(c_model(self), 1);
	c_model(self)->scale_dist = 0.2f;

	self->dir = vecN_xyz(dir);

	return self;
}

