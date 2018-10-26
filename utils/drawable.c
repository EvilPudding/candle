#include "drawable.h"
#include <utils/loader.h>
#include <candle.h>
#include <string.h>
#include <stdlib.h>

KHASH_MAP_INIT_INT(varray, varray_t*)

static khash_t(varray) *g_varrays;

static int32_t draw_conf_add_instance(draw_conf_t *self, drawable_t *draw,
		uint32_t gid);
static void draw_conf_update_inst(draw_conf_t *self, int32_t id);
static int32_t drawable_position_changed(drawable_t *self, struct draw_grp *grp);
static draw_group_t *get_group(uint32_t ref);
static void draw_conf_remove_instance(draw_conf_t *self, int32_t id);

#define MASK_TRANS (1 << 0)
#define MASK_PROPS (1 << 1)

KHASH_MAP_INIT_INT(draw_group, draw_group_t)
khash_t(draw_group) *g_draw_groups;

void drawable_init(drawable_t *self, uint32_t group, void *userptr)
{
	self->transform = mat4();

	self->userptr = userptr;
	drawable_add_group(self, group);
}

static draw_group_t *get_group(uint32_t ref)
{
	if(ref == 0) return NULL;
	int32_t ret;
	draw_group_t *grp;
	khiter_t k;

	if(!g_draw_groups) g_draw_groups = kh_init(draw_group);
	k = kh_get(draw_group, g_draw_groups, ref);

	if(k == kh_end(g_draw_groups))
	{
		k = kh_put(draw_group, g_draw_groups, ref, &ret);
		grp = &kh_value(g_draw_groups, k);
		*grp = (draw_group_t){ 0 };
	}
	else
	{
		grp = &kh_value(g_draw_groups, k);
	}
	return grp;
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
	if(self->mesh->skin)
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
	self->props = realloc(self->props, self->inst_alloc *
			sizeof(*self->props));
	/* memset(self->props + (self->inst_alloc - size), 0, size * sizeof(*self->props)); */

#ifdef MESH4
	self->angle4 = realloc(self->angle4, self->inst_alloc *
			sizeof(*self->angle4));
#endif

	self->comps = realloc(self->comps, self->inst_alloc *
			sizeof(*self->comps));
}
void draw_conf_inst_grow(draw_conf_t *self)
{
	if(self->inst_alloc < self->inst_num)
	{
		self->inst_alloc = self->inst_num;

		draw_conf_inst_prealloc(self, 0);
	}
}

void varray_vert_grow(varray_t *self)
{
	if(self->vert_alloc < self->vert_num)
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
	if(self->ind_alloc < self->ind_num)
	{
		self->ind_alloc = self->ind_num;
		varray_ind_prealloc(self, 0);
	}
}

void drawable_remove_group(drawable_t *self, uint32_t group)
{
	uint32_t gid;

	if(group == 0) return;

	for(gid = 0; gid < self->grp_num; gid++)
	{
		if(self->grp[gid].grp == group) break;
	}
	if(gid == self->grp_num) return;

	if(self->grp[gid].conf)
	{
		if(self->grp[gid].grp == 0)
		{
			free(self->grp[gid].conf);
		}
		else
		{
			draw_conf_remove_instance(self->grp[gid].conf,
					self->grp[gid].instance_id);
		}
	}
	self->grp_num--;
	if(gid < self->grp_num)
	{
		struct draw_grp *last = &self->grp[self->grp_num];
		if(last->conf)
		{
			last->conf->comps[last->instance_id] = &self->grp[gid];
		}
		self->grp[gid] = *last;
	}
	if(self->grp_num == 0)
	{
		drawable_add_group(self, 0);
	}
	drawable_model_changed(self);
}

void drawable_add_group(drawable_t *self, uint32_t group)
{
	uint32_t gid;
	if(group == 0 && self->grp_num > 0) return;

	for(gid = 0; gid < self->grp_num; gid++)
	{
		if(self->grp[gid].grp == group) return;
	}
	if(self->grp[0].grp == 0)
	{
		if(self->grp[0].conf) free(self->grp[0].conf);
		self->grp_num = 0;
		gid = 0;
	}

	self->grp[gid].grp = group;
	self->grp[gid].instance_id = 0;
	self->grp[gid].conf = NULL;
	self->grp[gid].updates = 0;
	self->grp_num++;
	drawable_model_changed(self);
}

