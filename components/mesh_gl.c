#include "mesh_gl.h"
#include <utils/loader.h>
#include "model.h"
#include <candle.h>
#include <components/spacial.h>
#include <components/skin.h>
#include <string.h>
#include <stdlib.h>

KHASH_MAP_INIT_INT(meshes, glg_t*)

static khash_t(meshes) *g_glos;

static int glg_update_buffers(glg_t *self);
static void glg_update_vbos(glg_t *self);
static int glg_add_instance(glg_t *self, mat4_t model);

static void c_mesh_gl_init(c_mesh_gl_t *self)
{
	if(!g_glos) g_glos = kh_init(meshes);
}

c_mesh_gl_t *c_mesh_gl_new()
{
	c_mesh_gl_t *self = component_new("mesh_gl");

	return self;
}

/* static void mesh_gl_add_group(c_mesh_gl_t *self) */
/* { */
/* 	int i = self->groups_num++; */
/* 	glg_t *group = &self->groups[i]; */

/* 	group->gl_ind_num = 0; */
/* 	group->gl_vert_num = 0; */
/* 	group->ready = 0; */
/* 	group->update_id = -1; */
/* 	group->updated = 1; */
/* 	group->vao = 0; */
/* 	group->entity = c_entity(self); */
/* 	group->layer_id = i; */
/* 	group->skinned = !!c_skin(self); */
/* } */

/* #ifdef MESH4 */
/* static int glg_get_vert(glg_t *self, vec4_t p, vec3_t n, vec2_t t) */
/* #else */
/* static int glg_get_vert(glg_t *self, vec3_t p, vec3_t n, vec2_t t) */
/* #endif */
/* { */
/* #ifdef OPTIMIZE_DRAW_MESH */
/* 	int i; */
/* 	for(i = 0; i < self->vert_num; i++) */
/* 	{ */
/* 		if(vec3_equals(self->pos[i], p) && */
/* 				vec3_equals(self->nor[i], n) && */
/* 				vec2_equals(self->tex[i], t)) return i; */
/* 	} */
/* #endif */
/* 	return -1; */
/* } */


static void glg_clear(glg_t *self)
{
	/* if(self->pos) free(self->pos); */
	/* if(self->nor) free(self->nor); */
	/* if(self->tex) free(self->tex); */
	/* if(self->tan) free(self->tan); */
	/* if(self->bit) free(self->bit); */
	/* if(self->ind) free(self->ind); */
	/* self->pos = self->nor = self->tex = */
	/* 	self->tan = self->bit = self->ind = NULL; */

	self->vert_num = 0;
	self->ind_num = 0;
}

void glg_vert_prealloc(glg_t *self, int size)
{
	self->vert_alloc += size;

	self->pos = realloc(self->pos, self->vert_alloc * sizeof(*self->pos));
	self->nor = realloc(self->nor, self->vert_alloc * sizeof(*self->nor));
	self->tex = realloc(self->tex, self->vert_alloc * sizeof(*self->tex));
	self->tan = realloc(self->tan, self->vert_alloc * sizeof(*self->tan));
	self->col = realloc(self->col, self->vert_alloc * sizeof(*self->col));
	if(self->skin)
	{
		self->wei = realloc(self->wei, self->vert_alloc * sizeof(*self->wei));
		self->bid = realloc(self->bid, self->vert_alloc * sizeof(*self->bid));
	}
	self->id = realloc(self->id, self->vert_alloc * sizeof(*self->id));
}

void glg_inst_prealloc(glg_t *self, int size)
{
	self->inst_alloc += size;

	self->inst = realloc(self->inst, self->inst_alloc * sizeof(*self->inst));
}
void glg_inst_grow(glg_t *self)
{
	if(self->inst_alloc < self->inst_num)
	{
		self->inst_alloc = self->inst_num;

		glg_inst_prealloc(self, 0);
	}
}

void glg_vert_grow(glg_t *self)
{
	if(self->vert_alloc < self->vert_num)
	{
		self->vert_alloc = self->vert_num;

		glg_vert_prealloc(self, 0);
	}
}

void glg_ind_prealloc(glg_t *self, int size)
{
	self->ind_alloc += size;

	self->ind = realloc(self->ind, self->ind_alloc * sizeof(*self->ind));
}

