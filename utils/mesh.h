#ifndef MESH_H
#define MESH_H

#include "macros.h"
#include "mafs.h"
#include "material.h"
#include "vector.h"
#include "khash.h"

struct mesh;
struct face;
struct edge;

typedef struct vec3_t(*support_cb)(struct mesh *self, const vec3_t dir);
#ifdef MESH4

#	define vecN vec4
#	define vecN_t vec4_t
#	define uvecN uvec4
#	define uvecN_t uvec4_t
static vec4_t VEC3(float x, float y, float z)
{
	return vec4(x, y, z, 0.0f);
}
static uvec4_t UVEC3(uint32_t x, uint32_t y, uint32_t z)
{
	return uvec4(x, y, z, 0);
}
#	define VEC3i(x, y, z) {x, y, z, 0.0f}
#	define vecN_xyz(v) vec4_xyz(v)
#	define ZN Z4
#	define _vecN(a) _vec4(a)
#	define _uvecN(a) _uvec4(a)
#	define NDIMS 4
#	define dN d4
#	define dN_t d4_t
#	define D3(x, y, z) d4(x, y, z, 0.0f)
#else
#	define vecN vec3
#	define _vecN(a) _vec3(a)
#	define vecN_t vec3_t
#	define uvecN uvec3
#	define uvecN_t uvec3_t
static vec3_t VEC3(float x, float y, float z)
{
	return vec3(x, y, z);
}
static uvec3_t UVEC3(uint32_t x, uint32_t y, uint32_t z)
{
	return uvec3(x, y, z);
}
#	define VEC3i(x, y, z) {x, y, z}
#	define vecN_xyz(v) v
#	define _uvecN(a) _uvec3(a)
#	define ZN Z3
#	define dN d3_t
#	define D3(x, y, z) d3(x, y, z)
#	define NDIMS 3
#endif

typedef float(*modifier_cb)(struct mesh *mesh, float percent, void *usrptr);

typedef int(*iter_cb)(struct mesh *mesh, void *selection, void *usrptr);

#define EXP(e) e
#define vecN_(eq) CAT2(vecN, _##eq)
#define dN_(eq) CAT2(dN, _##eq)


#define SEL_UNSELECTED 0
#define SEL_EDITING 1

KHASH_MAP_INIT_INT(id, int)

typedef enum
{
	MESH_VERT,
	MESH_EDGE,
	MESH_FACE,
	MESH_ANY
} geom_t;

typedef struct vertex_t
{
	vecN_t pos;
	vec4_t color;
	/* vec3_t normal; */
	int half;
	int tmp;

	/* Skin */
	vec4_t wei;
	vec4_t bid;
} vertex_t;

#define v_half(v, m) (m_edge(m, v->half))

typedef struct edge /* Half edge */
{
	int v;
	vec3_t n; /* NORMAL OF v */
	vec3_t tg; /* TANGENT OF v */
	vec2_t t; /* TEXTURE COORD OF v */

	int face; /* face_t id								 */
	int pair; /* edge_t id		for triangle meshes only */
	int next; /* edge_t id								 */
	int prev; /* edge_t id								 */
	int cell_pair; /* edge_t id		for tetrahedral meshes only */

	int tmp;

} edge_t;

#define e_prev(e, m) (m_edge(m, (e)->prev))
/* Returns the previous edge_t* of edge:e in mesh:m */

#define e_next(e, m) (m_edge(m, (e)->next))
/* Returns the next edge_t* of edge:e in mesh:m */

#define e_cpair(e, m) (m_edge(m, (e)->cell_pair))
/* Returns the selected pair edge_t* of edge:e in mesh:m */

#define e_pair(e, m) (m_edge(m, (e)->pair))
/* Returns the pair edge_t* of edge:e in mesh:m */

#define e_face(e, m) (m_face(m, (e)->face))
/* Returns the face_t* of edge:e in mesh:m */

#define e_vert(e, m) (m_vert(m, (e)->v))
/* Returns the 0th vertex_t* from edge:e in mesh:m */

typedef struct face_t /* Half face */
{
	int e_size;
	int e[4]; /* edge_t[e_size] id */

	vec3_t n; /* flat normal, only applicable to triangles */


#ifdef MESH4
	int pair;
	int cell;
#endif

	int tmp;
} face_t;

#define f_edge(f, i, m) (m_edge(m, f->e[i]))
/* Returns the i'th edge_t* from face:f in mesh:m */

#define f_vert(f, i, m) (e_vert(f_edge(f, i, m), m))
/* Returns the i'th vertex_t* from face:f in mesh:m */