void drawable_set_group(drawable_t *self, uint32_t group)
{
	uint32_t gid = 0;
	if(group == 0) return;

	if(self->grp[gid].grp == group) return;

	if(self->grp[gid].conf)
	{
		if(self->grp[gid].grp == 0)
		{
			free(self->grp[gid].conf);
		}
		else
		{
			draw_conf_remove_instance(self->grp[gid].conf,
					self->grp[gid].instance_id);
		}
	}

	self->grp[gid].grp = group;
	self->grp[gid].instance_id = 0;
	self->grp[gid].conf = NULL;
	self->grp[gid].updates = 0;

	drawable_model_changed(self);
}

void drawable_set_mesh(drawable_t *self, mesh_t *mesh)
{
	if(mesh != self->mesh)
	{
		mesh_t *previous = self->mesh;
		self->mesh = mesh;
		drawable_model_changed(self);
		if(mesh)
		{
			mesh->ref_num++;
			if(!previous)
			{
				for(uint32_t gid = 0; gid < self->grp_num; gid++)
				{
					drawable_position_changed(self, &self->grp[gid]);
				}
			}
			else
			{
				mesh_destroy(previous);
			}
		}
	}
}

void drawable_set_vs(drawable_t *self, vs_t *vs)
{
	if(self->vs != vs)
	{
		self->vs = vs;
		drawable_model_changed(self);
	}
}

void drawable_set_xray(drawable_t *self, int32_t xray)
{
	if(self->xray != xray)
	{
		self->xray = xray;
		drawable_model_changed(self);
	}
}

void drawable_set_mat(drawable_t *self, int32_t id)
{
	if(self->mat != id)
	{
		self->mat = id;
		drawable_model_changed(self);
	}
}

#ifdef MESH4
void drawable_set_angel4(drawable_t *self, mat4_t transform)
{
	uint32_t gid;

	self->angle4 = angle4;
	for(gid = 0; gid < self->grp_num; gid++)
	{
		drawable_position_changed(self, &self->grp[gid]);
	}
}
#endif

void drawable_set_transform(drawable_t *self, mat4_t transform)
{
	uint32_t gid;

	self->transform = transform;
	for(gid = 0; gid < self->grp_num; gid++)
	{
		drawable_position_changed(self, &self->grp[gid]);
	}
}

void drawable_set_entity(drawable_t *self, entity_t entity)
{
	self->entity = entity;
}

static int32_t draw_conf_add_instance(draw_conf_t *self, drawable_t *draw,
		uint32_t gid)
{
	SDL_SemWait(self->semaphore);

	int32_t i = self->inst_num++;
	/* if(draw->grp[gid].grp == ref("light")) */
		/* printf("adding		%d\n", self->inst_num); */

	draw_conf_inst_grow(self);

	self->props[i].x = draw->mat;
	self->props[i].y = 0;/*TODO: use y for something;*/
	self->props[i].zw = entity_to_uvec2(draw->entity);
	self->comps[i] = &draw->grp[gid];

	draw->grp[gid].instance_id = i;
	draw->grp[gid].conf = self;
	draw->grp[gid].updates = MASK_TRANS | MASK_PROPS;

	self->trans_updates++;
	self->props_updates++;

	SDL_SemPost(self->semaphore);
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
		uvec4_t bid)
{
	/* int32_t i = draw_conf_get_vert(self, p, n, t); */
	/* if(i >= 0) return i; */
	int32_t i = self->vert_num++;
	varray_vert_grow(self);

	self->pos[i] = p;
	self->nor[i] = n;
	self->tan[i] = tg;
	self->tex[i] = t;
	self->col[i] = c;
	self->id[i] = int_to_vec2(id);

	if(self->mesh->skin)
	{
		self->bid[i] = bid;
		self->wei[i] = wei;
	}

	return i;
}

void varray_add_line(varray_t *self, int32_t v1, int32_t v2)
{
	int32_t i = self->ind_num;

	self->ind_num += 2;
	varray_ind_grow(self);

	self->ind[i + 0] = v1;
	self->ind[i + 1] = v2;
}

