#include "drawable.h"
#include <utils/loader.h>
#include <candle.h>
#include <string.h>
#include <stdlib.h>
#include <systems/render_device.h>

KHASH_MAP_INIT_INT(varray, varray_t*)

static khash_t(varray) *g_varrays;

static int32_t draw_conf_add_instance(draw_conf_t *self, drawable_t *draw,
		uint32_t gid);
static void draw_conf_update_inst(draw_conf_t *self, int32_t id);
static int32_t drawable_position_changed(drawable_t *self, struct draw_bind *bind);
static draw_group_t *get_group(uint32_t ref);
static void draw_conf_remove_instance(draw_conf_t *self, int32_t id);
SDL_sem *g_group_semaphore;

#define MASK_TRANS (1 << 0)
#define MASK_PROPS (1 << 1)

KHASH_MAP_INIT_INT(draw_group, draw_group_t)
khash_t(draw_group) *g_draw_groups;

void drawable_init(drawable_t *self, uint32_t group)
{
	self->transform = mat4();
	self->mesh = NULL;
	self->skin = NULL;
	self->custom_texture = NULL;
	self->xray = false;
	self->mat = NULL;
	self->entity = entity_null;
	self->draw_callback = NULL;
	self->usrptr = NULL;
	self->vs = NULL;
	self->bind_num = 0;

	drawable_add_group(self, group);
}

static draw_group_t *get_group(uint32_t ref)
{
	int32_t ret;
	draw_group_t *bind;
	khiter_t k;

	if (ref == 0) return NULL;

	SDL_SemWait(g_group_semaphore);
	if (!g_draw_groups) g_draw_groups = kh_init(draw_group);
	k = kh_get(draw_group, g_draw_groups, ref);

	if (k == kh_end(g_draw_groups))
	{
		k = kh_put(draw_group, g_draw_groups, ref, &ret);
		bind = &kh_value(g_draw_groups, k);
		bind->configs = kh_init(config);
		bind->update_id = 0;
	}
	else
	{
		bind = &kh_value(g_draw_groups, k);
	}
	SDL_SemPost(g_group_semaphore);
	return bind;
}

static void varray_clear(varray_t *self)
{
	self->vert_num = 0;
	self->ind_num = 0;
}

void varray_vert_prealloc(varray_t *self, int32_t size)
{
	self->vert_alloc += size;

	self->pos = realloc(self->pos, self->vert_alloc * sizeof(*self->pos));
	self->nor = realloc(self->nor, self->vert_alloc * sizeof(*self->nor));
	self->tex = realloc(self->tex, self->vert_alloc * sizeof(*self->tex));
	self->tan = realloc(self->tan, self->vert_alloc * sizeof(*self->tan));
	self->col = realloc(self->col, self->vert_alloc * sizeof(*self->col));
	/* if (self->mesh->has_skin) */
	{
		self->wei = realloc(self->wei, self->vert_alloc * sizeof(*self->wei));
		self->bid = realloc(self->bid, self->vert_alloc * sizeof(*self->bid));
	}
	self->id = realloc(self->id, self->vert_alloc * sizeof(*self->id));
}

void draw_conf_inst_prealloc(draw_conf_t *self, int32_t size)
{
	self->inst_alloc += size;

	self->inst = realloc(self->inst, self->inst_alloc * sizeof(*self->inst));
	self->props = realloc(self->props, self->inst_alloc * sizeof(*self->props));
	/* memset(self->props + (self->inst_alloc - size), 0, size * sizeof(*self->props)); */

#ifdef MESH4
	self->angle4 = realloc(self->angle4, self->inst_alloc *
			sizeof(*self->angle4));
#endif

	self->comps = realloc(self->comps, self->inst_alloc * sizeof(*self->comps));
}
void draw_conf_inst_grow(draw_conf_t *self)
{
	if (self->inst_alloc < self->inst_num)
	{
		self->inst_alloc = self->inst_num;

		draw_conf_inst_prealloc(self, 0);
	}
}

void varray_vert_grow(varray_t *self)
{
	if (self->vert_alloc < self->vert_num)
	{
		self->vert_alloc = self->vert_num;

		varray_vert_prealloc(self, 0);
	}
}

void varray_ind_prealloc(varray_t *self, int32_t size)
{
	self->ind_alloc += size;

	self->ind = realloc(self->ind, self->ind_alloc * sizeof(*self->ind));
}

void varray_ind_grow(varray_t *self)
{
	if (self->ind_alloc < self->ind_num)
	{
		self->ind_alloc = self->ind_num;
		varray_ind_prealloc(self, 0);
	}
}

void drawable_remove_group(drawable_t *self, uint32_t group)
{
	uint32_t gid;
	if (group == 0) return;

	for(gid = 0; gid < self->bind_num; gid++)
	{
		if (self->bind[gid].grp == group) break;
	}
	if (gid == self->bind_num)
	{
		return;
	}

	if (self->bind[gid].conf)
	{
		if (self->bind[gid].grp == 0)
		{
			free(self->bind[gid].conf);
		}
		else
		{
			draw_conf_remove_instance(self->bind[gid].conf,
					self->bind[gid].instance_id);
		}
	}
	self->bind_num--;
	if (gid < self->bind_num)
	{
		struct draw_bind *last = &self->bind[self->bind_num];
		if (last->conf)
		{
			last->conf->comps[last->instance_id] = &self->bind[gid];
		}
		self->bind[gid] = *last;
	}
	if (self->bind_num == 0)
	{
		drawable_add_group(self, 0);
	}

	drawable_model_changed(self);
}

