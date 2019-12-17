#include "field.h"
#include "node.h"
#include "spatial.h"
#include "light.h"
#include <utils/drawable.h>
#include <utils/nk.h>
#include <utils/shader.h>
#include <systems/editmode.h>
#include <candle.h>

static vs_t *g_field_vs;

static int c_field_position_changed(c_field_t *self);

vs_t *field_vs()
{
	if (!g_field_vs)
	{
		g_field_vs = vs_new("field", false, 1, geometry_modifier_new(
			"#include \"marching.glsl\"\n"
		));
	}
	return g_field_vs;
}

static void c_field_init(c_field_t *self)
{
	field_vs();
	drawable_init(&self->draw, ref("visible"));
	drawable_add_group(&self->draw, ref("selectable"));
	drawable_set_vs(&self->draw, g_field_vs);
	drawable_set_entity(&self->draw, c_entity(self));

	self->visible = 1;
}

void c_field_update_mat(c_field_t *self)
{
	if (self->mat)
	{
		int transp = mat_is_transparent(self->mat);
		if (self->draw.bind[0].grp == ref("transparent")
				|| self->draw.bind[0].grp == ref("visible"))
		{
			drawable_set_group(&self->draw, transp ? ref("transparent") : ref("visible"));
		}
	}

	drawable_set_mat(&self->draw, self->mat);
}

void c_field_set_mat(c_field_t *self, mat_t *mat)
{
	if (!mat)
	{
		mat = g_mats[rand()%g_mats_num];
	}
	if (self->mat != mat)
	{
		self->mat = mat;
		c_field_update_mat(self);
	}

}

int c_field_menu(c_field_t *self, void *ctx)
{
	int changes = 0;
	if (self->mat && self->mat->name[0] != '_')
	{
		changes |= mat_menu(self->mat, ctx);
		c_field_update_mat(self);
	}
	if (changes)
	{
		drawable_model_changed(&self->draw);
	}
	return CONTINUE;
}

void c_field_pre_draw(c_field_t *self, shader_t *shader)
{

	uint32_t loc = shader_cached_uniform(shader, ref("values"));
	glUniform1i(loc, 6);
	glActiveTexture(GL_TEXTURE0 + 6);
	texture_bind(self->values, 0);
	glerr();
}

c_field_t *c_field_new(mat_t *mat, vec3_t start, vec3_t end, float cell_size,
                       int cast_shadow)
{
	vec3_t size;
	uvec3_t segments;
	uint32_t x, y, z;
	c_field_t *self = component_new("field");

	self->cast_shadow = cast_shadow;

	size = vec3_sub(end, start);
	segments = uvec3(size.x / cell_size,
	                 size.y / cell_size,
	                 size.z / cell_size);
	self->values = texture_new_3D(_vec3(segments), 1);
	for (x = 0; x < segments.x; x++)
		for (y = 0; y < segments.y; y++)
			for (z = 0; z < segments.z; z++)
	{
		float fx = cell_size * x - start.x;
		float fy = cell_size * y - start.y;
		/* float fz = cell_size * z - start.z; */
		uint32_t i = x + y * segments.x + z * (segments.x * segments.y);
		float sx = sinf(fx);
		float cy = cosf(fy);
		if (sx < 0.0f) sx = 0.0f;
		if (cy < 0.0f) cy = 0.0f;

		self->values->bufs[0].data[i] =   sx * cy * 255.0f;
	}
	texture_update_gl(self->values);

	self->mesh = mesh_new();
	mesh_point_grid(self->mesh, start, size, segments);
	drawable_set_mesh(&self->draw, self->mesh);
	drawable_set_callback(&self->draw, (draw_cb)c_field_pre_draw, self);
	c_field_set_mat(self, mat);

	c_field_position_changed(self);
	return self;
}

int c_field_created(c_field_t *self)
{
	entity_signal_same(c_entity(self), ref("mesh_changed"), NULL, NULL);
	return CONTINUE;
}

static int c_field_position_changed(c_field_t *self)
{
	c_node_t *node = c_node(self);
	c_node_update_model(node);
	drawable_set_transform(&self->draw, node->model);
	drawable_set_entity(&self->draw, node->unpack_inheritance);

	return CONTINUE;
}

void c_field_destroy(c_field_t *self)
{
	drawable_set_mesh(&self->draw, NULL);
}

REG()
{
	ct_t *ct = ct_new("field", sizeof(c_field_t),
			(init_cb)c_field_init, (destroy_cb)c_field_destroy, 1, ref("node"));

	ct_listener(ct, ENTITY, 0, ref("entity_created"), c_field_created);

	ct_listener(ct, WORLD, 0, ref("component_menu"), c_field_menu);

	ct_listener(ct, ENTITY, 0, ref("node_changed"), c_field_position_changed);
}
