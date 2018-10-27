#include "sprite.h"
#include "node.h"
#include "spacial.h"
#include "light.h"
#include <utils/drawable.h>
#include <utils/nk.h>
#include <utils/shader.h>
#include <systems/editmode.h>
#include <candle.h>

vs_t *g_sprite_vs;

static int c_sprite_position_changed(c_sprite_t *self);
mesh_t *g_sprite_mesh;

/* int c_sprite_menu(c_sprite_t *self, void *ctx); */

vs_t *sprite_vs()
{
	if(!g_sprite_mesh)
	{
		g_sprite_mesh = mesh_new();
		mesh_quad(g_sprite_mesh);
		g_sprite_mesh->cull = 0;
		g_sprite_vs = vs_new("sprite", 1, vertex_modifier_new(
			"	{\n"
			"		mat4 MV = camera(view) * M;\n"
			"		pos = (camera(projection) * MV) * vec4(0.0f, 0.0f, 0.0f, 1.0f);\n"
			"		vertex_position = pos.xyz;\n"
			"		vec2 size = vec2(P.x * (screen_size.y / screen_size.x), P.y);\n"
			"		pos = vec4(pos.xy + 0.5 * size, pos.z, pos.w);\n"

			"		vec3 vertex_normal    = (vec4( N, 0.0f)).xyz;\n"
			"		vec3 vertex_tangent   = (vec4(TG, 0.0f)).xyz;\n"
			"		vec3 vertex_bitangent = cross(vertex_normal, vertex_tangent);\n"
			"		texcoord = vec2(UV.x, 1.0f - UV.y);\n"

			"		object_id = id;\n"
			"		poly_id = ID;\n"
			"		TM = mat3(vertex_tangent, vertex_bitangent, vertex_normal);\n"
			"	}\n"
		));
	}
	return g_sprite_vs;
}

static void c_sprite_init(c_sprite_t *self)
{
	sprite_vs();
	drawable_init(&self->draw, ref("visible"), NULL);
	drawable_add_group(&self->draw, ref("selectable"));
	drawable_set_vs(&self->draw, g_sprite_vs);
	drawable_set_mesh(&self->draw, g_sprite_mesh);
	drawable_set_entity(&self->draw, c_entity(self));

	self->visible = 1;
}

c_sprite_t *c_sprite_new(mat_t *mat, int cast_shadow)
{
	c_sprite_t *self = component_new("sprite");

	self->cast_shadow = cast_shadow;

	self->mat = mat;
	drawable_set_mat(&self->draw, mat ? mat->id : 0);

	c_sprite_position_changed(self);
	return self;
}

int c_sprite_created(c_sprite_t *self)
{
	entity_signal_same(c_entity(self), sig("mesh_changed"), NULL, NULL);
	return CONTINUE;
}

static int c_sprite_position_changed(c_sprite_t *self)
{
	c_node_t *node = c_node(self);
	c_node_update_model(node);
	drawable_set_transform(&self->draw, node->model);

	return CONTINUE;
}

void c_sprite_destroy(c_sprite_t *self)
{
	drawable_set_mesh(&self->draw, NULL);
}

REG()
{
	ct_t *ct = ct_new("sprite", sizeof(c_sprite_t),
			c_sprite_init, c_sprite_destroy, 1, ref("node"));

	ct_listener(ct, ENTITY, sig("entity_created"), c_sprite_created);

	/* ct_listener(ct, WORLD, component_menu, c_sprite_menu); */

	ct_listener(ct, ENTITY, sig("node_changed"), c_sprite_position_changed);
}
