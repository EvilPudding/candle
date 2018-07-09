#include "skin.h"
#include <candle.h>
#include <components/spacial.h>
#include <components/node.h>
#include <components/model.h>
#include <components/mesh_gl.h>
#include <utils/mafs.h>
#include <utils/mesh.h>

vs_t *g_skin_vs;

static void c_skin_init(c_skin_t *self)
{
	if(!g_skin_vs)
	{
		g_skin_vs = vs_new("skin", 1, vertex_modifier_new(
			"	{\n"
			"		const vec3 colors[] = vec3[]("
						"vec3(1,0,0),"
						"vec3(0,1,0),"
						"vec3(0,1,1),"
						"vec3(0,0,1),"
						"vec3(1,0,1),"
						"vec3(1,1,0),"
						"vec3(0,1,0.5),"
						"vec3(1,0.5,1),"
						"vec3(0,0.5,1),"
						"vec3(0.5,1,0),"
						"vec3(0.3,0,0)"
				");\n"
			"#ifdef MESH4\n"
			"		float Y = cos(angle4);\n"
			"		float W = sin(angle4);\n"
			"		pos = vec4(vec3(P.x, P.y * Y + P.w * W, P.z), 1.0);\n"
			"#endif\n"
			"		vertex_position = (camera.view * M * pos).xyz;\n"

			"		poly_color = COL;\n"

			/* "			poly_color = colors[int(BID[0])] * WEI[0];\n" */
			/* "			poly_color += colors[int(BID[1])] * WEI[1];\n" */
			/* "			poly_color += colors[int(BID[2])] * WEI[2];\n" */
			/* "			poly_color += colors[int(BID[3])] * WEI[3];\n" */
			"		mat4 MODIFIER;\n"
			"		float wei = WEI[0] + WEI[1] + WEI[2] + WEI[3];\n"
			"		if(wei > 0) {\n"
			"			mat4 transform = bones[BID[0]] * WEI[0];\n"
			"			transform += bones[BID[1]] * WEI[1];\n"
			"			transform += bones[BID[2]] * WEI[2];\n"
			"			transform += bones[BID[3]] * WEI[3];\n"
			"			MODIFIER = transform;\n"
			"		} else\n"
			"		MODIFIER = M;\n"

			"		MODIFIER = camera.view * MODIFIER;\n"

			"		vec3 vertex_normal    = normalize(MODIFIER * vec4( N, 0.0f)).xyz;\n"
			"		vec3 vertex_tangent   = normalize(MODIFIER * vec4(TG, 0.0f)).xyz;\n"
			"		vec3 vertex_bitangent = cross(vertex_tangent, vertex_normal);\n"

			"		object_id = id;\n"
			"		poly_id = ID;\n"
			"		TM = mat3(vertex_tangent, vertex_bitangent, vertex_normal);\n"

			"		pos = (camera.projection * MODIFIER) * pos;\n"
			"	}\n"
		));
	}
}

c_skin_t *c_skin_new()
{
	c_skin_t *self = component_new("skin");
	c_model(self)->visible = 0;
	return self;
}

void c_skin_vert_prealloc(c_skin_t *self, int size)
{
	self->wei = realloc(self->wei, (self->vert_alloc + size) *
			sizeof(*self->wei));
	self->bid = realloc(self->bid, (self->vert_alloc + size) *
			sizeof(*self->bid));

	memset(self->wei + self->vert_alloc, 0, size * sizeof(*self->wei));
	memset(self->bid + self->vert_alloc, 0, size * sizeof(*self->bid));

	self->vert_alloc += size;
}

static void c_skin_bind_bones(c_skin_t *self, shader_t *shader)
{
	/* c_node_t *nc = c_node(self); */
	/* c_node_update_model(nc); */
	/* mat4_t inverse = mat4_invert(nc->model); */
	int i;
	for(i = 0; i < self->bones_num; i++)
	{
		c_node_t *node = c_node(&self->bones[i]);
		c_node_update_model(node);
		mat4_t transform = mat4_mul(node->model, self->off[i]);
		glUniformMatrix4fv(shader->u_bones[i], 1, GL_FALSE, (void*)transform._);
	}
}

int c_skin_render_visible(c_skin_t *self)
{
	/* if(!self->mesh || !self->visible) return CONTINUE; */
	shader_t *shader = vs_bind(g_skin_vs);
	c_skin_bind_bones(self, shader);
	return c_model_render(c_model(self), shader, 0);
}

int c_skin_render_shadows(c_skin_t *self)
{
	c_skin_render_visible(self);
	return CONTINUE;
}


int c_skin_render_selectable(c_skin_t *self)
{
	/* if(!self->mesh || !self->visible) return CONTINUE; */
	shader_t *shader = vs_bind(g_skin_vs);
	c_skin_bind_bones(self, shader);
	c_model_render(c_model(self), shader, 2);
	return CONTINUE;
}


int c_skin_render_transparent(c_skin_t *self)
{
	/* if(!self->mesh || !self->visible) return CONTINUE; */

	shader_t *shader = vs_bind(g_skin_vs);
	c_skin_bind_bones(self, shader);
	return c_model_render(c_model(self), shader, 1);
}


REG()
{
	ct_t *ct = ct_new("skin", sizeof(c_skin_t),
			c_skin_init, NULL, 2, ref("model"), ref("node"));

	ct_listener(ct, WORLD, sig("render_visible"), c_skin_render_visible);

	ct_listener(ct, WORLD, sig("render_transparent"), c_skin_render_transparent);

	ct_listener(ct, WORLD, sig("render_shadows"), c_skin_render_shadows);

	ct_listener(ct, WORLD, sig("render_selectable"), c_skin_render_selectable);
}

