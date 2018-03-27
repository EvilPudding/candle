#include "mesh_gl.h"
#include "../loader.h"
#include "model.h"
#include <candle.h>
#include <components/spacial.h>
#include <string.h>
#include <stdlib.h>

static int c_mesh_gl_new_loader(c_mesh_gl_t *self);
static int glg_update_buffers(glg_t *self);
static void glg_update_vbos(glg_t *self);


static void c_mesh_gl_init(c_mesh_gl_t *self)
{

	self->groups = calloc(8, sizeof(*self->groups));
}

c_mesh_gl_t *c_mesh_gl_new()
{
	c_mesh_gl_t *self = component_new(ct_mesh_gl);

	return self;
}

static void mesh_gl_add_group(c_mesh_gl_t *self)
{
	int i = self->groups_num++;
	glg_t *group = &self->groups[i];

#ifdef USE_VAO
	glGenVertexArrays(1, &group->vao); glerr();
#endif
	group->gl_ind_num = 0;
	group->gl_vert_num = 0;
	group->ready = 0;
	glGenBuffers(7, group->vbo); glerr();
	group->update_id = -1;
	group->updated = 1;
	group->entity = c_entity(self);
	group->layer_id = i;
}

static int c_mesh_gl_new_loader(c_mesh_gl_t *self)
{
	/* mesh_gl_add_group(self); */

	return 1;
}


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
	self->bit = realloc(self->bit, self->vert_alloc * sizeof(*self->bit));
	/* self->col = realloc(self->col, self->vert_alloc * sizeof(*self->col)); */
	self->id = realloc(self->id, self->vert_alloc * sizeof(*self->id));
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


#ifdef MESH4
static int glg_add_vert(glg_t *self, vec4_t p, vec3_t n, vec2_t t, int id)
#else
static int glg_add_vert(glg_t *self, vec3_t p, vec3_t n, vec2_t t, int id)
#endif
{
	/* int i = glg_get_vert(self, p, n, t); */
	/* if(i >= 0) return i; */
	int i;

	i = self->vert_num++;
	glg_vert_grow(self);

	self->pos[i] = p;
	self->nor[i] = n;
	self->tex[i] = t;
	/* self->col[i] = c; */
	self->id[i] = int_to_vec2(id);


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
	mesh_t *mesh = c_model(&self->entity)->mesh;

	glg_ind_prealloc(self, vector_count(mesh->edges) * 2);

	for(i = 0; i < vector_count(mesh->edges); i++)
	{
		edge_t *curr_edge = m_edge(mesh, i);
		if(!curr_edge) continue;

		edge_t *next_edge = m_edge(mesh, curr_edge->next);
		if(!next_edge) continue;

		int v1 = glg_add_vert(self, e_vert(curr_edge, mesh)->pos,
				vec3(0.0f), vec2(0.0f), 0);
		int v2 = glg_add_vert(self, e_vert(next_edge, mesh)->pos,
				vec3(0.0f), vec2(0.0f), 0);

		glg_add_line(self, v1, v2);

	}
}

