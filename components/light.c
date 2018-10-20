#include "light.h"
#include "node.h"
#include "probe.h"
#include "spacial.h"
#include "model.h"
#include <utils/drawable.h>
#include <candle.h>
#include <systems/editmode.h>
#include <systems/window.h>
#include <systems/renderer.h>
#include <stdlib.h>

static fs_t *g_depth_fs = NULL;
static mesh_t *g_light;
extern vs_t *g_quad_vs;
extern mesh_t *g_quad_mesh;
/* entity_t g_light = entity_null; */
static int g_lights_num;

void c_light_init(c_light_t *self)
{
	self->color = vec4(1.0f);
	self->shadow_size = 512;
	self->radius = 5.0f;

	if(!g_light)
	{
		g_depth_fs = fs_new("depth");

		g_light = mesh_new();
		mesh_lock(g_light);
		mesh_ico(g_light, -0.5f);
		mesh_select(g_light, SEL_EDITING, MESH_FACE, -1);
		mesh_subdivide(g_light, 1);
		mesh_spherize(g_light, 1.0f);
		mesh_unlock(g_light);

		/* g_light = entity_new(c_node_new(), c_model_new(mesh, NULL, 0, 0)); */
		/* c_node(&g_light)->ghost = 1; */

	}
	self->id = g_lights_num++;

	drawable_init(&self->draw, ref("light"), NULL);
	drawable_set_vs(&self->draw, g_model_vs);
	drawable_set_mesh(&self->draw, g_light);
	drawable_set_mat(&self->draw, self->id);
	drawable_set_entity(&self->draw, c_entity(self));

}

static int c_light_position_changed(c_light_t *self)
{
	if(self->radius == -1)
	{
		drawable_set_group(&self->draw, ref("ambient"));
		drawable_set_vs(&self->draw, g_quad_vs);
		drawable_set_mesh(&self->draw, g_quad_mesh);
	}
	else
	{
		drawable_set_group(&self->draw, ref("light"));
		drawable_set_vs(&self->draw, g_model_vs);
		drawable_set_mesh(&self->draw, g_light);

		c_node_t *node = c_node(self);
		c_node_update_model(node);
		vec3_t pos = c_node_local_to_global(node, vec3(0, 0, 0));
		mat4_t model = mat4_translate(pos);
		model = mat4_scale_aniso(model, vec3(self->radius * 1.15f));

		drawable_set_transform(&self->draw, model);
	}
	if(self->radius > 0 && !c_probe(self))
	{
		entity_add_component(c_entity(self),
				(c_t*)c_probe_new(self->shadow_size, ref("shadow"), 0, g_depth_fs));
	}

	return CONTINUE;
}

c_light_t *c_light_new(float radius,
		vec4_t color, int shadow_size)
{
	c_light_t *self = component_new("light");

	self->color = color;
	self->shadow_size = shadow_size;
	self->radius = radius;

	return self;
}

int c_light_menu(c_light_t *self, void *ctx)
{
	nk_layout_row_dynamic(ctx, 0, 1);

	int ambient = self->radius == -1.0f;
	nk_checkbox_label(ctx, "ambient", &ambient);

	if(!ambient)
	{
		float rad = self->radius;
		if(self->radius < 0.0f) self->radius = 0.01;
		nk_property_float(ctx, "radius:", 0.01, &rad, 1000, 0.1, 0.05);
		if(rad != self->radius)
		{
			self->radius = rad;
			c_light_position_changed(self);
		}
	}
	else
	{
		self->radius = -1;
	}

	nk_layout_row_dynamic(ctx, 180, 1);

	union { struct nk_colorf *nk; vec4_t *v; } color = { .v = &self->color };
	*color.nk = nk_color_picker(ctx, *color.nk, NK_RGBA);

	return CONTINUE;
}

void c_light_destroy(c_light_t *self)
{
}

REG()
{
	ct_t *ct = ct_new("light", sizeof(c_light_t), c_light_init,
			c_light_destroy, 1, ref("node"));

	ct_listener(ct, WORLD, sig("component_menu"), c_light_menu);

	ct_listener(ct, ENTITY, sig("node_changed"), c_light_position_changed);
}