void glg_ind_grow(glg_t *self)
{
	if(self->ind_alloc < self->ind_num)
	{
		self->ind_alloc = self->ind_num;
		glg_ind_prealloc(self, 0);
	}
}


static int glg_add_instance(glg_t *self, mat4_t model)
{
	int i = self->inst_num++;
	glg_inst_grow(self);

	self->inst[i] = model;
	return i;
}

static int glg_add_vert(
		glg_t *self,
		vecN_t p,
		vec3_t n,
		vec2_t t,
		vec3_t tg,
		vec3_t c,
		int id,
		vec4_t wei,
		uvec4_t bid)
{
	/* int i = glg_get_vert(self, p, n, t); */
	/* if(i >= 0) return i; */
	int i = self->vert_num++;
	glg_vert_grow(self);

	self->pos[i] = p;
	self->nor[i] = n;
	self->tan[i] = tg;
	self->tex[i] = t;
	self->col[i] = c;
	self->id[i] = int_to_vec2(id);

	if(self->skin)
	{
		self->bid[i] = bid;
		self->wei[i] = wei;
		/* printf("%u %u %u %u\n", self->bid[i].x, self->bid[i].y, */
		/* 		self->bid[i].z, self->bid[i].w); */
		/* printf("%f %f %f %f\n", self->wei[i].x, self->wei[i].y, */
		/* 		self->wei[i].z, self->wei[i].w); */
	}


	/* union { */
	/* 	unsigned int i; */
	/* 	struct{ */
	/* 		unsigned char r, g, b, a; */
	/* 	}; */
	/* } convert = {.i = id + 1}; */
	/* printf("%d = [%d]%d(%f) [%d]%d(%f)\n", id + 1, */
	/* 		convert.r, (int)round(self->id[i].x*255), self->id[i].x, */
	/* 		convert.g, (int)round(self->id[i].y*255), self->id[i].y); */

	return i;
}

void glg_add_line(glg_t *self, int v1, int v2)
{
	int i = self->ind_num;

	self->ind_num += 2;
	glg_ind_grow(self);

	self->ind[i + 0] = v1;
	self->ind[i + 1] = v2;
}

void glg_add_triangle(glg_t *self, int v1, int v2, int v3)
{
	int i = self->ind_num;
	self->ind_num += 3;
	glg_ind_grow(self);

	self->ind[i + 0] = v1;
	self->ind[i + 1] = v2;
	self->ind[i + 2] = v3;

	/* vec2_print(self->id[v1]); */
	/* vec2_print(self->id[v2]); */
	/* vec2_print(self->id[v3]); */
	/* printf("\n"); */
}

void glg_edges_to_gl(glg_t *self)
{
	int i;
	mesh_t *mesh = self->mesh;

	glg_ind_prealloc(self, vector_count(mesh->edges) * 2);

	for(i = 0; i < vector_count(mesh->edges); i++)
	{
		edge_t *curr_edge = m_edge(mesh, i);
		if(!curr_edge) continue;

		edge_t *next_edge = m_edge(mesh, curr_edge->next);
		if(!next_edge) continue;

		vertex_t *V1 = e_vert(curr_edge, mesh);
		vertex_t *V2 = e_vert(next_edge, mesh);
		int v1 = glg_add_vert(self, V1->pos,
				vec3(0.0f), vec2(0.0f), vec3(0.0f), V1->color.xyz, 0, vec4(0,0,0,0), uvec4(0,0,0,0));
		int v2 = glg_add_vert(self, V2->pos,
				vec3(0.0f), vec2(0.0f), vec3(0.0f), V2->color.xyz, 0, vec4(0,0,0,0), uvec4(0,0,0,0));

		glg_add_line(self, v1, v2);

	}
}

void glg_face_to_gl(glg_t *self, face_t *f, int id)
{
	mesh_t *mesh = self->mesh;
	int v[4], i;
	for(i = 0; i < f->e_size; i++)
	{
		edge_t *hedge = f_edge(f, i, mesh);
		vertex_t *V = e_vert(hedge, mesh);

		vec4_t  wei = vec4(0,0,0,0);
		uvec4_t bid = uvec4(0,0,0,0);
		if(self->skin)
		{
			wei = self->skin->wei[hedge->v];
			bid = self->skin->bid[hedge->v];
		}
		v[i] = glg_add_vert(self, V->pos, hedge->n,
				hedge->t, hedge->tg, V->color.xyz, id, wei, bid);
	}
	if(f->e_size == 4)
	{
		glg_add_triangle(self, v[0], v[1], v[2]);
		glg_add_triangle(self, v[2], v[3], v[0]);

	}
	else if(f->e_size == 3)
	{
		glg_add_triangle(self, v[0], v[1], v[2]);
	}
	else
	{
		printf("%d\n", f->e_size);
	}

}