void varray_add_triangle(varray_t *self, int32_t v1, int32_t v2, int32_t v3)
{
	int32_t i = self->ind_num;
	self->ind_num += 3;
	varray_ind_grow(self);

	self->ind[i + 0] = v1;
	self->ind[i + 1] = v2;
	self->ind[i + 2] = v3;

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
		edge_t *curr_edge = m_edge(mesh, i);
		if(!curr_edge) continue;

		edge_t *next_edge = m_edge(mesh, curr_edge->next);
		if(!next_edge) continue;

		vertex_t *V1 = e_vert(curr_edge, mesh);
		vertex_t *V2 = e_vert(next_edge, mesh);
		int32_t v1 = varray_add_vert(self, V1->pos,
				vec3(0.0f), vec2(0.0f), vec3(0.0f), V1->color.xyz, 0, vec4(0,0,0,0), uvec4(0,0,0,0));
		int32_t v2 = varray_add_vert(self, V2->pos,
				vec3(0.0f), vec2(0.0f), vec3(0.0f), V2->color.xyz, 0, vec4(0,0,0,0), uvec4(0,0,0,0));

		varray_add_line(self, v1, v2);

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

		vec4_t  wei = vec4(0,0,0,0);
		uvec4_t bid = uvec4(0,0,0,0);
		if(self->mesh->skin)
		{
			wei = self->mesh->skin->wei[hedge->v];
			bid = self->mesh->skin->bid[hedge->v];
		}
		v[i] = varray_add_vert(self, V->pos, hedge->n,
				hedge->t, hedge->tg, V->color.xyz, id, wei, bid);
	}
	if(f->e_size == 4)
	{
		varray_add_triangle(self, v[0], v[1], v[2]);
		varray_add_triangle(self, v[2], v[3], v[0]);

	}
	else if(f->e_size == 3)
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
	glBindBuffer(GL_ARRAY_BUFFER, *vbo); glerr();

	int32_t j = 0;
	while(j < dim)
	{
		int step = (dim - j < 4) ? dim - j : 4;

		const void *offset = (const void*)(sizeof(GLfloat) * j);
		uint64_t size = dim * sizeof(GLfloat);

		glEnableVertexAttribArray(id); glerr();

		if(type == GL_FLOAT)
		{
			glVertexAttribPointer(id, step, type, GL_FALSE, size, offset);
		}
		else
		{
			glVertexAttribIPointer(id, step, type, size, offset);
		}

		glVertexAttribDivisor(id, instanced_divisor); glerr();

		id++;
		j += step;
	}
}

static inline void create_buffer(uint32_t *vbo, void *arr,
		int32_t dim, int32_t count, int32_t instanced_divisor)
{
	uint32_t usage = instanced_divisor ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
	if(!*vbo)
	{
		glGenBuffers(1, vbo); glerr();
	}
	glBindBuffer(GL_ARRAY_BUFFER, *vbo); glerr();
	glBufferData(GL_ARRAY_BUFFER, dim * sizeof(GLfloat) * count, arr, usage);
	glerr();


}

static inline void update_buffer(uint32_t *vbo, void *arr, int32_t dim,
		int32_t count, int32_t inst)
{
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferSubData(GL_ARRAY_BUFFER, inst * dim * sizeof(GLfloat),
			dim * sizeof(GLfloat) * count, arr);
}


static void draw_conf_remove_instance(draw_conf_t *self, int32_t id)
{
	if(self->inst_num == 0)
	{
		puts("??");
		return;
	}
	SDL_SemWait(self->semaphore);
	int32_t i = --self->inst_num;
	/* if(self->comps[id]->grp == ref("light")) */
		/* printf("removing	%d\n", self->inst_num); */

	if(self->inst_num && id != i)
	{
		self->inst[id] = self->inst[i];
		self->props[id] = self->props[i];
#ifdef MESH4
		self->angle4[id] = self->angle4[i];
#endif
		self->comps[id] = self->comps[i];

		self->comps[id]->instance_id = id;

		self->comps[id]->updates = MASK_TRANS | MASK_PROPS;
		self->trans_updates++;
		self->props_updates++;
	}

	SDL_SemPost(self->semaphore);
}

