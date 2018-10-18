#include "axis.h"
#include <utils/drawable.h>
#include "model.h"
#include "node.h"
#include "spacial.h"
#include "light.h"
#include <utils/nk.h>
#include <systems/renderer.h>
#include <utils/shader.h>
#include <systems/editmode.h>
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

c_axis_t *c_axis_new(int type, vecN_t dir)
{
	c_axis_t *self = component_new("axis");

	mat_t *m = mat_new("m");

	m->emissive.color = vec4(_vec3(dir), 0.8f);
#ifdef MESH4
	if(dir.w)
	{
		m->emissive.color = vec4(1.0f, 0.0f, 0.9f, 0.8f);
	}
#endif
	m->albedo.color = vec4(1, 1, 1, 1.0f);
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
		self->dir_mesh = mesh_new();
		mesh_lock(self->dir_mesh);
		vecN_t point = vecN_(scale)(dir, 0.5);
		mesh_circle(self->dir_mesh, 0.03f, 8, dir);
		mesh_extrude_edges(self->dir_mesh, 1, point, 1.0f, NULL, NULL, NULL);
		mesh_translate(self->dir_mesh, XYZ(point));
		mesh_cube(self->dir_mesh, 0.1, 1.0f);
		mesh_unlock(self->dir_mesh);
	}

	entity_add_component(c_entity(self),
			c_model_new(self->dir_mesh, m, 0, !self->type));
	c_model(self)->xray = 1;
	c_model(self)->scale_dist = 0.2f;

	self->dir = XYZ(dir);

	return self;
}

int c_axis_press(c_axis_t *self, model_button_data *event)
{
	if(event->button != SDL_BUTTON_LEFT) return CONTINUE;
	self->pressing = 1;
	candle_grab_mouse(c_entity(self), 0);
	return STOP;
}

int c_axis_release(c_axis_t *self, mouse_button_data *event)
{
	if(self->pressing)
	{
		candle_release_mouse(c_entity(self), 0);
	}
	self->pressing = 0;
	return CONTINUE;
}

int c_axis_mouse_move(c_axis_t *self, mouse_move_data *event)
{
	if(self->pressing)
	{
		entity_t arrows = c_node(self)->parent;
		entity_t target = c_node(&arrows)->parent;
		if(target)
		{
			entity_t parent = c_node(&target)->parent;
			float amount = -event->sy * 0.04;

			c_spacial_t *sc = c_spacial(&target);

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
				c_spacial_set_pos(sc, vec3_add(dir, sc->pos));
			}
			else if(self->type == 1)
			{
				if(self->dir.x > 0.0f)
				{
					c_spacial_rotate_X(sc, amount);
				}
				else if(self->dir.y > 0.0f)
				{
					c_spacial_rotate_Y(sc, amount);
				}
				else if(self->dir.z > 0.0f)
				{
					c_spacial_rotate_Z(sc, amount);
				}
			}
			else if(self->type == 2)
			{
				c_spacial_set_scale(sc, vec3_add(vec3_scale(self->dir, amount), sc->scale));
			}
		}
	}

	return CONTINUE;
}

int c_axis_created(c_axis_t *self)
{
	entity_signal_same(c_entity(self), sig("mesh_changed"), NULL, NULL);
	return CONTINUE;
}

REG()
{
	ct_t *ct = ct_new("axis", sizeof(c_axis_t),
			c_axis_init, NULL, 1, ref("node"));

	ct_listener(ct, ENTITY, sig("entity_created"), c_axis_created);

	ct_listener(ct, ENTITY, sig("model_press"), c_axis_press);

	ct_listener(ct, WORLD, sig("mouse_release"), c_axis_release);

	ct_listener(ct, WORLD, sig("mouse_move"), c_axis_mouse_move);
}