/* void glg_get_tg_bt(glg_t *self) */
/* { */
/* 	int i; */

/* 	/1* memset(self->tan, 0, self->ind_num * sizeof(*self->tan)); *1/ */
/* 	/1* memset(self->bit, 0, self->ind_num * sizeof(*self->bit)); *1/ */

/* 	for(i = 0; i < self->ind_num; i += 3) */
/* 	{ */
/* 		int i0 = self->ind[i + 0]; */
/* 		int i1 = self->ind[i + 1]; */
/* 		int i2 = self->ind[i + 2]; */
/* 		self->tan[i0] = vec3(0.0f); */
/* 		self->tan[i1] = vec3(0.0f); */
/* 		self->tan[i2] = vec3(0.0f); */

/* 		/1* self->bit[i0] = vec3(0.0f); *1/ */
/* 		/1* self->bit[i1] = vec3(0.0f); *1/ */
/* 		/1* self->bit[i2] = vec3(0.0f); *1/ */
/* 	} */
/* 	for(i = 0; i < self->ind_num; i += 3) */
/* 	{ */
/* 		int i0 = self->ind[i + 0]; */
/* 		int i1 = self->ind[i + 1]; */
/* 		int i2 = self->ind[i + 2]; */

/* 		vec3_t v0 = ((vec3_t*)self->pos)[i0]; */
/* 		vec3_t v1 = ((vec3_t*)self->pos)[i1]; */
/* 		vec3_t v2 = ((vec3_t*)self->pos)[i2]; */

/* 		vec2_t w0 = ((vec2_t*)self->tex)[i0]; */
/* 		vec2_t w1 = ((vec2_t*)self->tex)[i1]; */
/* 		vec2_t w2 = ((vec2_t*)self->tex)[i2]; */

/* 		vec3_t dp1 = vec3_sub(v1, v0); */
/* 		vec3_t dp2 = vec3_sub(v2, v0); */

/* 		vec2_t duv1 = vec2_sub(w1, w0); */
/* 		vec2_t duv2 = vec2_sub(w2, w0); */

/* 		float r = 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x); */
/* 		vec3_t tangent   = vec3_scale(vec3_sub(vec3_scale(dp1, duv2.y), vec3_scale(dp2, duv1.y)), r); */
/* 		/1* vec3_t bitangent = vec3_scale(vec3_sub(vec3_scale(dp2, duv1.x), vec3_scale(dp1, duv2.x)), r); *1/ */


/* 		self->tan[i0] = vec3_add(self->tan[i0], tangent); */
/* 		self->tan[i1] = vec3_add(self->tan[i1], tangent); */
/* 		self->tan[i2] = vec3_add(self->tan[i2], tangent); */

/* 		/1* self->bit[i0] = vec3_add(self->bit[i0], bitangent); *1/ */
/* 		/1* self->bit[i1] = vec3_add(self->bit[i1], bitangent); *1/ */
/* 		/1* self->bit[i2] = vec3_add(self->bit[i2], bitangent); *1/ */
/* 	} */
/* 	for(i = 0; i < self->ind_num; i++) */
/* 	{ */
/* 		int i0 = self->ind[i]; */
/* 		vec3_t n = self->nor[i0]; */
/* 		vec3_t t = self->tan[i0]; */
/* 		/1* vec3_t b = self->bit[i]; *1/ */

/* 		/1* Gram-Schmidt orthogonalize *1/ */
/* 		t = vec3_norm(vec3_sub(t, vec3_scale(n, vec3_dot(n, t)))); */

/* 		self->tan[i0] = t; */
/* 	} */
/* } */