void drawable_add_group(drawable_t *self, uint32_t group)
{
	uint32_t gid;
	if (group == 0 && self->bind_num > 0) return;

	for(gid = 0; gid < self->bind_num; gid++)
	{
		if (self->bind[gid].grp == group) return;
	}
	if (self->bind[0].grp == 0)
	{
		if (self->bind[0].conf) free(self->bind[0].conf);
		self->bind_num = 0;
		gid = 0;
	}

	self->bind[gid].grp = group;
	self->bind[gid].instance_id = 0;
	self->bind[gid].conf = NULL;
	self->bind[gid].updates = 0;
	self->bind_num++;
	drawable_model_changed(self);
}

void drawable_set_group(drawable_t *self, uint32_t group)
{
	uint32_t gid = 0;

	if (self->bind[gid].grp == group) return;

	if (group == 0 && self->bind_num > 0)
	{
		drawable_remove_group(self, self->bind[gid].grp);
		return;
	}

	if (self->bind[gid].conf)
	{
		if (self->bind[gid].grp == 0)
		{
			free(self->bind[gid].conf);
		}
		else
		{
			draw_conf_remove_instance(self->bind[gid].conf,
					self->bind[gid].instance_id);
		}
	}

	self->bind[gid].grp = group;
	self->bind[gid].instance_id = 0;
	self->bind[gid].conf = NULL;
	self->bind[gid].updates = 0;

	drawable_model_changed(self);
}

void drawable_set_texture(drawable_t *self, texture_t *texture)
{
	if (texture != self->custom_texture)
	{
		self->custom_texture = texture;
		drawable_model_changed(self);
	}
}

void drawable_set_skin(drawable_t *self, skin_t *skin)
{
	if (skin != self->skin)
	{
		self->skin = skin;
		drawable_model_changed(self);
	}
}

void drawable_set_mesh(drawable_t *self, mesh_t *mesh)
{
	if (mesh != self->mesh)
	{
		mesh_t *previous = self->mesh;
		self->mesh = mesh;
		drawable_model_changed(self);
		if (mesh)
		{
			mesh->ref_num++;
			if (!previous)
			{
				uint32_t gid;
				for(gid = 0; gid < self->bind_num; gid++)
				{
					drawable_position_changed(self, &self->bind[gid]);
				}
			}
		}
		if (previous)
		{
			previous->ref_num++;
			if (previous->ref_num == 0)
			{
				mesh_destroy(previous);
			}
		}
	}
}

void drawable_set_vs(drawable_t *self, vs_t *vs)
{
	if (self->vs != vs)
	{
		self->vs = vs;
		drawable_model_changed(self);
	}
}

void drawable_set_xray(drawable_t *self, int32_t xray)
{
	if (self->xray != xray)
	{
		self->xray = xray;
		drawable_model_changed(self);
	}
}

void drawable_set_matid(drawable_t *self, uint32_t matid)
{
	if (matid != self->matid || self->mat)
	{
		self->matid = matid;
		self->mat = NULL;
		drawable_model_changed(self);
	}
}

void drawable_set_mat(drawable_t *self, mat_t *mat)
{
	if (self->mat != mat || self->matid != mat->id)
	{
		self->mat = mat;
		self->matid = mat->id;
		drawable_model_changed(self);
	}
}

#ifdef MESH4
void drawable_set_angle4(drawable_t *self, float angle4)
{
	uint32_t gid;

	self->angle4 = angle4;
	for(gid = 0; gid < self->bind_num; gid++)
	{
		drawable_position_changed(self, &self->bind[gid]);
	}
}
#endif

void drawable_set_transform(drawable_t *self, mat4_t transform)
{
	uint32_t gid;

	self->transform = transform;
	for(gid = 0; gid < self->bind_num; gid++)
	{
		drawable_position_changed(self, &self->bind[gid]);
	}
}

void drawable_set_callback(drawable_t *self, draw_cb cb, void *usrptr)
{
	if (self->draw_callback == NULL || self->usrptr != usrptr)
	{
		self->draw_callback = cb;
		self->usrptr = usrptr;
		drawable_model_changed(self);
	}
	else
	{
		self->draw_callback = cb;
		self->usrptr = usrptr;
	}
}

void drawable_set_entity(drawable_t *self, entity_t entity)
{
	if (self->entity != entity)
	{
		self->entity = entity;
		drawable_model_changed(self);
	}
}

