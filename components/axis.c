#include "axis.h"
#include "mesh_gl.h"
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

c_axis_t *c_axis_new(int type, vec4_t dir)
{
	c_axis_t *self = component_new("axis");

	mat_t *m = mat_new("m");
	if(dir.w)
	{
		m->emissive.color = vec4(1.0f, 0.0f, 0.9f, 0.5f);
	}
	else
	{
		m->emissive.color = vec4(_vec3(dir.xyz), 0.5f);
	}
	self->type = type;

	self->dir_mesh = mesh_new();

	mesh_lock(self->dir_mesh);
#ifdef MESH4
	mesh_circle(self->dir_mesh, 0.03f, 8, dir);
	mesh_extrude_edges(self->dir_mesh, 1,
			vec4_scale(dir, 0.5), 1.0f, NULL, NULL, NULL);
	mesh_extrude_edges(self->dir_mesh, 1,
			vec4(0.0, 0.0, 0.0, 0.0), 1.70f, NULL, NULL, NULL);
	mesh_extrude_edges(self->dir_mesh, 1,
			vec4_scale(dir, 0.1), 0.01f, NULL, NULL, NULL);
#else
	mesh_circle(self->dir_mesh, 0.03f, 8, dir.xyz);
	mesh_extrude_edges(self->dir_mesh, 1,
			vec4_scale(dir, 0.5).xyz, 1.0f, NULL, NULL, NULL);
	mesh_extrude_edges(self->dir_mesh, 1,
			vec4(0.0, 0.0, 0.0, 0.0).xyz, 1.70f, NULL, NULL, NULL);
	mesh_extrude_edges(self->dir_mesh, 1,
			vec4_scale(dir, 0.1).xyz, 0.01f, NULL, NULL, NULL);
#endif
	mesh_unlock(self->dir_mesh);

	entity_add_component(c_entity(self),
			c_model_new(type?g_rot_axis_mesh:self->dir_mesh, m, 0, !self->type));
	c_model(self)->xray = 1;
	c_model(self)->scale_dist = 0.2f;

	self->dir = dir.xyz;

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

			c_spacial_t *sc = c_spacial(&target);

			if(self->type == 0)
			{
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
					/* dir = mat4_mul_vec4(sc->rot_matrix, vec4(_vec3(dir), 0.0f)).xyz; */
					dir = quat_mul_vec3(sc->rot_quat, dir);
				}
				dir = vec3_scale(dir, -event->sy * 0.04);
				c_spacial_set_pos(sc, vec3_add(dir, sc->pos));
			}
			else
			{
				if(self->dir.x > 0.0f)
				{
					c_spacial_rotate_X(sc, -event->sy * 0.04);
				}
				else if(self->dir.y > 0.0f)
				{
					c_spacial_rotate_Y(sc, -event->sy * 0.04);
				}
				else if(self->dir.z > 0.0f)
				{
					c_spacial_rotate_Z(sc, -event->sy * 0.04);
				}
			}
		}
	}

	return CONTINUE;
}

int c_axis_created(c_axis_t *self)
{
	entity_signal_same(c_entity(self), sig("mesh_changed"), NULL);
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
