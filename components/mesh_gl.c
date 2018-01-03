#include "mesh_gl.h"
#include "../loader.h"
#include "model.h"
#include <candle.h>

static int c_mesh_gl_new_loader(c_mesh_gl_t *self);
static int c_mesh_gl_update_buffers(c_mesh_gl_t *self);
static void c_mesh_gl_update_vbos(c_mesh_gl_t *self);

unsigned long ct_mesh_gl;

static void c_mesh_gl_init(c_mesh_gl_t *self)
{
	self->super = component_new(ct_mesh_gl);

	self->update_id = -1;
	self->cull_face = GL_BACK;
}

c_mesh_gl_t *c_mesh_gl_new()
{
	c_mesh_gl_t *self = calloc(1, sizeof *self);
	c_mesh_gl_init(self);

	return self;
}

static int c_mesh_gl_new_loader(c_mesh_gl_t *self)
{
#ifdef USE_VAO
	glGenVertexArrays(1, &self->vao); glerr();
#endif
	self->gl_ind_num = 0;
	self->gl_vert_num = 0;
	self->gl_updated = 1;
	self->update_id = -1;
	self->ready = 0;
	glGenBuffers(7, self->vbo); glerr();

	return 1;
}


#ifdef MESH4
static int c_mesh_gl_get_vert(c_mesh_gl_t *self, vec4_t p, vec3_t n, vec2_t t)
#else
static int c_mesh_gl_get_vert(c_mesh_gl_t *self, vec3_t p, vec3_t n, vec2_t t)
#endif
{
#ifdef OPTIMIZE_DRAW_MESH
	int i;
	for(i = 0; i < self->vert_num; i++)
	{
		if(vec3_equals(self->pos[i], p) &&
				vec3_equals(self->nor[i], n) &&
				vec2_equals(self->tex[i], t)) return i;
	}
#endif
	return -1;
}


static void c_mesh_gl_clear(c_mesh_gl_t *self)
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

void c_mesh_gl_vert_prealloc(c_mesh_gl_t *self, int size)
{
	self->vert_alloc += size;

	self->pos = realloc(self->pos, self->vert_alloc * sizeof(*self->pos));
	self->nor = realloc(self->nor, self->vert_alloc * sizeof(*self->nor));
	self->tex = realloc(self->tex, self->vert_alloc * sizeof(*self->tex));
	self->tan = realloc(self->tan, self->vert_alloc * sizeof(*self->tan));
	self->bit = realloc(self->bit, self->vert_alloc * sizeof(*self->bit));
	self->col = realloc(self->col, self->vert_alloc * sizeof(*self->col));
}

void c_mesh_gl_vert_grow(c_mesh_gl_t *self)
{
	if(self->vert_alloc < self->vert_num)
	{
		self->vert_alloc = self->vert_num;

		c_mesh_gl_vert_prealloc(self, 0);
	}
}

void c_mesh_gl_ind_prealloc(c_mesh_gl_t *self, int size)
{
	self->ind_alloc += size;

	self->ind = realloc(self->ind, self->ind_alloc * sizeof(*self->ind));
}

void c_mesh_gl_ind_grow(c_mesh_gl_t *self)
{
	if(self->ind_alloc < self->ind_num)
	{
		self->ind_alloc = self->ind_num;
		c_mesh_gl_ind_prealloc(self, 0);
	}
}


#ifdef MESH4
static int c_mesh_gl_add_vert(c_mesh_gl_t *self, vec4_t p, vec3_t n, vec2_t t,
		vec4_t c)
#else
static int c_mesh_gl_add_vert(c_mesh_gl_t *self, vec3_t p, vec3_t n, vec2_t t,
		vec4_t c)
#endif
{
	int i = c_mesh_gl_get_vert(self, p, n, t);
	if(i >= 0) return i;

	i = self->vert_num++;
	c_mesh_gl_vert_grow(self);

	self->pos[i] = p;
	self->nor[i] = n;
	self->tex[i] = t;
	self->col[i] = c;

	return i;
}

void c_mesh_gl_add_line(c_mesh_gl_t *self, int v1, int v2)
{
	int i = self->ind_num;

	self->ind_num += 2;
	c_mesh_gl_ind_grow(self);

	self->ind[i + 0] = v1;
	self->ind[i + 1] = v2;
}

void c_mesh_gl_add_triangle(c_mesh_gl_t *self, int v1, int v2, int v3)
{
	int i = self->ind_num;
	self->ind_num += 3;
	c_mesh_gl_ind_grow(self);

	self->ind[i + 0] = v1;
	self->ind[i + 1] = v2;
	self->ind[i + 2] = v3;
}