static inline void create_buffer(glg_t *self, int id, void *arr, uint type, int dim,
		int instanced_divisor)
{
	unsigned int usage = instanced_divisor ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
	unsigned int num = instanced_divisor ? self->gl_inst_num : self->gl_vert_num;
	if(!self->vbo[id])
	{
		glGenBuffers(1, &self->vbo[id]); glerr();
		self->vbo_num++;
	}
	glBindBuffer(GL_ARRAY_BUFFER, self->vbo[id]); glerr();
	glBufferData(GL_ARRAY_BUFFER, dim * sizeof(GLfloat) * num, arr, usage);
	glerr();


	{
		// Because OpenGL doesn't support attributes with more than 4
		// elements, each set of 4 elements gets its own attribute.
		uint elementSizeDiv = dim / 4;
		uint remaining = dim % 4;
		uint j;
		for(j = 0; j < elementSizeDiv; j++) {
			glEnableVertexAttribArray(id);
			if(type == GL_FLOAT)
			{
				glVertexAttribPointer(id, 4, type, GL_FALSE,
					dim * sizeof(GLfloat),
					(const GLvoid*)(sizeof(GLfloat) * j * 4));
			}
			else
			{
				glVertexAttribIPointer(id, 4, type,
					dim * sizeof(GLfloat),
					(const GLvoid*)(sizeof(GLfloat) * j * 4));
			}

			glVertexAttribDivisor(id, instanced_divisor);
			glerr();
			id++;
		}
		if(remaining > 0)
		{
			glEnableVertexAttribArray(id);
			if(type == GL_FLOAT)
			{
				glVertexAttribPointer(id, remaining, type, GL_FALSE,
					remaining * sizeof(GLfloat),
					(const GLvoid*)(sizeof(GLfloat) * elementSizeDiv * 4));
			}
			else
			{
				glVertexAttribIPointer(id, remaining, type,
					remaining * sizeof(GLfloat),
					(const GLvoid*)(sizeof(GLfloat) * elementSizeDiv * 4));
			}

			glVertexAttribDivisor(id, instanced_divisor);
			glerr();
			id++;
		}
	}
}

static inline void update_buffer(glg_t *self, int id, void *arr, int dim, int inst)
{
	if(!self->vbo[id]) return;
	unsigned int num = inst ? self->gl_inst_num : self->gl_vert_num;

	glBindBuffer(GL_ARRAY_BUFFER, self->vbo[id]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, dim * sizeof(GLfloat) * num, arr);
}


int c_mesh_gl_on_mesh_changed(c_mesh_gl_t *self)
{
	c_model_t *model = c_model(self);
	if(!self->glo || model->mesh != self->glo->mesh)
	{
		uint p = hash_ptr(model->mesh);
		khiter_t k = kh_get(meshes, g_glos, p);
		if(k == kh_end(g_glos))
		{
			int ret;
			k = kh_put(meshes, g_glos, p, &ret);

			self->glo = kh_value(g_glos, k) = calloc(1, sizeof(glg_t));
			self->glo->mesh = model->mesh;
			self->glo->mat = model->mat;
			self->glo->updated = 1;
			c_skin_t *skin = c_skin(self);
			if(skin) self->glo->skin = &skin->info;
		}
		else
		{
			self->glo = kh_value(g_glos, k) = calloc(1, sizeof(glg_t));
		}

		self->instance_id = glg_add_instance(self->glo, mat4());
	}

	return CONTINUE;
}

#ifdef MESH4
vec4_t get_cell_normal(mesh_t *mesh, cell_t *cell)
{
	face_t *f = c_face(cell, 0, mesh);

	edge_t *e0 = f_edge(f, 0, mesh);

	vec4_t p0 = f_vert(f, 0, mesh)->pos;
	vec4_t p1 = f_vert(f, 1, mesh)->pos;
	vec4_t p2 = f_vert(f, 2, mesh)->pos;
	vec4_t p3 = e_vert(e_prev(e_pair(e0, mesh), mesh), mesh)->pos;

	vec4_t n = vec4_unit(vec4_cross(
				vec4_sub(p0, p1),
				vec4_sub(p2, p0),
				vec4_sub(p0, p3)));

	return n;

}
#endif

