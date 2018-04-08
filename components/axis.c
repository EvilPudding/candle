#include "axis.h"
#include "mesh_gl.h"
#include "model.h"
#include "node.h"
#include "spacial.h"
#include "light.h"
#include <nk.h>
#include <systems/renderer.h>
#include "shader.h"
#include <systems/editmode.h>
#include <candle.h>


static int c_axis_render_xray(c_axis_t *self);

static mesh_t *g_axis_mesh = NULL;

static void c_axis_init(c_axis_t *self)
{
	if(!g_axis_mesh)
	{
		g_axis_mesh = mesh_circle(0.01f, 8);

		mesh_lock(g_axis_mesh);
#ifdef MESH4
		mesh_extrude_edges(g_axis_mesh, 1,
				vec4(0.0, 0.5, 0.0, 0.0), 1.0f, NULL);
		mesh_extrude_edges(g_axis_mesh, 1,
				vec4(0.0, 0.0, 0.0, 0.0), 2.00f, NULL);
		mesh_extrude_edges(g_axis_mesh, 1,
				vec4(0.0, 0.05, 0.0, 0.0), 0.01f, NULL);
#else
		mesh_extrude_edges(g_axis_mesh, 1,
				vec3(0.0, 0.5, 0.0), 1.0f, NULL);
		mesh_extrude_edges(g_axis_mesh, 1,
				vec3(0.0, 0.0, 0.0), 2.00f, NULL);
		mesh_extrude_edges(g_axis_mesh, 1,
				vec3(0.0, 0.05, 0.0), 0.01f, NULL);
#endif
		mesh_unlock(g_axis_mesh);

	}
}

c_axis_t *c_axis_new(vec4_t dir)
{
	c_axis_t *self = component_new("axis");

	mat_t *m = mat_new("m");
	m->diffuse.color.xyz = dir.xyz;

	entity_add_component(c_entity(self), c_model_new(g_axis_mesh, m, 0, 0));

	return self;
}

int c_axis_created(c_axis_t *self)
{
	entity_signal_same(c_entity(self), sig("mesh_changed"), NULL);
	return CONTINUE;
}

static int c_axis_render_xray(c_axis_t *self)
{
	return c_model_render(c_model(self), 0);
}


REG()
{
	ct_t *ct = ct_new("axis", sizeof(c_axis_t),
			c_axis_init, NULL, 1, ref("node"));

	ct_listener(ct, ENTITY, sig("entity_created"), c_axis_created);

	ct_listener(ct, WORLD, sig("render_xray"), c_axis_render_xray);
}