void c_mesh_gl_edges_to_gl(c_mesh_gl_t *self)
{
	int i;
	mesh_t *mesh = self->mesh;

	c_mesh_gl_ind_prealloc(self, mesh->edges_size * 2);

	for(i = 0; i < mesh->edges_size; i++)
	{
		edge_t *curr_edge = m_edge(mesh, i);
		if(!curr_edge) continue;

		edge_t *next_edge = m_edge(mesh, curr_edge->next);
		if(!next_edge) continue;

		int v1 = c_mesh_gl_add_vert(self, e_vert(curr_edge, mesh)->pos,
				vec3(0.0f), vec2(0.0f), vec4(0.0f));
		int v2 = c_mesh_gl_add_vert(self, e_vert(next_edge, mesh)->pos,
				vec3(0.0f), vec2(0.0f), vec4(0.0f));

		c_mesh_gl_add_line(self, v1, v2);

	}
}

void c_mesh_gl_face_to_gl(c_mesh_gl_t *self, face_t *f)
{
	mesh_t *mesh = self->mesh;
	int v[4], i;
	for(i = 0; i < f->e_size; i++)
	{
		edge_t *hedge = f_edge(f, i, mesh);
		v[i] = c_mesh_gl_add_vert(self, e_vert(hedge, mesh)->pos, hedge->n,
				hedge->t, e_vert(hedge, mesh)->color);
	}
	if(f->e_size == 4)
	{
		c_mesh_gl_add_triangle(self, v[0], v[1], v[2]);
		c_mesh_gl_add_triangle(self, v[2], v[3], v[0]);

	}
	else if(f->e_size == 3)
	{
		c_mesh_gl_add_triangle(self, v[0], v[1], v[2]);
	}
	else
	{
		printf("%d\n", f->e_size);
	}

}

void c_mesh_gl_get_tg_bt(c_mesh_gl_t *self)
{
	mesh_t *mesh = self->mesh;
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
	entity_t ent = c_entity(self);
	self->mesh = c_model(ent)->mesh;
	if(self->vao == 0)
	{
		self->vao = 1;
		loader_push(candle->loader, (loader_cb)c_mesh_gl_new_loader, NULL,
				(c_t*)self);
	}
	return 1;
}

int c_mesh_gl_update_ram(c_mesh_gl_t *self)
{
	int i;
	mesh_t *mesh = self->mesh;
	c_mesh_gl_clear(self);

	c_mesh_gl_vert_prealloc(self, mesh->verts_size);

	if(mesh->faces_size)
	{
		for(i = 0; i < mesh->verts_size; i++)
		{
			vertex_t *v = m_vert(mesh, i);
			if(!v) continue;

			int start = mesh_vert_get_half(mesh, v);

			edge_t *hedge = m_edge(mesh, start);
			if(!hedge) continue;

			vec3_t smooth_normal = hedge->n;

			edge_t *E;
			int e, n;
			int limit = 16;
			for(e = hedge->pair; e >= 0; e = E->pair)
			{
				E = m_edge(mesh, e); if(!E) break;
				n = E->next;
				if(n == start || n < 0) break;
				if(!limit--) break;
				E = m_edge(mesh, n);
				if(fabs(vec3_dot(E->n, hedge->n)) >= mesh->smooth_max)
				{
					smooth_normal = vec3_add(smooth_normal, E->n);
				}
			}
			hedge->n = smooth_normal = vec3_norm(smooth_normal);
			limit = 16;
			for(e = hedge->pair; e >= 0; e = E->pair)
			{
				E = m_edge(mesh, e); if(!E) break;
				n = m_edge(mesh, e)->next;
				if(!limit--) break;
				if(n == start || n < 0) break;
				E = m_edge(mesh, n);
				if(fabs(vec3_dot(E->n, hedge->n)) > mesh->smooth_max)
				{
					E->n = smooth_normal;
				}
			}
		}
		int triangle_count = 0;
		for(i = 0; i < mesh->faces_size; i++)
		{
			if(mesh->faces[i].e_size == 3) triangle_count++;
			else triangle_count += 2;
		}
		c_mesh_gl_ind_prealloc(self, triangle_count * 3);

		for(i = 0; i < mesh->faces_size; i++)
		{
			face_t *face = m_face(mesh, i); if(!face) continue;
			c_mesh_gl_face_to_gl(self, face);
		}
		c_mesh_gl_get_tg_bt(self);
	}
	else
	{
		c_mesh_gl_edges_to_gl(self);
	}
	loader_push(candle->loader, (loader_cb)c_mesh_gl_update_buffers, NULL,
			(c_t*)self);
	return 1;
}

int c_mesh_gl_updated(c_mesh_gl_t *self)
{
	return self->gl_updated;
}