int glg_update_ram(glg_t *self)
{
	mesh_t *mesh = self->mesh;
	glg_clear(self);

	/* mat_layer_t *layer = &model->layers[self->layer_id]; */
	glg_vert_prealloc(self, vector_count(mesh->verts));

	/* int selection = layer->selection; */

	vector_t *faces = mesh->faces;
	vector_t *edges = mesh->edges;

	/* mesh_selection_t *sel = &mesh->selections[selection]; */
	/* if(selection != -1) */
	/* { */
		/* return 1; */
	/* 	faces = sel->faces; */
	/* 	edges = sel->edges; */
	/* } */

	int i;
	if(vector_count(faces))
	{
		mesh_update_smooth_normals(mesh, 1.0f - mesh->smooth_angle);
		int triangle_count = 0;
		for(i = 0; i < vector_count(faces); i++)
		{
			int id = i;
			face_t *face = m_face(mesh, id); if(!face) continue;

			if(face->e_size == 3) triangle_count++;
			else triangle_count += 2;
		}
		glg_ind_prealloc(self, triangle_count * 3);

		for(i = 0; i < vector_count(faces); i++)
		{
			int id = i;
			face_t *face = m_face(mesh, id); if(!face) continue;

#ifdef MESH4

			/* if(face->pair != -1) */
			/* { */
			/* 	cell_t *cell1 = f_cell(f_pair(face, mesh), mesh); */
			/* 	cell_t *cell0 = f_cell(face, mesh); */
			/* 	if(cell0 && cell1) */
			/* 	{ */
			/* 		vec4_t n0 = get_cell_normal(mesh, cell0); */
			/* 		vec4_t n1 = get_cell_normal(mesh, cell1); */
			/* 		float dot = vec4_dot(n1, n0); */
			/* 		if(dot > 0.999) continue; */
			/* 	} */
			/* } */

#endif

			glg_face_to_gl(self, face, id);
		}
		/* if(mesh->has_texcoords) */
		/* { */
		/* 	glg_get_tg_bt(self); */
		/* } */
	}
	else if(vector_count(edges))
	{
		glg_edges_to_gl(self);
	}
	loader_push(g_candle->loader, (loader_cb)glg_update_buffers, self,
			NULL);
	return 1;

}

int c_mesh_gl_updated(c_mesh_gl_t *self)
{
	if(!self->glo) return STOP;
	/* int i; */
	/* for(i = 0; i < self->groups_num; i++) */
	/* { */
		if(!self->glo->updated) return STOP;
	/* } */
	return CONTINUE;
}

static int glg_update_buffers(glg_t *self)
{
	if(!self->vao)
	{
#ifdef USE_VAO
		glGenVertexArrays(1, &self->vao); glerr();
#endif
	}
	glBindVertexArray(self->vao); glerr();

	if(self->vert_num > self->gl_vert_num)
	{
		self->gl_vert_num = self->vert_num;

		/* VERTEX BUFFER */
#ifdef MESH4
		create_buffer(self, 0, self->pos, GL_FLOAT, 4, 0);
#else
		create_buffer(self, 0, self->pos, GL_FLOAT, 3, 0);
#endif

		/* NORMAL BUFFER */
		create_buffer(self, 1, self->nor, GL_FLOAT, 3, 0);

		/* TEXTURE COORDS BUFFER */
		create_buffer(self, 2, self->tex, GL_FLOAT, 2, 0);

		/* TANGENT BUFFER */
		create_buffer(self, 3, self->tan, GL_FLOAT, 3, 0);

		/* ID BUFFER */
		create_buffer(self, 4, self->id, GL_FLOAT, 2, 0);

		/* COLOR BUFFER */
		create_buffer(self, 5, self->col, GL_FLOAT, 3, 0);

		if(self->skin)
		{
			/* BONEID BUFFER */
			create_buffer(self, 7, self->bid, GL_UNSIGNED_INT, 4, 0);

			/* BONE WEIGHT BUFFER */
			create_buffer(self, 8, self->wei, GL_FLOAT, 4, 0);
		}
	}
	if(self->inst_num > self->gl_inst_num)
	{
		self->gl_inst_num = self->inst_num;

		/* TRANSFORM BUFFER */
		create_buffer(self, 9, self->inst, GL_FLOAT, 16, 1);
	}

	if(self->ind_num > self->gl_ind_num)
	{
		int id = 13;
		if(!self->vbo[id])
		{
			glGenBuffers(1, &self->vbo[id]); glerr();
			self->vbo_num++;
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self->vbo[id]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, self->ind_num *
				sizeof(*self->ind), self->ind, GL_STATIC_DRAW);
		glerr();

	}
	self->gl_ind_num = self->ind_num;

	if(self->gl_ind_num)
	{
		glg_update_vbos(self);
	}

	self->updated = 1;
	/* if(!self->ready) */
	/* { */
		/* entity_signal(self->entity, sig("spacial_changed"), NULL, NULL); */
	/* } */
	self->ready = 1;

	glBindVertexArray(0); glerr();

	return 1;
}