void glg_face_to_gl(glg_t *self, face_t *f, int id)
{
	mesh_t *mesh = c_model(&self->entity)->mesh;
	int v[4], i;
	for(i = 0; i < f->e_size; i++)
	{
		edge_t *hedge = f_edge(f, i, mesh);
		/* vec4_t color = e_vert(hedge, mesh)->color; */
		v[i] = glg_add_vert(self, e_vert(hedge, mesh)->pos, hedge->n,
				hedge->t, id);
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

void glg_get_tg_bt(glg_t *self)
{
	mesh_t *mesh = c_model(&self->entity)->mesh;
	int i, a;
	/* if(self->bit) free(self->bit); */
	/* if(self->tan) free(self->tan); */
	/* self->bit = calloc(3 * self->vert_num, sizeof(*self->bit)); */
	/* self->tan = calloc(3 * self->vert_num, sizeof(*self->tan)); */

	vec3_t *tan1 = calloc(self->vert_num * 2, sizeof(*tan1));
	vec3_t *tan2 = tan1 + self->vert_num;

	if(!mesh->has_texcoords)
	{
		for(a = 0; a < self->vert_num; a++)
		{
			vec3_t n = self->nor[a];

			vec3_t c1 = vec3_cross(n, vec3(0.0, 0.0, 1.0)); 
			vec3_t c2 = vec3_cross(n, vec3(0.0, 1.0, 0.0)); 

			if(vec3_len(c1) > vec3_len(c2))
			{
				tan1[a] = vec3_norm(c1);
			}
			else
			{
				tan1[a] = vec3_norm(c2);
			}
		}
	}
	else
	{
		for(i = 0; i < self->ind_num; i += 3)
		{
			int i1 = self->ind[i + 0];
			int i2 = self->ind[i + 1];
			int i3 = self->ind[i + 2];

			vec3_t v1 = ((vec3_t*)self->pos)[i1];
			vec3_t v2 = ((vec3_t*)self->pos)[i2];
			vec3_t v3 = ((vec3_t*)self->pos)[i3];

			vec2_t w1 = ((vec2_t*)self->tex)[i1];
			vec2_t w2 = ((vec2_t*)self->tex)[i2];
			vec2_t w3 = ((vec2_t*)self->tex)[i3];

			float x1 = v2.x - v1.x;
			float x2 = v3.x - v1.x;
			float y1 = v2.y - v1.y;
			float y2 = v3.y - v1.y;
			float z1 = v2.z - v1.z;
			float z2 = v3.z - v1.z;

			float s1 = w2.x - w1.x;
			float s2 = w3.x - w1.x;
			float t1 = w2.y - w1.y;
			float t2 = w3.y - w1.y;

			float r = 1.0F / (s1 * t2 - s2 * t1);
			vec3_t sdir = vec3((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
					(t2 * z1 - t1 * z2) * r);
			vec3_t tdir = vec3((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
					(s1 * z2 - s2 * z1) * r);

			tan1[i1] = vec3_add(tan1[i1], sdir);
			tan1[i2] = vec3_add(tan1[i2], sdir);
			tan1[i3] = vec3_add(tan1[i3], sdir);

			tan2[i1] = vec3_add(tan2[i1], tdir);
			tan2[i2] = vec3_add(tan2[i2], tdir);
			tan2[i3] = vec3_add(tan2[i3], tdir);
		}
	}
	for(a = 0; a < self->vert_num; a++)
	{
		vec3_t *n = &((vec3_t*)self->nor)[a];
		vec3_t *t = &tan1[a];

		// Gram-Schmidt orthogonalize
		vec3_t tangent = vec3_norm(
				vec3_sub(*t, vec3_scale(*n, vec3_dot(*n, *t))));

		self->tan[a] = tangent;

		self->bit[a] = vec3_cross(tangent, *n);

		// Calculate handedness
		/* mesh->tan[a].w = (vec3_dot(vec3_mull_cross(n, t), tan2[a]) < 0.0F) ? -1.0F : 1.0F; */
	}

	free(tan1);
}

static inline void create_buffer(int id, GLuint vbo, void *arr, int dim, size_t size)
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo); glerr();
	glBufferData(GL_ARRAY_BUFFER, dim * sizeof(GLfloat) * size, arr,
			GL_STATIC_DRAW); glerr();
	glEnableVertexAttribArray(id); glerr();
	glVertexAttribPointer(id, dim, GL_FLOAT, GL_FALSE, 0, NULL); glerr();
}

static inline void update_buffer(int id, GLuint vbo, void *arr, int dim, size_t size)
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo); glerr();
	glBufferSubData(GL_ARRAY_BUFFER, 0, dim * sizeof(GLfloat) * size, arr); glerr();
}


int c_mesh_gl_on_mesh_changed(c_mesh_gl_t *self)
{
	self->mesh = c_model(self)->mesh;

	/* TODO strange code */
	if(self->groups[0].vao == 0)
	{
		self->groups[0].vao = 1;
		loader_push(candle->loader, (loader_cb)c_mesh_gl_new_loader, NULL,
				(c_t*)self);
	}

	int i;
	for(i = 0; i < self->groups_num; i++)
	{
		glg_t *group = &self->groups[i];
		group->update_id = -1;
	}

	return 1;
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
	c_model_t *model = c_model(&self->entity);
	mesh_t *mesh = model->mesh;
	glg_clear(self);

	glg_vert_prealloc(self, vector_count(mesh->verts));

	int selection = model->layers[self->layer_id].selection;

	vector_t *faces = mesh->faces;
	vector_t *edges = mesh->edges;

	mesh_selection_t *sel = &mesh->selections[selection];
	if(selection != -1) faces = sel->faces;
	if(selection != -1) edges = sel->edges;
	/* if(sel->visible == 0) return 1; */

	int i;
	if(vector_count(faces))
	{
		mesh_update_smooth_normals(mesh);
		int triangle_count = 0;
		for(i = 0; i < vector_count(faces); i++)
		{
			int id = i;
			if(selection != -1) id = vector_value(faces, i, int);
			face_t *face = m_face(mesh, id); if(!face) continue;

			if(face->e_size == 3) triangle_count++;
			else triangle_count += 2;
		}
		glg_ind_prealloc(self, triangle_count * 3);

		for(i = 0; i < vector_count(faces); i++)
		{
			int id = i;
			if(selection != -1) id = vector_value(faces, i, int);
			face_t *face = m_face(mesh, id); if(!face) continue;
#ifdef MESH4
			if(face->pair != -1)
			{
				cell_t *cell1 = f_cell(f_pair(face, mesh), mesh);
				cell_t *cell0 = f_cell(face, mesh);
				if(cell0 && cell1)
				{
					vec4_t n0 = get_cell_normal(mesh, cell0);
					vec4_t n1 = get_cell_normal(mesh, cell1);
					float dot = vec4_dot(n1, n0);
					if(dot > 0.999) continue;
				}
			}
#endif
			glg_face_to_gl(self, face, id);
		}
		glg_get_tg_bt(self);
	}
	else if(vector_count(edges))
	{
		glg_edges_to_gl(self);
	}
	loader_push(candle->loader, (loader_cb)glg_update_buffers, self,
			NULL);
	return 1;

}