varray_t *varray_get(mesh_t *mesh)
{
	uint32_t p = hash_ptr(mesh);

	varray_t *varray;
	khiter_t k = kh_get(varray, g_varrays, p);
	if(k == kh_end(g_varrays))
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
	if(!self->vs) return NULL;
	if(!self->mesh) return NULL;

	draw_conf_t *result;
	struct conf_vars conf = {
		.mesh = self->mesh,
		.xray = self->xray,
		.vs = self->vs
	};

	if(self->grp[gid].grp == 0)
	{
		if(!self->grp[gid].conf)
		{
			result = calloc(1, sizeof(draw_conf_t));
			result->semaphore = SDL_CreateSemaphore(1);
		}
		else
		{
			result = self->grp[gid].conf;
		}
		result->vars = conf;
		return result;
	}

	draw_group_t *draw_group = get_group(self->grp[gid].grp);
	if(draw_group == NULL) return NULL;


	uint32_t p = murmur_hash(&conf, sizeof(conf), 0);

	khiter_t k = kh_get(config, draw_group, p);
	if(k == kh_end(draw_group))
	{
		int32_t ret;
		k = kh_put(config, draw_group, p, &ret);

		result = kh_value(draw_group, k) = calloc(1, sizeof(draw_conf_t));
		result->semaphore = SDL_CreateSemaphore(1);
		result->vars = conf;
	}
	else
	{
		result = kh_value(draw_group, k);
	}
	return result;
}

void drawable_model_changed(drawable_t *self)
{
	uint32_t gid;
	for(gid = 0; gid < self->grp_num; gid++)
	{
		struct draw_grp *grp = &self->grp[gid];
		draw_conf_t *conf = drawable_get_conf(self, gid);

		/* NON BUFFEREABLE INSTANCE PROPERTIES */
		if(conf != grp->conf)
		{
			if(grp->conf)
			{
				draw_conf_remove_instance(grp->conf, grp->instance_id);
				grp->conf = NULL;
			}

			if(conf)
			{
				draw_conf_add_instance(conf, self, gid);
			}
		}
		if(!conf) continue;

		/* BUFFEREABLE INSTANCE PROPERTIES */ 
		uvec4_t *props = &grp->conf->props[grp->instance_id];
		uvec2_t new_ent = entity_to_uvec2(self->entity);

		uvec4_t new_props = uvec4(
				self->mat,
				0 /* TODO use y for something */,
				new_ent.x,
				new_ent.y);

		if(!uvec4_equals(*props, new_props))
		{
			*props = new_props;
			if(!(grp->updates & MASK_PROPS))
			{
				SDL_SemWait(conf->semaphore);
				conf->props_updates++;
				SDL_SemPost(conf->semaphore);
				grp->updates |= MASK_PROPS;
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
	mesh_t *mesh = self->mesh;
	varray_clear(self);

	/* mat_layer_t *layer = &model->layers[self->layer_id]; */
	varray_vert_prealloc(self, vector_count(mesh->verts));

	/* int32_t selection = layer->selection; */

	vector_t *faces = mesh->faces;
	vector_t *edges = mesh->edges;

	/* mesh_selection_t *sel = &mesh->selections[selection]; */
	/* if(selection != -1) */
	/* { */
		/* return 1; */
	/* 	faces = sel->faces; */
	/* 	edges = sel->edges; */
	/* } */

	int32_t i;
	if(vector_count(faces))
	{
		mesh_update_smooth_normals(mesh, 1.0f - mesh->smooth_angle);
		int32_t triangle_count = 0;
		for(i = 0; i < vector_count(faces); i++)
		{
			int32_t id = i;
			face_t *face = m_face(mesh, id); if(!face) continue;

			if(face->e_size == 3) triangle_count++;
			else triangle_count += 2;
		}
		varray_ind_prealloc(self, triangle_count * 3);

		for(i = 0; i < vector_count(faces); i++)
		{
			int32_t id = i;
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

			varray_face_to_gl(self, face, id);
		}
	}
	else if(vector_count(edges))
	{
		varray_edges_to_gl(self);
	}
	return 1;
}

static void varray_update_buffers(varray_t *self)
{
	uint32_t *vbo = self->vbo;
	if(self->vert_num > self->vert_num_gl)
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

		if(self->mesh->skin)
		{
			/* BONEID BUFFER */
			create_buffer(&vbo[6], self->bid, 4, self->vert_num, 0);

			/* BONE WEIGHT BUFFER */
			create_buffer(&vbo[7], self->wei, 4, self->vert_num, 0);
		}
	}
	if(self->ind_num > self->ind_num_gl)
	{
		self->ind_num_gl = self->ind_num;
		if(!vbo[15]) glGenBuffers(1, &vbo[15]);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[15]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, self->ind_num *
				sizeof(*self->ind), self->ind, GL_STATIC_DRAW);
		glerr();

	}

	if(self->ind_num_gl)
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

		if(self->mesh->skin)
		{
			/* BONEID BUFFER */
			update_buffer(&vbo[6], self->bid, 4, self->vert_num, 0); glerr();

			/* BONE WEIGHT BUFFER */
			update_buffer(&vbo[7], self->wei, 4, self->vert_num, 0); glerr();
		}

		/* INDEX BUFFER */
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[15]);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
				self->ind_num_gl * sizeof(*self->ind), self->ind);
		glerr();

	}
	self->updating = 0;
}