#ifdef MESH4
#define f_cell(f, m) m_cell(m, f->cell)
/* Returns the pair face_t* of face:f in mesh:m */
#define f_pair(f, m) m_face(m, f->pair)
/* Returns the pair face_t* of face:f in mesh:m */
#endif

#ifdef MESH4
typedef struct cell_t /* Cell */
{
	int f_size;
	int f[5]; /* face_t[f_size] id */

} cell_t;

#define c_face(c, i, m)		(m_face(m, c->f[i]))
/* Returns the i'th face_t* from cell:c in mesh:m */

#define c_edge(c, i, j, m)	(f_edge(c_face(c, i, m), j, m))
/* Returns the j'th edge_t* of the i'th face_t from cell:c in mesh:m */

#define c_vert(c, i, j, m)	(f_vert(c_face(c, i, m), j, m))
/* Returns the j'th vertex4_t* of the i'th face_t from cell:c in mesh:m */

#endif 

typedef struct
{
	khash_t(id) *faces;
	khash_t(id) *edges;
	khash_t(id) *verts;
#ifdef MESH4
	khash_t(id) *cells;
#endif
} mesh_selection_t;

typedef struct mesh
{
	vector_t *faces;
	vector_t *verts;
	vector_t *edges;
#ifdef MESH4
	vector_t *cells;
#endif

	mesh_selection_t selections[16];

	khash_t(id) *faces_hash;
	khash_t(id) *edges_hash;

	int has_texcoords;
	int triangulated;
	int static_normals;

	int current_cell;
	int current_surface;

	mat4_t transformation;

	support_cb support;

	char name[256];
	int locked_write;
	int locked_read;
	int update_id;
	int changes;

	void *mtx;

	/* DISPAY PROPERTIES */
	float offset;
	float smooth_angle;
	int ref_num;

	int has_skin;
} mesh_t;

typedef enum
{
	DIR_X,
	DIR_Y,
	DIR_Z
#ifdef MESH4
	, DIR_W
#endif
} direction_t;

#ifdef MESH4
#define m_cell(m, i) ((cell_t*)vector_get(m->cells, i))
#endif

#define m_face(m, i) ((face_t*)vector_get(m->faces, i))
#define m_edge(m, i) ((edge_t*)vector_get(m->edges, i))
#define m_vert(m, i) ((vertex_t*)vector_get(m->verts, i))

mesh_t *mesh_new(void);
mesh_t *mesh_clone(mesh_t *self);
void mesh_assign(mesh_t *self, mesh_t *other);
void mesh_destroy(mesh_t *self);

void mesh_load(mesh_t *self, const char *filename);
void mesh_circle(mesh_t *self, float radius, int segments, vecN_t dir);
void mesh_arc(mesh_t *self, float radius, int segments, vecN_t dir,
              float start, float end, bool_t closed);

void mesh_frame_cuboid(mesh_t *self, vec3_t p2, vec3_t p1);
void mesh_frame_capsule(mesh_t *self, float radius, vec3_t dir);
void mesh_frame_sphere(mesh_t *self, float radius);

void mesh_quad(mesh_t *self);
mesh_t *mesh_torus(float radius, float inner_radius, int segments,
		int inner_segments);
void mesh_disk(mesh_t *self, float radius, float inner_radius, int segments,
		vecN_t dir);
void mesh_cube(mesh_t *self, float size, float tex_scale);
void mesh_ico(mesh_t *self, float size);
void mesh_point_grid(mesh_t *self, vecN_t start, vecN_t size, uvecN_t segments);
void mesh_triangle_grid(mesh_t *self, uvec2_t segments);

void mesh_clear(mesh_t *self);
void mesh_subdivide(mesh_t *mesh, int subdivisions);
void mesh_spherize(mesh_t *mesh, float roundness);
mesh_t *mesh_lathe(mesh_t *mesh, float angle, int segments,
		float x, float y, float z);

void mesh_cuboid(mesh_t *self, float tex_scale, vec3_t p1, vec3_t p2);
/* mesh_t *mesh_cuboid(float tex_scale, */
		/* float x1, float y1, float z1, */
		/* float x2, float y2, float z2); */

void mesh_translate_uv(mesh_t *self, vec2_t p);
void mesh_scale_uv(mesh_t *self, float scale);

void mesh_lock(mesh_t *self);
void mesh_lock_write(mesh_t *self);
void mesh_unlock(mesh_t *self);
void mesh_unlock_write(mesh_t *self);
void mesh_update(mesh_t *self);
void mesh_modified(mesh_t *self);

