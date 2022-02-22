#include "sprite.h"
#include "node.h"
#include "spatial.h"
#include "light.h"
#include <utils/drawable.h>
#include <utils/nk.h>
#include <utils/shader.h>
#include <systems/editmode.h>
#include <candle.h>

static vs_t *g_sprite_vs;
static vs_t *g_fixed_sprite_vs;

static int c_sprite_position_changed(c_sprite_t *self);
static mesh_t *g_sprite_mesh;

mesh_t *sprite_mesh()
{
	if(!g_sprite_mesh)
	{
		g_sprite_mesh = mesh_new();
		mesh_quad(g_sprite_mesh);
	}
	return g_sprite_mesh;
}

vs_t *sprite_vs()
{
	if(!g_sprite_vs)
	{
		g_sprite_vs = vs_new("sprite", false, 1, vertex_modifier_new(
			"{\n"
			"	vec2 scale = vec2(length(M[0].xyz), length(M[1].xyz));\n"
			"	pos = M * vec4(0.0f, 0.0f, 0.0f, 1.0f);\n"
			"	$vertex_world_position = pos.xyz;\n"
			"	pos = camera(view) * pos;\n"
			"	$vertex_position = pos.xyz;\n"
			"	pos = camera(projection) * pos;\n"
			"	vec2 size = vec2(P.x * (screen_size.y / screen_size.x), P.y) * 0.5 * scale;\n"
			"	pos = vec4(pos.xy + 0.5 * size, pos.z, pos.w);\n"
			"	$vertex_normal    = (vec4( N, 0.0f)).xyz;\n"
			"	$vertex_tangent   = (vec4(TG, 0.0f)).xyz;\n"
			"	$texcoord = vec2(UV.x, 1.0f - UV.y);\n"
			"}\n"
		));
	}
	return g_sprite_vs;
}

vs_t *fixed_sprite_vs()
{
	if(!g_fixed_sprite_vs)
	{
		g_fixed_sprite_vs = vs_new("fixed_sprite", false, 1, vertex_modifier_new(
			"{\n"
			"	vec2 scale = vec2(length(M[0].xyz), length(M[1].xyz));"
			/* "   * length($obj_pos - camera(pos));\n" */
			/* "	pos = vec4($obj_pos, 0.0);\n" */
			"	pos = M * vec4(0.0f, 0.0f, 0.0f, 1.0f);\n"
			"	$vertex_world_position = pos.xyz;\n"
			"	pos = camera(view) * pos;\n"
			"	$vertex_position = pos.xyz;\n"
			"	pos = camera(projection) * pos;\n"
			"	vec2 size = vec2(P.x * (screen_size.y / screen_size.x), P.y) * 0.5 * scale;\n"
			"	pos = vec4(pos.xy + 0.5 * size, pos.z, pos.w);\n"
			"	$vertex_normal    = (vec4( N, 0.0f)).xyz;\n"
			"	$vertex_tangent   = (vec4(TG, 0.0f)).xyz;\n"
			"	$texcoord = vec2(UV.x, 1.0f - UV.y);\n"
			"}\n"
		));
	}
	return g_fixed_sprite_vs;
}

static void c_sprite_init(c_sprite_t *self)
{
	sprite_vs();
	drawable_init(&self->draw, ref("visible"));
	drawable_add_group(&self->draw, ref("selectable"));
	drawable_set_vs(&self->draw, g_sprite_vs);
	drawable_set_mesh(&self->draw, g_sprite_mesh);
	drawable_set_entity(&self->draw, c_entity(self));
	drawable_set_cull(&self->draw, 0);

	self->visible = 1;
}

void c_sprite_update_mat(c_sprite_t *self)
{
	if(self->mat)
	{
		int transp = mat_is_transparent(self->mat);
		if(self->draw.bind[0].grp == ref("transparent")
				|| self->draw.bind[0].grp == ref("visible"))
		{
			drawable_set_group(&self->draw, transp ? ref("transparent") : ref("visible"));
		}
	}

	drawable_set_mat(&self->draw, self->mat);
}

void c_sprite_set_mat(c_sprite_t *self, mat_t *mat)
{
	if(!mat)
	{
		mat = g_mats[rand()%g_mats_num];
	}
	if(self->mat != mat)
	{
		self->mat = mat;
		c_sprite_update_mat(self);
	}

}

int c_sprite_menu(c_sprite_t *self, void *ctx)
{
	int changes = 0;
	if(self->mat && self->mat->name[0] != '_')
	{
		changes |= mat_menu(self->mat, ctx);
		c_sprite_update_mat(self);
	}
	if(changes)
	{
		drawable_model_changed(&self->draw);
	}
	return CONTINUE;
}

int c_sprite_created(c_sprite_t *self)
{
	entity_signal_same(c_entity(self), ref("mesh_changed"), NULL, NULL);
	return CONTINUE;
}

static int c_sprite_position_changed(c_sprite_t *self)
{
	c_node_t *node = c_node(self);
	c_node_update_model(node);
	drawable_set_transform(&self->draw, node->model);
	drawable_set_entity(&self->draw, node->unpack_inheritance);

	return CONTINUE;
}

void c_sprite_destroy(c_sprite_t *self)
{
	drawable_set_mesh(&self->draw, NULL);
}

void ct_sprite(ct_t *self)
{
	ct_init(self, "sprite", sizeof(c_sprite_t));
	ct_set_init(self, (init_cb)c_sprite_init);
	ct_set_destroy(self, (destroy_cb)c_sprite_destroy);
	ct_add_dependency(self, ct_node);
	ct_add_listener(self, ENTITY, 0, ref("entity_created"), c_sprite_created);
	ct_add_listener(self, WORLD, 0, ref("component_menu"), c_sprite_menu);
	ct_add_listener(self, ENTITY, 0, ref("node_changed"), c_sprite_position_changed);
}

c_sprite_t *c_sprite_new(mat_t *mat, int cast_shadow)
{
	c_sprite_t *self = component_new(ct_sprite);

	self->cast_shadow = cast_shadow;

	c_sprite_set_mat(self, mat);

	c_sprite_position_changed(self);
	return self;
}