void c_mesh_gl_update(c_mesh_gl_t *self)
{
	/* TODO update only dirty group */
	c_model_t *model = c_model(self);

	/* int i; */
	if(!model->mesh || model->mesh->locked_read ||
			self->ram_update_id == model->mesh->update_id) return;

	mesh_lock_write(model->mesh);

	/* for(i = 0; i < model->layers_num; i++) */
	/* { */
		/* if(i >= self->groups_num) mesh_gl_add_group(self); */

		glg_t *group = self->glo;
		if(model->mesh->update_id != group->update_id && group->updated)
		{
			group->updated = 0;
			glg_update_ram(group);
			group->update_id = model->mesh->update_id;

		}

	/* } */
	self->ram_update_id = model->mesh->update_id;
	mesh_unlock_write(model->mesh);
}

int glg_draw(glg_t *self, shader_t *shader, int flags)
{
	mesh_t *mesh = self->mesh;

	if(!mesh || !self->ready)
	{
		return STOP;
	}

	int cull_face;
	int wireframe;
	/* c_model_t *model = c_model(&self->entity); */
	/* mat_layer_t *layer = &model->layers[self->layer_id]; */
	int is_emissive = 0;
	int is_transparent = 0;

	if(self->mat && shader)
	{
		is_transparent = self->mat->transparency.color.a > 0.0f ||
			self->mat->transparency.texture;

		if(flags == 1)
		{
			is_emissive = self->mat->emissive.color.a > 0.0f ||
				self->mat->emissive.texture;
			if(!is_emissive && !is_transparent)
			{
				return CONTINUE;
			}
		}
		else if(flags == 0 && is_transparent)
		{
			return CONTINUE;
		}

		mat_bind(self->mat, shader);
	}

	if(mesh->cull_front)
	{
		if(mesh->cull_back)
		{
			cull_face = GL_FRONT_AND_BACK;
		}
		else
		{
			cull_face = GL_FRONT;
		}
	}
	else if(mesh->cull_back)
	{
		cull_face = GL_BACK;
	}
	else
	{
		cull_face = GL_NONE;
	}

	wireframe = mesh->wireframe;

	/* glPolygonOffset(0.0f, self->layers[self->layer_id].offset); */
	glerr();

	c_renderer_t *renderer = c_renderer(&SYS);
	if(is_emissive)
	{
		if(renderer->depth_inverted)
		{
			glDepthFunc(GL_GEQUAL);
		}
		else
		{
			glDepthFunc(GL_LEQUAL);
		}
	}
	else
	{
		if(renderer->depth_inverted)
		{
			glDepthFunc(GL_GREATER);
		}
		else
		{
			glDepthFunc(GL_LESS);
		}
	}

	int cull_was_enabled = glIsEnabled(GL_CULL_FACE);
	if(cull_face == GL_NONE)
	{
		glDisable(GL_CULL_FACE); glerr();
	}
	else
	{
		glCullFace(cull_face); glerr();
	}

#ifdef USE_VAO

	glBindVertexArray(self->vao); glerr();
	glerr();

#else

	c_mesh_gl_bind_vbo(self); glerr();
	glerr();

#endif

	if(wireframe)
	{
		glLineWidth(2);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glerr();

	if(vector_count(mesh->faces))
	{
		glDrawElements(GL_TRIANGLES, self->gl_ind_num,
				GL_UNSIGNED_INT, 0); glerr();
	}
	else
	{
		glLineWidth(3); glerr();
		/* glLineWidth(5); */
		glDrawElements(GL_LINES, self->gl_ind_num,
				GL_UNSIGNED_INT, 0); glerr();
	}

#ifndef USE_VAO
	int i;
	for(i = 0; i < self->vbo_num; i++)
	{
		glDisableVertexAttribArray(i);
	}
#endif

	glerr();

	if(cull_was_enabled)
	{
		glEnable(GL_CULL_FACE);
	}
	/* printf("/GLG_DRAW\n"); */
	return CONTINUE;

}

int c_mesh_gl_draw_ent(c_mesh_gl_t *self, entity_t ent,
		mat4_t *transform, int transparent)
{
	/* int i; */
	int res = CONTINUE;

	mesh_t *mesh = c_model(self)->mesh;
	if(!mesh)
	{
		return STOP;
	}

	c_mesh_gl_update(self);

	shader_t *shader = c_renderer(&SYS)->shader;
	if(shader)
	{
		glUniform1f(shader->u_has_tex, (float)mesh->has_texcoords);
		vec2_t id_color = entity_to_vec2(ent);

		glUniform2f(shader->u_id, id_color.x, id_color.y);
		glerr();
	}

	if(self->glo->moved)
	{
		self->glo->moved = 0;
		update_buffer(self->glo, 9, self->glo->inst, 16, 1); glerr();
	}

	/* for(i = 0; i < self->groups_num; i++) */
	/* { */
		res &= glg_draw(self->glo, shader, transparent);
	/* } */
	return res;
}

int c_mesh_gl_draw(c_mesh_gl_t *self, mat4_t *transform, int transparent)
{
	return c_mesh_gl_draw_ent(self, c_entity(self), transform, transparent);
}

static void glg_update_vbos(glg_t *self)
{
	/* VERTEX BUFFER */
	int i = 0;
#ifdef MESH4
	update_buffer(self, i, self->pos, 4, 0); glerr();
#else
	update_buffer(self, i, self->pos, 3, 0); glerr();
#endif
	i++;

	/* NORMAL BUFFER */
	update_buffer(self, i, self->nor, 3, 0); glerr();
	i++;

	/* TEXTURE COORDS BUFFER */
	update_buffer(self, i, self->tex, 2, 0); glerr();
	i++;

	/* TANGENT BUFFER */
	update_buffer(self, i, self->tan, 3, 0); glerr();
	i++;

	/* ID BUFFER */
	update_buffer(self, i, self->id, 2, 0); glerr();
	i++;

	/* COLOR BUFFER */
	update_buffer(self, i, self->col, 3, 0); glerr();
	i++;

	if(self->skin)
	{
		/* BONEID BUFFER */
		update_buffer(self, i, self->bid, 4, 0); glerr();
		i++;

		/* BONE WEIGHT BUFFER */
		update_buffer(self, i, self->wei, 4, 0); glerr();
		i++;
	}

	/* TRANSFORM BUFFER */
	update_buffer(self, 9, self->inst, 16, 1); glerr();
	i++;

	/* INDEX BUFFER */
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self->vbo[13]);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
			self->gl_ind_num * sizeof(*self->ind), self->ind);
	glerr();


}