void mesh_get_tg_bt(mesh_t *self);

void mesh_update_smooth_normals(mesh_t *self, float smooth_max);

int mesh_dup_vert(mesh_t *self, int i);
int mesh_add_vert(mesh_t *self, vecN_t p);
int mesh_add_vec3(mesh_t *self, vec3_t p);
int mesh_assert_vert(mesh_t *self, vecN_t pos);
int mesh_assert_vec3(mesh_t *self, vec3_t pos);
int mesh_append_edge(mesh_t *self, vecN_t p);
int mesh_add_edge_s(mesh_t *self, int v, int next);
int mesh_add_edge(mesh_t *self, int v, int next, int prev, vec3_t vn, vec2_t vt);
void mesh_link_edges(mesh_t *self, int e0, int e1);
int mesh_add_triangle(mesh_t *self,
		int v0, vec3_t v0n, vec2_t v0t,
		int v1, vec3_t v1n, vec2_t v1t,
		int v2, vec3_t v2n, vec2_t v2t);
int mesh_add_tetrahedron(mesh_t *self, int v0, int v1, int v2, int v3);
int mesh_add_tetrahedral_prism(mesh_t *self, int fid, int v0, int v1, int v2);
int mesh_add_triangle_s(mesh_t *self, int v0, int v1, int v2);
void mesh_check_pairs(mesh_t *self);
int mesh_remove_lone_faces(mesh_t *self);
int mesh_remove_lone_edges(mesh_t *self);
void mesh_remove_faces(mesh_t *self);
void mesh_remove_face(mesh_t *self, int face_i, bool_t face_only);
void mesh_remove_edge(mesh_t *self, int edge_i);
void mesh_remove_vert(mesh_t *self, int vert_i);
void mesh_select(mesh_t *self, int selection, geom_t geom, int id);
void mesh_unselect(mesh_t *self, int selection, geom_t geom, int id);
void mesh_paint(mesh_t *self, vec4_t color);
void mesh_weld(mesh_t *self, geom_t geom);
void mesh_for_each_selected(mesh_t *self, geom_t geom, iter_cb cb, void *usrptr);

void mesh_extrude_faces(mesh_t *self, int steps, vecN_t offset,
		float scale, modifier_cb scale_cb, modifier_cb offset_cb,
		void *usrptr);
void mesh_extrude_edges(mesh_t *self, int steps, vecN_t offset,
		float scale, modifier_cb scale_cb, modifier_cb offset_cb,
		void *usrptr);
void mesh_translate_points(mesh_t *self, float percent, vecN_t offset,
		float scale, modifier_cb scale_cb, modifier_cb offset_cb,
		void *usrptr);

void mesh_triangulate(mesh_t *self);
void mesh_invert_normals(mesh_t *self);
int mesh_edge_rotate_to_unpaired(mesh_t *self, int edge_id);

void mesh_add_quad(mesh_t *self,
		int v0, vec3_t v0n, vec2_t v0t,
		int v1, vec3_t v1n, vec2_t v1t,
		int v2, vec3_t v2n, vec2_t v2t,
		int v3, vec3_t v3n, vec2_t v3t);
int mesh_add_regular_quad( mesh_t *self,
	vecN_t p1, vec3_t n1, vec2_t t1, vecN_t p2, vec3_t n2, vec2_t t2,
	vecN_t p3, vec3_t n3, vec2_t t3, vecN_t p4, vec3_t n4, vec2_t t4
);

void mesh_verts_prealloc(mesh_t *self, int size);
void mesh_edges_prealloc(mesh_t *self, int size);
void mesh_faces_prealloc(mesh_t *self, int size);

int mesh_update_unpaired_edges(mesh_t *self);
int mesh_update_unpaired_faces(mesh_t *self);
int mesh_vert_has_face(mesh_t *self, vertex_t *vert, int face_id);

void mesh_translate(mesh_t *self, vec3_t t);
void mesh_rotate(mesh_t *self, float angle, int x, int y, int z);
mat4_t mesh_save(mesh_t *self);
void mesh_restore(mesh_t *self, mat4_t saved);

int mesh_update_flips(mesh_t *self);
vecN_t mesh_get_selection_center(mesh_t *self);
float mesh_get_selection_radius(mesh_t *self, vecN_t center);

vertex_t *mesh_farthest(mesh_t *self, const vec3_t dir);

/* COLLISIONS */
int mesh_gjk_intersection(mesh_t *self, mesh_t *other);

void meshes_reg(void);

#endif /* !MESH_H */