static int32_t draw_conf_add_instance(draw_conf_t *self, drawable_t *draw,
		uint32_t gid)
{
	int32_t i;
	uvec2_t ent;

#ifndef __EMSCRIPTEN__
	SDL_SemWait(self->semaphore);
#endif

	i = self->inst_num++;
	/* if (draw->bind[gid].bind == ref("light")) */
		/* printf("adding		%d\n", self->inst_num); */

	draw_conf_inst_grow(self);

	self->props[i].x = draw->matid;
	self->props[i].y = 0;/*TODO: use y for something;*/
	ent = entity_to_uvec2(draw->entity);
	ZW(self->props[i]) = vec2(_vec2(ent));
	self->comps[i] = &draw->bind[gid];
	self->inst[i] = draw->transform;

	draw->bind[gid].instance_id = i;
	draw->bind[gid].conf = self;
	draw->bind[gid].updates = MASK_TRANS | MASK_PROPS;

	self->trans_updates++;
	self->props_updates++;

#ifndef __EMSCRIPTEN__
	SDL_SemPost(self->semaphore);
#endif
	return i;
}

static int32_t varray_add_vert(
		varray_t *self,
		vecN_t p,
		vec3_t n,
		vec2_t t,
		vec3_t tg,
		vec3_t c,
		int32_t id,
		vec4_t wei,
		vec4_t bid)
{
	/* int32_t i = draw_conf_get_vert(self, p, n, t); */
	/* if (i >= 0) return i; */
	int32_t i = self->vert_num++;
	varray_vert_grow(self);

	self->pos[i] = p;
	self->nor[i] = n;
	self->tan[i] = tg;
	self->tex[i] = t;
	self->col[i] = c;
	self->id[i] = int_to_vec2(id);

	/* if (self->mesh->has_skin) */
	{
		self->bid[i] = bid;
		self->wei[i] = wei;
	}

	return i;
}

void varray_add_line(varray_t *self, vertid_t v1, vertid_t v2)
{
	int32_t i = self->ind_num;

	self->ind_num += 2;
	varray_ind_grow(self);

	memcpy(&self->ind[i + 0], &v1, sizeof(*self->ind));
	memcpy(&self->ind[i + 1], &v2, sizeof(*self->ind));
	/* self->ind[i + 0] = v1; */
	/* self->ind[i + 1] = v2; */
}

void varray_add_point(varray_t *self, vertid_t v1)
{
	int32_t i = self->ind_num;

	self->ind_num += 1;
	varray_ind_grow(self);

	memcpy(&self->ind[i], &v1, sizeof(*self->ind));
	/* self->ind[i + 0] = v1; */
}

void varray_add_triangle(varray_t *self, vertid_t v1, vertid_t v2, vertid_t v3)
{
	int32_t i = self->ind_num;
	self->ind_num += 3;
	varray_ind_grow(self);

	memcpy(&self->ind[i + 0], &v1, sizeof(*self->ind));
	memcpy(&self->ind[i + 1], &v2, sizeof(*self->ind));
	memcpy(&self->ind[i + 2], &v3, sizeof(*self->ind));
	/* self->ind[i + 0] = v1; */
	/* self->ind[i + 1] = v2; */
	/* self->ind[i + 2] = v3; */

	/* vec2_print(self->id[v1]); */
	/* vec2_print(self->id[v2]); */
	/* vec2_print(self->id[v3]); */
	/* printf("\n"); */
}

void varray_edges_to_gl(varray_t *self)
{
	int32_t i;
	mesh_t *mesh = self->mesh;

	varray_ind_prealloc(self, vector_count(mesh->edges) * 2);

	for(i = 0; i < vector_count(mesh->edges); i++)
	{
		edge_t *next_edge;
		edge_t *curr_edge = m_edge(mesh, i);
		int32_t v1, v2;
		vertex_t *V1, *V2;

		if (!curr_edge) continue;

		next_edge = m_edge(mesh, curr_edge->next);
		if (!next_edge) continue;

		V1 = e_vert(curr_edge, mesh);
		V2 = e_vert(next_edge, mesh);
		v1 = varray_add_vert(self, V1->pos, Z3, Z2, Z3,
		                     vec4_xyz(V1->color), 0, Z4, Z4);
		v2 = varray_add_vert(self, V2->pos, Z3, Z2, Z3,
		                     vec4_xyz(V2->color), 0, Z4, Z4);

		varray_add_line(self, v1, v2);

	}
}

void varray_verts_to_gl(varray_t *self)
{
	int32_t i;
	mesh_t *mesh = self->mesh;

	varray_ind_prealloc(self, vector_count(mesh->verts));

	for(i = 0; i < vector_count(mesh->verts); i++)
	{
		int32_t v1;
		vertex_t *curr_vert = m_vert(mesh, i);
		if (!curr_vert) continue;

		v1 = varray_add_vert(self, curr_vert->pos, Z3, Z2, Z3,
		                     vec4_xyz(curr_vert->color), 0, Z4, Z4);

		varray_add_point(self, v1);
	}
}


void varray_face_to_gl(varray_t *self, face_t *f, int32_t id)
{
	mesh_t *mesh = self->mesh;
	int32_t v[4], i;
	for(i = 0; i < f->e_size; i++)
	{
		edge_t *hedge = f_edge(f, i, mesh);
		vertex_t *V = e_vert(hedge, mesh);

		v[i] = varray_add_vert(self, V->pos, hedge->n,
				hedge->t, hedge->tg, vec4_xyz(V->color), id, V->wei, V->bid);
	}
	if (f->e_size == 4)
	{
		varray_add_triangle(self, v[0], v[1], v[2]);
		varray_add_triangle(self, v[2], v[3], v[0]);

	}
	else if (f->e_size == 3)
	{
		varray_add_triangle(self, v[0], v[1], v[2]);
	}
	else
	{
		printf("%d\n", f->e_size);
	}

}