void glg_destroy(glg_t *self)
{
	glDeleteBuffers(self->vbo_num, self->vbo);
#ifdef USE_VAO
	if(self->vao)
	{
		glDeleteVertexArrays(1, &self->vao);
	}
#endif
	if(self->tex) free(self->tex);
	if(self->nor) free(self->nor);
	if(self->pos) free(self->pos);
	if(self->tan) free(self->tan);
	if(self->col) free(self->col);
	if(self->id) free(self->id);
	if(self->bid) free(self->bid);
	if(self->wei) free(self->wei);
	if(self->ind) free(self->ind);

}

int c_mesh_gl_position_changed(c_mesh_gl_t *self)
{
	c_node_t *node = c_node(self);
	c_node_update_model(node);
	if(self->glo)
	{
		/* printf("update position %p\n", self); */
		self->glo->inst[self->instance_id] = node->model;
		self->glo->moved = 1;
	}
	return CONTINUE;
}

void c_mesh_gl_destroy(c_mesh_gl_t *self)
{
	/* int i; */
	/* for(i = 0; i < self->groups_num; i++) */
	/* { */
		glg_destroy(self->glo);
	/* } */
	/* free(self->groups); */
}

REG()
{
	ct_t *ct = ct_new("mesh_gl", sizeof(c_mesh_gl_t), c_mesh_gl_init,
			c_mesh_gl_destroy, 1, ref("node"));

	ct_listener(ct, ENTITY, sig("mesh_changed"), c_mesh_gl_on_mesh_changed);
	ct_listener(ct, ENTITY, sig("node_changed"), c_mesh_gl_position_changed);

	ct_add_interaction(ct, ref("model"));

}