int c_mesh_gl_updated(c_mesh_gl_t *self)
{
	int i;
	for(i = 0; i < self->groups_num; i++)
	{
		if(!self->groups[i].updated) return 0;
	}
	return 1;
}

static int glg_update_buffers(glg_t *self)
{
	int i = 0;
	glBindVertexArray(self->vao); glerr();

	if(self->vert_num > self->gl_vert_num)
	{

		self->gl_vert_num = self->vert_num;

		/* VERTEX BUFFER */
#ifdef MESH4
		create_buffer(i, self->vbo[i], self->pos, 4, self->gl_vert_num);
#else
		create_buffer(i, self->vbo[i], self->pos, 3, self->gl_vert_num);
#endif
		i++;

		/* NORMAL BUFFER */
		create_buffer(i, self->vbo[i], self->nor, 3, self->gl_vert_num);
		i++;

		/* TEXTURE COORDS BUFFER */
		create_buffer(i, self->vbo[i], self->tex, 2, self->gl_vert_num);
		i++;

		/* TANGENT BUFFER */
		create_buffer(i, self->vbo[i], self->tan, 3, self->gl_vert_num);
		i++;

		/* BITANGENT BUFFER */
		create_buffer(i, self->vbo[i], self->bit, 3, self->gl_vert_num);
		i++;

		/* COLOR BUFFER */
		/* create_buffer(i, self->vbo[i], self->col, 4, self->gl_vert_num); */
		/* i++; */

		create_buffer(i, self->vbo[i], self->id, 2, self->gl_vert_num);
		i++;
	}

	if(self->ind_num > self->gl_ind_num)
	{

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self->vbo[6]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, self->ind_num *
				sizeof(*self->ind), self->ind, GL_STATIC_DRAW);

	}

	self->gl_ind_num = self->ind_num;

	if(self->gl_ind_num)
	{
		glg_update_vbos(self);
	}

	self->updated = 1;
	/* if(!self->ready) */
	{
		entity_signal(self->entity, spacial_changed, NULL);
	}
	self->ready = 1;

	glBindVertexArray(0); glerr();

	return 1;
}

void c_mesh_gl_update(c_mesh_gl_t *self)
{
	/* TODO update only dirty group */
	if(!self->mesh) return;
	int i;
	/* if(self->mesh->update_locked) return; */
	if(self->mesh->mid_load) return;
	SDL_SemWait(self->mesh->sem);

	c_model_t *model = c_model(self);

	for(i = 0; i < model->layers_num; i++)
	{
		if(i >= self->groups_num) mesh_gl_add_group(self);

		glg_t *group = &self->groups[i];
		if(self->mesh->update_id != group->update_id && group->updated)
		{
			group->updated = 0;
			glg_update_ram(group);
			group->update_id = self->mesh->update_id;

		}

	}
	SDL_SemPost(self->mesh->sem);
}