static void bind_buffer(uint32_t *vbo, int32_t id, uint32_t type, int32_t dim,
		int32_t instanced_divisor)
{
	int32_t j = 0;
	glBindBuffer(GL_ARRAY_BUFFER, *vbo); glerr();

	while(j < dim)
	{
		int step = (dim - j < 4) ? dim - j : 4;

		const void *offset = (const void*)(sizeof(GLfloat) * j);
		uint64_t size = dim * sizeof(GLfloat);

		glEnableVertexAttribArray(id); glerr();

		/* if (type == GL_FLOAT) */
		{
			glVertexAttribPointer(id, step, type, GL_FALSE, size, offset);
		}
		/* else */
		/* { */
			/* glVertexAttribIPointer(id, step, type, size, offset); */
		/* } */

		glVertexAttribDivisor(id, instanced_divisor); glerr();

		id++;
		j += step;
	}
}

static void create_buffer(uint32_t *vbo, void *arr,
		int32_t dim, int32_t count, int32_t instanced_divisor)
{
	uint32_t usage = instanced_divisor ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
	if (!*vbo)
	{
		glGenBuffers(1, vbo); glerr();
	}
	glBindBuffer(GL_ARRAY_BUFFER, *vbo); glerr();
	glBufferData(GL_ARRAY_BUFFER, dim * sizeof(GLfloat) * count, arr, usage);
	glerr();


}

static void update_buffer(uint32_t *vbo, void *arr, int32_t dim,
		int32_t count, int32_t inst)
{
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferSubData(GL_ARRAY_BUFFER, inst * dim * sizeof(GLfloat),
			dim * sizeof(GLfloat) * count, arr);
}


static void draw_conf_remove_instance(draw_conf_t *self, int32_t id)
{
	int32_t last;

	if (self->inst_num == 0)
	{
		puts("??");
		return;
	}
#ifndef __EMSCRIPTEN__
	SDL_SemWait(self->semaphore);
#endif
	last = --self->inst_num;

	self->comps[id]->conf = NULL;

	if (self->inst_num && id != last)
	{
		self->inst[id] = self->inst[last];
		self->props[id] = self->props[last];
#ifdef MESH4
		self->angle4[id] = self->angle4[last];
#endif
		self->comps[id] = self->comps[last];

		self->comps[id]->instance_id = id;

		self->comps[id]->updates = MASK_TRANS | MASK_PROPS;
		self->trans_updates++;
		self->props_updates++;
	}

#ifndef __EMSCRIPTEN__
	SDL_SemPost(self->semaphore);
#endif
}

varray_t *varray_get(mesh_t *mesh)
{
	uint32_t p = hash_ptr(mesh);

	varray_t *varray;
	khiter_t k = kh_get(varray, g_varrays, p);
	if (k == kh_end(g_varrays))
	{
		int32_t ret;
		k = kh_put(varray, g_varrays, p, &ret);

		varray = kh_value(g_varrays, k) = calloc(1, sizeof(varray_t));
		varray->mesh = mesh;
	}
	else
	{
		varray = kh_value(g_varrays, k);
	}
	return varray;
}

draw_conf_t *drawable_get_conf(drawable_t *self, uint32_t gid)
{
	draw_conf_t *result;
	struct conf_vars conf;
	draw_group_t *draw_group;
	uint32_t p;
	khiter_t k;

	if (!self->vs) return NULL;
	if (!self->mesh) return NULL;

	conf.mesh           = self->mesh;
	conf.skin           = self->skin;
	conf.custom_texture = self->custom_texture;
	conf.vs             = self->vs;
	conf.draw_callback  = self->draw_callback;
	conf.usrptr         = self->usrptr;
	conf.xray           = self->xray;
	conf.mat_type       = self->mat ? self->mat->type : 0;

	if (self->bind[gid].grp == 0)
	{
		if (!self->bind[gid].conf)
		{
			result = calloc(1, sizeof(draw_conf_t));
#ifndef __EMSCRIPTEN__
			result->semaphore = SDL_CreateSemaphore(1);
#endif
		}
		else
		{
			result = self->bind[gid].conf;
		}
		result->vars = conf;
		return result;
	}

	draw_group = get_group(self->bind[gid].grp);
	if (draw_group->configs == NULL) return NULL;

	p = murmur_hash(&conf, sizeof(conf), 0);

	k = kh_get(config, draw_group->configs, p);
	if (k == kh_end(draw_group->configs))
	{
		int32_t ret;
		k = kh_put(config, draw_group->configs, p, &ret);

		result = kh_value(draw_group->configs, k) = calloc(1, sizeof(draw_conf_t));
#ifndef __EMSCRIPTEN__
		result->semaphore = SDL_CreateSemaphore(1);
#endif
		result->vars = conf;
	}
	else
	{
		result = kh_value(draw_group->configs, k);
	}
	return result;
}