static int c_mesh_gl_update_buffers(c_mesh_gl_t *self)
{
	glBindVertexArray(self->vao); glerr();

	if(self->vert_num > self->gl_vert_num)
	{

		self->gl_vert_num = self->vert_num;

		/* VERTEX BUFFER */
#ifdef MESH4
		create_buffer(0, self->vbo[0], self->pos, 4, self->gl_vert_num);
#else
		create_buffer(0, self->vbo[0], self->pos, 3, self->gl_vert_num);
#endif

		/* NORMAL BUFFER */
		create_buffer(1, self->vbo[1], self->nor, 3, self->gl_vert_num);

		/* TEXTURE COORDS BUFFER */
		create_buffer(2, self->vbo[2], self->tex, 2, self->gl_vert_num);

		/* TANGENT BUFFER */
		create_buffer(3, self->vbo[3], self->tan, 3, self->gl_vert_num);

		/* BITANGENT BUFFER */
		create_buffer(4, self->vbo[4], self->bit, 3, self->gl_vert_num);

		/* COLOR BUFFER */
		create_buffer(5, self->vbo[5], self->col, 4, self->gl_vert_num);
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
		c_mesh_gl_update_vbos(self);
	}

	self->gl_updated = 1;
	self->ready = 1;

	glBindVertexArray(0); glerr();

	return 1;
}

void c_mesh_gl_update(c_mesh_gl_t *self)
{
	if(self->mesh->update_id != self->update_id && self->gl_updated)
	{
		self->gl_updated = 0;
		c_mesh_gl_update_ram(self);
		self->update_id = self->mesh->update_id;
	}
}

int c_mesh_gl_draw(c_mesh_gl_t *self)
{

	mesh_t *mesh = self->mesh;
	if(!mesh)
	{
		return 0;
	}

	c_mesh_gl_update(self);

	if(!self->ready)
	{
		return 0;
	}

	if(!self->cull_face)
	{
		glDisable(GL_CULL_FACE); glerr();
	}
	else
	{
		glCullFace(self->cull_face); glerr();
	}

#ifdef USE_VAO

	glBindVertexArray(self->vao); glerr();
	glerr();

#else

	c_mesh_gl_bind_vbo(self); glerr();
	glerr();

#endif

	if(self->wireframe)
	{
		glLineWidth(1);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glerr();

	if(mesh->faces_size)
	{
		glDrawElements(GL_TRIANGLES, self->gl_ind_num,
				GL_UNSIGNED_INT, 0); glerr();
	}
	else
	{
		glLineWidth(3);
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
	return 1;
}

static void c_mesh_gl_update_vbos(c_mesh_gl_t *self)
{
	/* VERTEX BUFFER */
#ifdef MESH4
	update_buffer(0, self->vbo[0], self->pos, 4, self->gl_vert_num);
#else
	update_buffer(0, self->vbo[0], self->pos, 3, self->gl_vert_num);
#endif

	/* NORMAL BUFFER */
	update_buffer(1, self->vbo[1], self->nor, 3, self->gl_vert_num);

	/* TEXTURE COORDS BUFFER */
	update_buffer(2, self->vbo[2], self->tex, 2, self->gl_vert_num);

	/* TANGENT BUFFER */
	update_buffer(3, self->vbo[3], self->tan, 3, self->gl_vert_num);

	/* BITANGENT BUFFER */
	update_buffer(4, self->vbo[4], self->bit, 3, self->gl_vert_num);

	/* COLOR BUFFER */
	update_buffer(5, self->vbo[5], self->col, 4, self->gl_vert_num);

	/* INDEX BUFFER */
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self->vbo[6]);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
			self->gl_ind_num * sizeof(*self->ind), self->ind);
	glerr();

}


static int c_mesh_gl_destroy_loader(c_mesh_gl_t *self)
{
	glDeleteBuffers(7, self->vbo);
#ifdef USE_VAO
	glDeleteVertexArrays(1, &self->vao);
#endif
	free(self);

	return 1;
}

void c_mesh_gl_destroy(c_mesh_gl_t *self)
{
	if(self->tex) free(self->tex);
	if(self->nor) free(self->nor);
	if(self->pos) free(self->pos);
	if(self->tan) free(self->tan);
	if(self->bit) free(self->bit);
	if(self->col) free(self->col);
	if(self->ind) free(self->ind);

	loader_push(candle->loader, (loader_cb)c_mesh_gl_destroy_loader, NULL,
			(c_t*)self);
}

void c_mesh_gl_register(ecm_t *ecm)
{
	ct_t *ct = ecm_register(ecm, &ct_mesh_gl, sizeof(c_mesh_gl_t),
			(init_cb)c_mesh_gl_init, 0);
	ct_register_listener(ct, WORLD, mesh_changed,
			(signal_cb)c_mesh_gl_on_mesh_changed);

	ct_add_dependency(ecm_get(ecm, ct_model), ct_mesh_gl);

	/* ct_register_listener(ct, WORLD, collider_callback, c_grid_collider); */
}