int glg_draw(glg_t *self, shader_t *shader, int transparent)
{
	mesh_t *mesh = c_model(&self->entity)->mesh;

	if(!mesh)
	{
		return 0;
	}
	if(!self->ready)
	{
		return 0;
	}

	int cull_face;
	int wireframe;
	c_model_t *model = c_model(&self->entity);
	mat_layer_t *layer = &model->layers[self->layer_id];

	if(layer->mat && shader)
	{
		if((layer->mat->transparency.color.a != 0.0f) != transparent) return 0;

		mat_bind(layer->mat, shader);
	}
	/* printf("GLG_DRAW\n"); */

	{
		if(layer->cull_front)
		{
			if(layer->cull_back)
			{
				cull_face = GL_FRONT_AND_BACK;
			}
			else
			{
				cull_face = GL_FRONT;
			}
		}
		else if(layer->cull_back)
		{
			cull_face = GL_BACK;
		}
		else
		{
			cull_face = GL_NONE;
		}

		wireframe = layer->wireframe;
	}

	/* glPolygonOffset(0.0f, model->layers[self->layer_id].offset); */
	glerr();

	if(self->layer_id)
	{
		glDepthFunc(GL_LEQUAL);
	}
	else
	{
		glPolygonOffset(0.0f, 0.0f);
	}

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
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
#endif

	glerr();
	glEnable(GL_CULL_FACE);
	/* printf("/GLG_DRAW\n"); */
	return 1;

}

int c_mesh_gl_draw(c_mesh_gl_t *self, int transparent)
{
	int i;
	int res = 1;
		glerr();

	if(!self->mesh)
	{
		return 0;
	}

	c_mesh_gl_update(self);

	shader_t *shader = c_renderer(&candle->systems)->shader;
	if(shader)
	{
		glUniform1f(shader->u_has_tex, (float)self->mesh->has_texcoords);
		vec2_t id_color = int_to_vec2(c_entity(self));

		glUniform2f(shader->u_id, id_color.x, id_color.y);
		glerr();
	}

	for(i = 0; i < self->groups_num; i++)
	{
		res |= glg_draw(&self->groups[i], shader, transparent);
	}
	return res;
}

static void glg_update_vbos(glg_t *self)
{
	/* VERTEX BUFFER */
	int i = 0;
#ifdef MESH4
	update_buffer(i, self->vbo[i], self->pos, 4, self->gl_vert_num);
#else
	update_buffer(i, self->vbo[i], self->pos, 3, self->gl_vert_num);
#endif
	i++;

	/* NORMAL BUFFER */
	update_buffer(i, self->vbo[i], self->nor, 3, self->gl_vert_num);

	i++;

	/* TEXTURE COORDS BUFFER */
	update_buffer(i, self->vbo[i], self->tex, 2, self->gl_vert_num);

	i++;

	/* TANGENT BUFFER */
	update_buffer(i, self->vbo[i], self->tan, 3, self->gl_vert_num);
	i++;

	/* BITANGENT BUFFER */
	update_buffer(i, self->vbo[i], self->bit, 3, self->gl_vert_num);

	i++;

	/* COLOR BUFFER */
	/* update_buffer(i, self->vbo[i], self->col, 4, self->gl_vert_num); */
	/* i++; */

	/* ID BUFFER */
	update_buffer(i, self->vbo[i], self->id, 2, self->gl_vert_num);
	i++;

	/* INDEX BUFFER */
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self->vbo[i]);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
			self->gl_ind_num * sizeof(*self->ind), self->ind);
	glerr();

}

static void glg_destroy_loader(glg_t *self)
{
	glDeleteBuffers(7, self->vbo);
#ifdef USE_VAO
	glDeleteVertexArrays(1, &self->vao);
#endif

}

static int c_mesh_gl_destroy_loader(c_mesh_gl_t *self)
{
	int i;
	for(i = 0; i < self->groups_num; i++)
	{
		glg_destroy_loader(&self->groups[i]);
	}	
	free(self->groups);
	free(self);

	return 1;
}

void glg_destroy(glg_t *self)
{
	if(self->tex) free(self->tex);
	if(self->nor) free(self->nor);
	if(self->pos) free(self->pos);
	if(self->tan) free(self->tan);
	if(self->bit) free(self->bit);
	/* if(self->col) free(self->col); */
	if(self->id) free(self->id);
	if(self->ind) free(self->ind);

}

void c_mesh_gl_destroy(c_mesh_gl_t *self)
{
	int i;
	for(i = 0; i < self->groups_num; i++)
	{
		glg_destroy(&self->groups[i]);
	}

	loader_push(candle->loader, (loader_cb)c_mesh_gl_destroy_loader, NULL,
			(c_t*)self);
}

DEC_CT(ct_mesh_gl)
{
	ct_t *ct = ct_new("c_mesh_gl", &ct_mesh_gl,
			sizeof(c_mesh_gl_t), (init_cb)c_mesh_gl_init, 0);

	ct_listener(ct, ENTITY, mesh_changed, c_mesh_gl_on_mesh_changed);

	ct_add_interaction(ct, ct_model);

}