void drawable_model_changed(drawable_t *self)
{
	uint32_t gid;
	for(gid = 0; gid < self->bind_num; gid++)
	{
		vec4_t *props;
		uvec2_t new_ent;
		vec4_t new_props;
		struct draw_bind *bind = &self->bind[gid];
		draw_conf_t *conf = drawable_get_conf(self, gid);

		/* NON BUFFEREABLE INSTANCE PROPERTIES */
		if (conf != bind->conf)
		{
			if (bind->conf)
			{
				draw_conf_remove_instance(bind->conf, bind->instance_id);
			}

			if (conf)
			{
				draw_conf_add_instance(conf, self, gid);
			}
		}
		if (!conf) continue;

		/* BUFFEREABLE INSTANCE PROPERTIES */ 
		props = &bind->conf->props[bind->instance_id];
		new_ent = entity_to_uvec2(self->entity);

		new_props = vec4(
				self->matid,
				0 /* TODO use y for something */,
				new_ent.x,
				new_ent.y);

		if (!vec4_equals(*props, new_props))
		{
			*props = new_props;
			if (!(bind->updates & MASK_PROPS))
			{
#ifndef __EMSCRIPTEN__
				SDL_SemWait(conf->semaphore);
#endif
				conf->props_updates++;
#ifndef __EMSCRIPTEN__
				SDL_SemPost(conf->semaphore);
#endif
				bind->updates |= MASK_PROPS;
			}
		}
	}
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

int32_t varray_update_ram(varray_t *self)
{
	int32_t i;
	mesh_t *mesh = self->mesh;
	varray_clear(self);

	/* mat_layer_t *layer = &model->layers[self->layer_id]; */
	varray_vert_prealloc(self, vector_count(mesh->verts));

	/* int32_t selection = layer->selection; */

	if (vector_count(mesh->faces))
	{
		int32_t triangle_count = 0;
		mesh_update_smooth_normals(mesh, 1.0f - mesh->smooth_angle);
		for(i = 0; i < vector_count(mesh->faces); i++)
		{
			int32_t id = i;
			face_t *face = m_face(mesh, id); if (!face) continue;

			if (face->e_size == 3) triangle_count++;
			else triangle_count += 2;
		}
		varray_ind_prealloc(self, triangle_count * 3);

		for(i = 0; i < vector_count(mesh->faces); i++)
		{
			int32_t id = i;
			face_t *face = m_face(mesh, id); if (!face) continue;

#ifdef MESH4

			/* if (face->pair != -1) */
			/* { */
			/* 	cell_t *cell1 = f_cell(f_pair(face, mesh), mesh); */
			/* 	cell_t *cell0 = f_cell(face, mesh); */
			/* 	if (cell0 && cell1) */
			/* 	{ */
			/* 		vec4_t n0 = get_cell_normal(mesh, cell0); */
			/* 		vec4_t n1 = get_cell_normal(mesh, cell1); */
			/* 		float dot = vec4_dot(n1, n0); */
			/* 		if (dot > 0.999) continue; */
			/* 	} */
			/* } */

#endif

			varray_face_to_gl(self, face, id);
		}
	}
	else if (vector_count(mesh->edges))
	{
		varray_edges_to_gl(self);
	}
	else if (vector_count(mesh->verts))
	{
		varray_verts_to_gl(self);
	}
	return 1;
}

static void varray_update_buffers(varray_t *self)
{
	uint32_t *vbo = self->vbo;
	if (self->vert_num > self->vert_num_gl)
	{
		self->vert_num_gl = self->vert_num;

		/* VERTEX BUFFER */
		create_buffer(&vbo[0], self->pos, N, self->vert_num, 0);

		/* NORMAL BUFFER */
		create_buffer(&vbo[1], self->nor, 3, self->vert_num, 0);

		/* TEXTURE COORDS BUFFER */
		create_buffer(&vbo[2], self->tex, 2, self->vert_num, 0);

		/* TANGENT BUFFER */
		create_buffer(&vbo[3], self->tan, 3, self->vert_num, 0);

		/* ID BUFFER */
		create_buffer(&vbo[4], self->id, 2, self->vert_num, 0);

		/* COLOR BUFFER */
		create_buffer(&vbo[5], self->col, 3, self->vert_num, 0);

		/* if (self->mesh->has_skin) */
		{
			/* BONEID BUFFER */
			create_buffer(&vbo[6], self->bid, 4, self->vert_num, 0);

			/* BONE WEIGHT BUFFER */
			create_buffer(&vbo[7], self->wei, 4, self->vert_num, 0);
		}
	}
	if (self->ind_num > self->ind_num_gl)
	{
		self->ind_num_gl = self->ind_num;
		if (!vbo[15]) glGenBuffers(1, &vbo[15]);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[15]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, self->ind_num *
				sizeof(*self->ind), self->ind, GL_STATIC_DRAW);
		glerr();

	}

	if (self->ind_num)
	{
		/* VERTEX BUFFER */
		update_buffer(&vbo[0], self->pos, N, self->vert_num, 0); glerr();

		/* NORMAL BUFFER */
		update_buffer(&vbo[1], self->nor, 3, self->vert_num, 0); glerr();

		/* TEXTURE COORDS BUFFER */
		update_buffer(&vbo[2], self->tex, 2, self->vert_num, 0); glerr();

		/* TANGENT BUFFER */
		update_buffer(&vbo[3], self->tan, 3, self->vert_num, 0); glerr();

		/* ID BUFFER */
		update_buffer(&vbo[4], self->id, 2, self->vert_num, 0); glerr();

		/* COLOR BUFFER */
		update_buffer(&vbo[5], self->col, 3, self->vert_num, 0); glerr();

		/* if (self->mesh->has_skin) */
		{
			/* BONEID BUFFER */
			update_buffer(&vbo[6], self->bid, 4, self->vert_num, 0); glerr();

			/* BONE WEIGHT BUFFER */
			update_buffer(&vbo[7], self->wei, 4, self->vert_num, 0); glerr();
		}

		/* INDEX BUFFER */
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[15]);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
				self->ind_num * sizeof(*self->ind), self->ind);
		glerr();

	}
	self->updating = false;
}