void varray_update(varray_t *self)
{
	if(!self->mesh || self->mesh->locked_read) return;

	if(self->update_id_ram != self->mesh->update_id)
	{
		mesh_lock_write(self->mesh);

		self->updating = 1;
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
	bind_buffer(&vbo[4], 4, GL_UNSIGNED_INT, 2, 0);

	/* COLOR BUFFER */
	bind_buffer(&vbo[5], 5, GL_FLOAT, 3, 0);

	if(self->mesh->skin)
	{
		/* BONEID BUFFER */
		bind_buffer(&vbo[6], 6, GL_UNSIGNED_INT, 4, 0);

		/* BONE WEIGHT BUFFER */
		bind_buffer(&vbo[7], 7, GL_FLOAT, 4, 0);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[15]);
}

static void draw_conf_update_vao(draw_conf_t *self)
{
	if(!self->vbo[8]) return;

	if(!self->vao) glGenVertexArrays(1, &self->vao);
	glBindVertexArray(self->vao); glerr();
	varray_bind(self->varray);

	/* TRANSFORM BUFFER */
	bind_buffer(&self->vbo[8], 8, GL_FLOAT, 16, 1);

	/* PROPERTY BUFFER */
	bind_buffer(&self->vbo[12], 12, GL_UNSIGNED_INT, 4, 1);

#ifdef MESH4
	/* 4D ANGLE BUFFER */
	bind_buffer(&self->vbo[13], 13, GL_FLOAT, 1, 1);
#endif
	glBindVertexArray(0); glerr();
}

int32_t draw_conf_draw(draw_conf_t *self, int32_t instance_id)
{
	if(!self || !self->inst_num) return 0;
	mesh_t *mesh = self->vars.mesh;
	/* printf("%d %p %d %s\n", self->vars.transparent, self->vars.mesh, */
			/* self->vars.xray, self->vars.vs->name); */

	if(!self->varray) self->varray = varray_get(self->vars.mesh);

	varray_update(self->varray);

	draw_conf_update_inst(self, instance_id);

	draw_conf_update_vao(self);

	vs_bind(self->vars.vs);

	int32_t cull_was_enabled = glIsEnabled(GL_CULL_FACE);
	if(mesh->cull)
	{
		const uint32_t culls[4] = {0, GL_FRONT, GL_BACK, GL_FRONT_AND_BACK};
		glCullFace(culls[mesh->cull]);
	}
	else
	{
		glDisable(GL_CULL_FACE); glerr();
	}

	glUniform1ui(23, mesh->has_texcoords);

	glBindVertexArray(self->vao); glerr();

	glLineWidth(3);

	glPolygonMode(GL_FRONT_AND_BACK, mesh->wireframe ? GL_LINE : GL_FILL);

	if(self->vars.xray) glDepthRange(0, 0.01);

	uint32_t primitive = vector_count(mesh->faces) ? GL_TRIANGLES : GL_LINES;
	
	if(instance_id == -1)
	{
		glDrawElementsInstancedBaseInstance(primitive,
				self->varray->ind_num_gl, GL_UNSIGNED_INT, 0, self->inst_num, 0);
		glerr();
	}
	else
	{
		glDrawElementsInstancedBaseInstance(primitive,
				self->varray->ind_num_gl, GL_UNSIGNED_INT, 0, 1, instance_id);
		glerr();
	}

	glDepthRange(0.0, 1.00);
	if(cull_was_enabled) glEnable(GL_CULL_FACE);

	return 1;

}

int32_t drawable_draw(drawable_t *self)
{

	/* if(self->conf->vars.scale_dist > 0.0f) */
	/* { */
	/* 	c_node_update_model(node); */

	/* 	mat4_t trans = node->model; */

	/* 	vec3_t pos = mat4_mul_vec4(trans, vec4(0,0,0,1)).xyz; */
	/* 	float dist = vec3_dist(pos, c_renderer(&SYS)->inputs.camera.pos); */
	/* 	trans = mat4_scale_aniso(trans, vec3(dist * self->conf->vars.scale_dist)); */
	/* } */


	int32_t res = 0;
	uint32_t gid;
	for(gid = 0; gid < self->grp_num; gid++)
	{
		res |= draw_conf_draw(self->grp[gid].conf, self->grp[gid].instance_id);
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
	if(self->comps[id]->updates & MASK_PROPS)
	{
		/* PROPERTY BUFFER */
		update_buffer(&self->vbo[12], &self->props[id], 4, 1, id); glerr();
	}
	self->comps[id]->updates &= ~MASK_PROPS;
}
static void draw_conf_update_inst_trans(draw_conf_t *self, int32_t id)
{
	if(self->comps[id]->updates & MASK_TRANS)
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

static void draw_conf_update_inst(draw_conf_t *self, int32_t id)
{
	SDL_SemWait(self->semaphore);
	if(self->inst_num > self->gl_inst_num)
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
		SDL_SemPost(self->semaphore);
		return;
	}

	if(id == -1)
	{
		int32_t i;
		if(self->trans_updates)
		{
			if(self->inst_num > 8 && self->trans_updates > self->inst_num / 2)
			{
				draw_conf_update_trans(self);
			}
			else for(i = 0; i < self->inst_num; i++)
			{
				draw_conf_update_inst_trans(self, i);
			}
			self->trans_updates = 0;
		}
		if(self->props_updates)
		{
			if(self->inst_num > 8 && self->props_updates > self->inst_num / 2)
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
		draw_conf_update_inst_trans(self, id);
	}
	SDL_SemPost(self->semaphore);
}

void varray_destroy(varray_t *self)
{
	int32_t i;
	for(i = 0; i < 24; i++) if(self->vbo[i]) glDeleteBuffers(1, &self->vbo[i]);
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

void draw_conf_destroy(draw_conf_t *self)
{
	int32_t i;
	if(self->vao)
	{
		glDeleteVertexArrays(1, &self->vao);
	}
	self->vao = 0;
	for(i = 0; i < 24; i++) if(self->vbo[i]) glDeleteBuffers(1, &self->vbo[i]);
#ifdef MESH4
	if(self->angle4) free(self->angle4);
#endif
	if(self->inst) free(self->inst);
	if(self->props) free(self->props);
	if(self->comps) free(self->comps);

	if(self->varray) varray_destroy(self->varray);


}

static int32_t drawable_position_changed(drawable_t *self, struct draw_grp *grp)
{
	if(grp->conf)
	{
		grp->conf->inst[grp->instance_id] = self->transform;
#ifdef MESH4
		grp->conf->angle4[grp->instance_id].y = self->angle4;
#endif
		if(!(grp->conf->comps[grp->instance_id]->updates & MASK_TRANS))
		{
			grp->conf->trans_updates++;
			grp->conf->comps[grp->instance_id]->updates |= MASK_TRANS;
		}
	}
	return CONTINUE;
}

void drawable_destroy(drawable_t *self)
{
	uint32_t gid;
	for(gid = 0; gid < self->grp_num; gid++)
	{
		draw_conf_destroy(self->grp[gid].conf);
	}
}

static int32_t draw_group_draw(draw_group_t *self)
{
	if(!self) return 0;
	int32_t res = 0;
	khiter_t k;
	for(k = kh_begin(self); k != kh_end(self); ++k)
	{
		if(!kh_exist(self, k)) continue;
		draw_conf_t *conf = kh_value(self, k);
		res |= draw_conf_draw(conf, -1);
	}
	return res;
}

void draw_group(uint32_t ref)
{
	draw_group_draw(get_group(ref));
}

REG()
{
	g_varrays = kh_init(varray);
}