void varray_update(varray_t *self)
{
	if (!self->mesh || self->mesh->locked_read) return;

	if (self->update_id_ram != self->mesh->update_id)
	{
		mesh_lock_write(self->mesh);

		self->updating = true;
		glBindVertexArray(0); glerr();

		varray_update_ram(self);
		varray_update_buffers(self);

		self->update_id_ram = self->mesh->update_id;
		mesh_unlock_write(self->mesh);
	}
}

static void varray_bind(varray_t *self)
{
	uint32_t *vbo = self->vbo;
	/* VERTEX BUFFER */
	bind_buffer(&vbo[0], 0, GL_FLOAT, N, 0);

	/* NORMAL BUFFER */
	bind_buffer(&vbo[1], 1, GL_FLOAT, 3, 0);

	/* TEXTURE COORDS BUFFER */
	bind_buffer(&vbo[2], 2, GL_FLOAT, 2, 0);

	/* TANGENT BUFFER */
	bind_buffer(&vbo[3], 3, GL_FLOAT, 3, 0);

	/* ID BUFFER */
	bind_buffer(&vbo[4], 4, GL_FLOAT, 2, 0);

	/* COLOR BUFFER */
	bind_buffer(&vbo[5], 5, GL_FLOAT, 3, 0);

	/* if (self->mesh->has_skin) */
	{
		/* BONEID BUFFER */
		bind_buffer(&vbo[6], 6, GL_FLOAT, 4, 0);

		/* BONE WEIGHT BUFFER */
		bind_buffer(&vbo[7], 7, GL_FLOAT, 4, 0);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[15]);
}

static void draw_conf_update_vao(draw_conf_t *self)
{
	if (!self->vbo[8]) return;

	if (!self->vao) glGenVertexArrays(1, &self->vao);
	glBindVertexArray(self->vao); glerr();
	varray_bind(self->varray);

	/* TRANSFORM BUFFER */
	bind_buffer(&self->vbo[8], 8, GL_FLOAT, 16, 1);

	/* PROPERTY BUFFER */
	bind_buffer(&self->vbo[12], 12, GL_FLOAT, 4, 1);

#ifdef MESH4
	/* 4D ANGLE BUFFER */
	bind_buffer(&self->vbo[13], 13, GL_FLOAT, 1, 1);
#endif
	glBindVertexArray(0); glerr();
}


static void draw_conf_update_skin(draw_conf_t *self)
{
	skin_t *skin = self->vars.skin;
	if (!self->skin)
	{
		glGenBuffers(1, &self->skin); glerr();
		glBindBuffer(GL_UNIFORM_BUFFER, self->skin); glerr();
		glBufferData(GL_UNIFORM_BUFFER, sizeof(skin->transforms),
				&skin->transforms[0], GL_DYNAMIC_DRAW); glerr();
	}
	if (self->last_skin_update == skin->update_id) return;
	self->last_skin_update = skin->update_id;
	glBindBuffer(GL_UNIFORM_BUFFER, self->skin); glerr();

	/* void *p = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY); */
	/* memcpy(p, &skin->transforms[0], skin->bones_num * sizeof(mat4_t)); */
	/* glUnmapBuffer(GL_UNIFORM_BUFFER); glerr(); */
	

	glBufferSubData(GL_UNIFORM_BUFFER, 0, skin->bones_num * sizeof(mat4_t),
	                &skin->transforms[0]);
}

extern uint32_t g_mat_ubo;
int32_t draw_conf_draw(draw_conf_t *self, int32_t instance_id)
{
	mesh_t *mesh;
	c_render_device_t *rd;
	shader_t *shader;
	int32_t cull_was_enabled;
	uint32_t primitive;

	if (!self || !self->inst_num) return 0;
#ifndef __EMSCRIPTEN__
	SDL_SemWait(self->semaphore);
#endif
	mesh = self->vars.mesh;
	/* printf("%d %p %d %s\n", self->vars.transparent, self->vars.mesh, */
			/* self->vars.xray, self->vars.vs->name); */
	if (!self->varray) self->varray = varray_get(self->vars.mesh);

	varray_update(self->varray);

	if (self->varray->ind_num == 0)
	{
		goto end;
	}

	draw_conf_update_inst(self, instance_id);

	draw_conf_update_vao(self);

	rd = c_render_device(&SYS);

	if (self->vars.skin)
	{
		draw_conf_update_skin(self);

		c_render_device_bind_ubo(rd, 21, self->skin);
	}
	{
		c_render_device_bind_ubo(rd, 22, material_get_ubo(self->vars.mat_type));
	}

	shader = vs_bind(self->vars.vs, self->vars.mat_type);
	if (!shader) goto end;

	if (self->vars.custom_texture)
	{
		uint32_t loc;
		loc = shader_cached_uniform(shader, ref("g_framebuffer"));
		glUniform1i(loc, 6);
		glActiveTexture(GL_TEXTURE0 + 6);
		texture_bind(self->vars.custom_texture, 0);
	}

	if (self->vars.draw_callback)
	{
		self->vars.draw_callback(self->vars.usrptr, shader);
	}

	cull_was_enabled = glIsEnabled(GL_CULL_FACE);
	if (mesh->cull)
	{
		const uint32_t culls[4] = {0, GL_FRONT, GL_BACK, GL_FRONT_AND_BACK};
		
		glCullFace(culls[mesh->cull ^ (rd->cull_invert ? 3 : 0)]); glerr();
	}
	else
	{
		glDisable(GL_CULL_FACE); glerr();
	}

	glUniform1i(shader_cached_uniform(shader, ref("has_tex")),
	                                  mesh->has_texcoords);
	glUniform1i(shader_cached_uniform(shader, ref("receive_shadows")),
	                                  mesh->receive_shadows);

	glBindVertexArray(self->vao); glerr();

	glLineWidth(3); glerr();

#ifndef __EMSCRIPTEN__
	glPolygonMode(GL_FRONT_AND_BACK, mesh->wireframe ? GL_LINE : GL_FILL);
	glerr();
#endif

	if (self->vars.xray) glDepthRange(0, 0.01);

	if (vector_count(mesh->faces))
	{
		primitive = GL_TRIANGLES;
		glUniform1i(shader_cached_uniform(shader, ref("has_normals")), 1);
		glerr();
	}
	else
	{
		glUniform1i(shader_cached_uniform(shader, ref("has_normals")), 0);
		glerr();
		if (vector_count(mesh->edges))
		{
			primitive = GL_LINES;
		}
		else
		{
			primitive = GL_POINTS;
		}
	}
	if (self->inst_num == 1) instance_id = 0;
	
	if (instance_id == -1)
	{
		glDrawElementsInstanced(primitive, self->varray->ind_num, IDTYPE,
		                        0, self->inst_num); glerr();
	}
	else
	{
		/* glDrawElementsInstancedBaseInstance(primitive, */
		/* 		self->varray->ind_num, GL_UNSIGNED_INT, 0, 1, instance_id); */
		glDrawElements(primitive, self->varray->ind_num, IDTYPE, 0);

		/* glDrawArraysOneInstance(primitive, instance_id, self->varray->ind_num, int instance ); */
		glerr();
	}
	if (cull_was_enabled) glEnable(GL_CULL_FACE);

end:
#ifndef __EMSCRIPTEN__
	SDL_SemPost(self->semaphore);
#endif

	glDepthRange(0.0, 1.00); glerr();

	return 1;
}

int32_t drawable_draw(drawable_t *self)
{
	/* if (self->conf->vars.scale_dist > 0.0f) */
	/* { */
	/* 	c_node_update_model(node); */

	/* 	mat4_t trans = node->model; */

	/* 	vec3_t pos = mat4_mul_vec4(trans, vec4(0,0,0,1)).xyz; */
	/* 	float dist = vec3_dist(pos, c_renderer(&SYS)->inputs.camera.pos); */
	/* 	trans = mat4_scale_aniso(trans, vec3(dist * self->conf->vars.scale_dist)); */
	/* } */


	int32_t res = 0;
	uint32_t gid;
	for(gid = 0; gid < self->bind_num; gid++)
	{
		res |= draw_conf_draw(self->bind[gid].conf, self->bind[gid].instance_id);
	}

	return res;
}

static void draw_conf_update_props(draw_conf_t *self)
{
	uint32_t i;
	/* PROPERTY BUFFER */
	update_buffer(&self->vbo[12], self->props, 4, self->inst_num, 0); glerr();
	for(i = 0; i < self->inst_num; i++)
	{
		self->comps[i]->updates &= ~MASK_PROPS;
	}
}

static void draw_conf_update_trans(draw_conf_t *self)
{
	uint32_t i;
	/* TRANSFORM BUFFER */
	update_buffer(&self->vbo[8], self->inst, 16, self->inst_num, 0); glerr();

#ifdef MESH4
	/* 4D ANGLE BUFFER */
	update_buffer(&self->vbo[13], self->angle4, 1, self->inst_num, 0); glerr();
#endif

	for(i = 0; i < self->inst_num; i++)
	{
		self->comps[i]->updates &= ~MASK_TRANS;
	}
}

static void draw_conf_update_inst_props(draw_conf_t *self, int32_t id)
{
	if (self->comps[id]->updates & MASK_PROPS)
	{
		/* PROPERTY BUFFER */
		update_buffer(&self->vbo[12], &self->props[id], 4, 1, id); glerr();
	}
	self->comps[id]->updates &= ~MASK_PROPS;
}

static void draw_conf_update_inst_trans(draw_conf_t *self, int32_t id)
{
	if (self->comps[id]->updates & MASK_TRANS)
	{
		/* TRANSFORM BUFFER */
		update_buffer(&self->vbo[8], &self->inst[id], 16, 1, id); glerr();

#ifdef MESH4
		/* 4D ANGLE BUFFER */
		update_buffer(&self->vbo[13], &self->angle4[id], 1, 1, id); glerr();
#endif
	}

	self->comps[id]->updates &= ~MASK_TRANS;
}

int world_frame(void);
static void draw_conf_update_inst(draw_conf_t *self, int32_t id)
{
	int frame = world_frame();
	if (self->inst_num > self->gl_inst_num)
	{
		self->gl_inst_num = self->inst_num;

		/* TRANSFORM BUFFER */
		create_buffer(&self->vbo[8], self->inst, 16, self->inst_num, 1);

		/* MATERIAL BUFFER */
		create_buffer(&self->vbo[12], self->props, 4, self->inst_num, 1);

#ifdef MESH4
		/* 4D ANGLE BUFFER */
		create_buffer(&self->vbo[13], self->angle4, 1, self->inst_num, 1);
#endif
		return;
	}

	if (id == -1)
	{
		int32_t i;
		if (self->trans_updates && self->last_update_frame != frame)
		{
			if (self->inst_num > 8 && self->trans_updates > self->inst_num / 2)
			{
				draw_conf_update_trans(self);
			}
			else for(i = 0; i < self->inst_num; i++)
			{
				draw_conf_update_inst_trans(self, i);
			}
			self->trans_updates = 0;
			self->last_update_frame = frame;
		}
		if (self->props_updates)
		{
			if (self->inst_num > 8 && self->props_updates > self->inst_num / 2)
			{
				draw_conf_update_props(self);
			}
			else for(i = 0; i < self->inst_num; i++)
			{
				draw_conf_update_inst_props(self, i);
			}
			self->props_updates = 0;
		}
	}
	else
	{
		draw_conf_update_inst_props(self, id);
		self->props_updates = 0;

		if (self->last_update_frame != frame)
		{
			draw_conf_update_inst_trans(self, id);
			self->last_update_frame = frame;
			self->trans_updates = 0;
		}
	}
}

void varray_destroy(varray_t *self)
{
	int32_t i;
	for(i = 0; i < 24; i++) if (self->vbo[i]) glDeleteBuffers(1, &self->vbo[i]);
	if (self->tex) free(self->tex);
	if (self->nor) free(self->nor);
	if (self->pos) free(self->pos);
	if (self->tan) free(self->tan);
	if (self->col) free(self->col);
	if (self->id) free(self->id);
	if (self->bid) free(self->bid);
	if (self->wei) free(self->wei);
	if (self->ind) free(self->ind);


}

void draw_conf_destroy(draw_conf_t *self)
{
	int32_t i;
	if (self->vao)
	{
		glDeleteVertexArrays(1, &self->vao);
	}
	self->vao = 0;
	for(i = 0; i < 24; i++) if (self->vbo[i]) glDeleteBuffers(1, &self->vbo[i]);
#ifdef MESH4
	if (self->angle4) free(self->angle4);
#endif
	if (self->inst) free(self->inst);
	if (self->props) free(self->props);
	if (self->comps) free(self->comps);

	if (self->varray) varray_destroy(self->varray);


}

static int32_t drawable_position_changed(drawable_t *self, struct draw_bind *bind)
{
	if (bind->conf)
	{
		bind->conf->inst[bind->instance_id] = self->transform;
#ifdef MESH4
		bind->conf->angle4[bind->instance_id] = self->angle4;
#endif
		if (!(bind->conf->comps[bind->instance_id]->updates & MASK_TRANS))
		{
			bind->conf->trans_updates++;
			bind->conf->comps[bind->instance_id]->updates |= MASK_TRANS;
		}
	}
	return CONTINUE;
}

void drawable_destroy(drawable_t *self)
{
	uint32_t gid;
	for(gid = 0; gid < self->bind_num; gid++)
	{
		draw_conf_t *conf = self->bind[gid].conf;
		draw_conf_remove_instance(conf, self->bind[gid].instance_id);
		/* TODO delete empty configurations */ 
		/* if (conf->inst_num == 0) draw_conf_destroy(); */
	}
}

static int32_t draw_group_draw(draw_group_t *self)
{
	int32_t res = 0;
	khiter_t k;
	draw_conf_t *conf;

	if (!self) return 0;
	if (!self->configs) return 0;

	SDL_SemWait(g_group_semaphore);
	for(k = kh_begin(self->configs); k != kh_end(self->configs); ++k)
	{
		if (!kh_exist(self->configs, k)) continue;
		conf = kh_value(self->configs, k);
		res |= draw_conf_draw(conf, -1);
	}
	SDL_SemPost(g_group_semaphore);
	return res;
}

void draw_group(uint32_t ref)
{
	draw_group_draw(get_group(ref));
}

void draw_groups_init()
{
	g_varrays = kh_init(varray);
	g_group_semaphore = SDL_CreateSemaphore(1);
}
