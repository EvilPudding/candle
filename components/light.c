#include "light.h"
#include "node.h"
#include "spatial.h"
#include "model.h"
#include "../utils/nk.h"
#include "../utils/drawable.h"
#include "../utils/str.h"
#include "../candle.h"
#include "../systems/editmode.h"
#include "../systems/window.h"
#include <stdlib.h>
#include "../systems/render_device.h"
#include "../components/sprite.h"
#include "../utils/bulb.h"

static mesh_t *g_light;
extern vs_t *g_quad_vs;
extern mesh_t *g_quad_mesh;
/* entity_t g_light = entity_null; */
static int g_lights_num;
static mat_t *g_light_widget;

static mesh_t *g_histogram_mesh;
texture_t *g_histogram_buffer;
texture_t *g_histogram_accum;
static drawable_t g_histogram;

static int c_light_position_changed(c_light_t *self);

static void c_light_caustics_init(c_light_t *self)
{
	if(!g_histogram_buffer)
	{
		char *header = str_new(64);
		char *shader = str_new(64);
		const uint32_t histogram_resolution = 256;
		g_histogram_mesh = mesh_new();
		mesh_lock(g_histogram_mesh);
		mesh_point_grid(g_histogram_mesh,
		                VEC3(0.0, 0.0, -1.0),
		                VEC3(histogram_resolution, histogram_resolution, 1.0),
		                UVEC3(histogram_resolution, histogram_resolution, 2));
		mesh_unlock(g_histogram_mesh);
		g_histogram_buffer = texture_new_2D(histogram_resolution,
											histogram_resolution,
		                                    0, 3,
			buffer_new("depth", true, -1),
			buffer_new("pos", true, 4),
			buffer_new("color", false, 4));
		g_histogram_accum = texture_new_2D((histogram_resolution / 2) * 2,
		                                   (histogram_resolution / 2) * 3,
		                                   TEX_INTERPOLATE, 1,
			buffer_new("color", true, 4));

		str_cat(&header,
			"vec2 sampleCube(const vec3 v, out uint faceIndex, out float z)\n"
			"{\n"
			"	vec3 vAbs = abs(v);\n"
			"	float ma;\n"
			"	vec2 uv;\n"
			"	if(vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)\n"
			"	{\n"
			"		faceIndex = v.z < 0.0 ? 5u : 4u;\n"
			"		ma = 0.5 / vAbs.z;\n"
			"		uv = vec2(v.z < 0.0 ? -v.x : v.x, -v.y);\n"
			"		z = vAbs.z;\n"
			"	}\n"
			"	else if(vAbs.y >= vAbs.x)\n");
		str_cat(&header,
			"	{\n"
			"		faceIndex = v.y < 0.0 ? 3u : 2u;\n"
			"		ma = 0.5 / vAbs.y;\n"
			"		uv = vec2(v.x, v.y < 0.0 ? -v.z : v.z);\n"
			"		z = vAbs.y;\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		faceIndex = v.x < 0.0 ? 1u : 0u;\n"
			"		ma = 0.5 / vAbs.x;\n"
			"		uv = vec2(v.x < 0.0 ? v.z : -v.z, -v.y);\n"
			"		z = vAbs.x;\n"
			"	}\n"
			"	return uv * ma + 0.5;\n"
			"}\n");
		str_cat(&header,
			"BUFFER {\n"
			"	sampler2D pos;\n"
			"	sampler2D color;\n"
			"} buf;\n"
			"#define NEAR 0.1\n"
			"#define FAR 1000.0\n"
			"float linearize(float depth)\n"
			"{\n"
			"    return (2.0 * NEAR * FAR) / ((FAR + NEAR) - (2.0 * depth - 1.0) * (FAR - NEAR));\n"
			"}\n"
			"float unlinearize(float depth)\n"
			"{\n"
			"	return FAR * (1.0 - (NEAR / depth)) / (FAR - NEAR);\n"
			"}\n");
		str_cat(&header,
			"vec3 decode_normal(vec2 enc)\n"
			"{\n"
			"    vec2 fenc = enc * 4.0 - 2.0;\n"
			"    float f = dot(fenc,fenc);\n"
			"    float g = sqrt(1.0 - f / 4.0);\n"
			"    vec3 n;\n"
			"    n.xy = fenc * g;\n"
			"    n.z = 1.0 - f / 2.0;\n"
			"    return n;\n"
			"}\n");

		str_cat(&shader,
			"{\n"
			"	vec4 edir = texelFetch(buf.pos, ivec2(P.xy), 0);\n"
			"	vec3 dir;\n"
			"	if (P.z < 0.0)\n"
			"		dir = decode_normal(edir.xy);\n"
			"	else\n"
			"		dir = decode_normal(edir.zw);\n"
			"	uint face;\n"
			"	float z;\n"
			"	vec2 target = sampleCube(dir, face, z);\n"
			"	vec2 layer = vec2(float(face % 2u), float(face / 2u));\n"
			"	target = (layer + target) / vec2(2, 3);\n");
		str_cat(&shader,
			"	pos = vec4((target - 0.5) * 2.0, 0.0, 1.0);\n"
			"	vec4 color = texelFetch(buf.color, ivec2(P.xy), 0);\n"
			"	vec3 col;\n"
			"	if (P.z < 0.0)\n"
			"	{\n"
			"		float transm = 1.0 - color.a;\n"
			"		float dark = min(1.0 - abs(0.5 - transm) * 2.0, 1.0);\n"
			"		float light = clamp(transm * 2.0 - 1.0, 0.0, 1.0);\n"
			"		col = color.rgb * dark + light;\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		col = color.rgb;\n"
			"	}\n"
			"	$poly_color = vec4(col / 64.0, 1.0);\n"
			"}\n");


		drawable_init(&g_histogram, ref("histogram"));
		drawable_set_mesh(&g_histogram, g_histogram_mesh);
		drawable_set_vs(&g_histogram,
						vs_new("histogram", false, 2,
						       vertex_header_new(header),
						       vertex_modifier_new(shader)));
		str_free(header);
		str_free(shader);
	}
}

static void c_light_drawable_init(c_light_t *self)
{
	const bool_t changed = !g_light || !self->widget.mesh;
	if(!g_light)
	{
		g_light = mesh_new();
		mesh_lock(g_light);
		mesh_ico(g_light, -0.5f);
		mesh_select(g_light, SEL_EDITING, MESH_FACE, -1);
		mesh_subdivide(g_light, 1);
		mesh_spherize(g_light, 1.0f);
		mesh_unlock(g_light);

		g_light_widget = mat_new("light_widget", "default");
		mat1t(g_light_widget, ref("albedo.texture"),
		      texture_from_memory("bulb", (const char *)bulb_png, bulb_png_len));
		mat1f(g_light_widget, ref("albedo.blend"), 1.0f);
		mat4f(g_light_widget, ref("emissive.color"), vec4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	if (!self->widget.mesh)
	{
		drawable_init(&self->widget, ref("widget"));
		drawable_add_group(&self->widget, ref("selectable"));
		drawable_set_vs(&self->widget, sprite_vs());
		drawable_set_mat(&self->widget, g_light_widget);
		drawable_set_entity(&self->widget, c_entity(self));
		drawable_set_xray(&self->widget, true);
		drawable_set_mesh(&self->widget, g_quad_mesh);

		drawable_init(&self->draw, self->light_group);
		drawable_set_vs(&self->draw, model_vs());
		drawable_set_mesh(&self->draw, g_light);
		drawable_set_matid(&self->draw, self->id);
		drawable_set_entity(&self->draw, c_entity(self));
	}
	if (changed)
	{
		c_light_position_changed(self);
	}
}

void c_light_init(c_light_t *self)
{
	self->color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	self->radius = 5.0f;
	self->volumetric_intensity = 0.0f;
	self->visible = 1;
	self->shadow_cooldown = 1;
	self->caustics = false;

	self->ambient_group = ref("ambient");
	self->light_group = ref("light");
	self->visible_group = ref("shadow");
	self->caustics_group = ref("transparent");

	self->id = g_lights_num++;

	c_light_position_changed(self);
	world_changed();

}

void c_light_set_lod(c_light_t *self, uint32_t lod)
{
	if (self->lod != lod)
	{
		self->lod = lod;
		self->frames_passed = self->shadow_cooldown;
	}
}

probe_tile_t get_free_tile(uint32_t level, uint32_t *result_level);
void release_tile(probe_tile_t tile);
uint32_t get_level_size(uint32_t level);

extern texture_t *g_probe_cache;
static void c_light_add_caustics_passes(c_light_t *self)
{
	uint32_t i;
	vec2_t light_pos, light_size;
	c_light_caustics_init(self);

	light_pos = vec2_div(vec2(_vec2(self->tile.pos)),
	                     vec2(g_probe_cache->width, g_probe_cache->height));
	light_size = vec2_div(vec2(self->tile.size.x * 2, self->tile.size.y * 3),
	                      vec2(g_probe_cache->width, g_probe_cache->height));

	renderer_add_pass(self->renderer, "caustics_clear", NULL,
		~0, IGNORE_CAM_VIEWPORT,
		g_histogram_accum, NULL, 0, ~0, 2,
		opt_clear_color(vec4(0.0, 0.0, 0.0, 0.0), NULL),
		opt_cam(~0, NULL)
	);

	for (i = 0; i < 6; ++i)
	{
		renderer_add_pass(self->renderer, "caustics", "candle:caustics",
			self->caustics_group, IGNORE_CAM_VIEWPORT,
			g_histogram_buffer, g_histogram_buffer, 0, ~0, 3,
			opt_clear_depth(1.0f, NULL),
			opt_clear_color(vec4(0.0, 0.0, 0.0, 1.0), NULL),
			opt_cam(i, NULL)
		);
		renderer_add_pass(self->renderer, "caustics_accum0", "candle:color",
			ref("histogram"), IGNORE_CAM_VIEWPORT | ADD,
			g_histogram_accum, NULL, 0, ~0, 2,
			opt_cam(i, NULL),
			opt_tex("buf", g_histogram_buffer, NULL)
		);
	}

	renderer_add_pass(self->renderer, "caustics_copy", "candle:upsample",
		ref("quad"), DEPTH_DISABLE | IGNORE_CAM_VIEWPORT,
		g_probe_cache, NULL, 0, ~0, 4,
		opt_tex("buf", g_histogram_accum, NULL),
		opt_viewport(light_pos, light_size),
		opt_int("level", 0, NULL),
		opt_num("alpha", 1.0, NULL)
	);

	renderer_add_pass(self->renderer, "depth", "candle:shadow_map",
		self->caustics_group, CULL_DISABLE, g_probe_cache, g_probe_cache, 0, ~0, 1,
		opt_cam(~0, NULL)
	);
}

void c_light_set_caustics(c_light_t *self, bool_t value)
{
	self->caustics = value;
}

static void c_light_create_renderer(c_light_t *self)
{
	uint32_t f;
	texture_t *output;
	vec3_t p1[6], up[6];

	if (!g_probe_cache)
		return;

	self->renderer = renderer_new(1.0f);

	self->tile = get_free_tile(0, &self->lod);

	output = g_probe_cache;

	p1[0] = vec3(1.0, 0.0, 0.0);
	p1[1] = vec3(-1.0, 0.0, 0.0);
	p1[2] = vec3(0.0, 1.0, 0.0);
	p1[3] = vec3(0.0, -1.0, 0.0);
	p1[4] = vec3(0.0, 0.0, 1.0);
	p1[5] = vec3(0.0, 0.0, -1.0);

	up[0] = vec3(0.0,-1.0,0.0);
	up[1] = vec3(0.0, -1.0, 0.0);
	up[2] = vec3(0.0, 0.0, 1.0);
	up[3] = vec3(0.0, 0.0, -1.0);
	up[4] = vec3(0.0, -1.0, 0.0);
	up[5] = vec3(0.0, -1.0, 0.0);

	for(f = 0; f < 6; f++)
	{
		self->renderer->pos[f].x = self->tile.pos.x + (f % 2) * self->tile.size.x;
		self->renderer->pos[f].y = self->tile.pos.y + (f / 2) * self->tile.size.y;
		self->renderer->relative_transform[f] = mat4_invert(mat4_look_at(Z3, p1[f], up[f]));
		self->renderer->size[f] = self->tile.size;
	}
	self->renderer->camera_count = 6;
	self->renderer->width = self->tile.size.x;
	self->renderer->height = self->tile.size.y;

	renderer_add_pass(self->renderer, "depth", "candle:shadow_map", self->visible_group,
		CULL_DISABLE, output, output, 0, ~0, 2,
		opt_clear_depth(1.0f, NULL),
		opt_cam(~0, NULL)
	);
	if (self->caustics)
	{
		c_light_add_caustics_passes(self);
	}


	self->renderer->output = output;
}

void c_light_set_shadow_cooldown(c_light_t *self, uint32_t cooldown)
{
	self->shadow_cooldown = cooldown;
}

void c_light_set_groups(c_light_t *self, uint32_t visible_group,
		uint32_t ambient_group, uint32_t light_group, uint32_t caustics_group)
{
	self->visible_group = visible_group;
	self->caustics_group = caustics_group;
	if (!self->renderer)
	{
		c_light_create_renderer(self);
	}
	self->renderer->passes[0].draw_signal = visible_group;
	if (self->caustics)
	{
		self->renderer->passes[1].draw_signal = caustics_group;
	}

	self->ambient_group = ambient_group;
	self->light_group = light_group;
	self->modified = true;

}

void c_light_visible(c_light_t *self, uint32_t visible)
{
	drawable_set_mesh(&self->draw, visible ? g_light : NULL);
	if(!self->visible && visible)
	{
		self->frames_passed = self->shadow_cooldown;
		drawable_model_changed(&self->draw);
	}
	self->visible = visible;
}

static int c_light_position_changed(c_light_t *self)
{
	c_node_t *node = c_node(self);
	c_node_update_model(node);
	drawable_set_transform(&self->widget, node->model);
	self->modified = true;
	return CONTINUE;
}

static int c_light_pre_draw(c_light_t *self)
{
	if(!self->modified && (!self->renderer || renderer_updated(self->renderer)))
	{
		return CONTINUE;
	}

	if (self->modified)
	{
		c_light_drawable_init(self);
		if(self->radius == -1)
		{
			drawable_set_group(&self->draw, self->ambient_group);
			drawable_set_vs(&self->draw, g_quad_vs);
			if(self->visible)
			{
				drawable_set_mesh(&self->draw, g_quad_mesh);
			}
			self->modified = false;
		}
		else
		{
			c_node_t *node;
			vec3_t pos;
			mat4_t model;
			float scale;
			self->frames_passed++;
			if(self->frames_passed >= self->shadow_cooldown)
			{
				self->frames_passed = 0;
			}
			else
			{
				return CONTINUE;
			}

			if(!self->renderer)
			{
				c_light_create_renderer(self);
			}
			drawable_set_group(&self->draw, self->light_group);
			drawable_set_vs(&self->draw, model_vs());
			if(self->visible)
			{
				drawable_set_mesh(&self->draw, g_light);
			}
			node = c_node(self);
			pos = c_node_pos_to_global(node, vec3(0, 0, 0));
			model = mat4_translate(pos);
			scale = self->radius * 1.15f;
			model = mat4_scale_aniso(model, vec3(scale, scale, scale));

			drawable_set_transform(&self->draw, model);

			renderer_set_model(self->renderer, ~0, &node->model);
		}
	}

	if (self->radius != -1)
	{
		renderer_draw(self->renderer);
	}

	if (self->caustics && self->renderer)
	{
		if (!renderer_pass(self->renderer, ref("caustics")))
		{
			c_light_add_caustics_passes(self);
		}
	}

	self->modified = false;
	return CONTINUE;
}

int c_light_menu(c_light_t *self, void *ctx)
{
	int ambient;
	float power, last_power;
	bool_t changed = false;
	struct nk_colorf *old;
	union { struct nk_colorf nk; vec4_t v; } new;
	nk_layout_row_dynamic(ctx, 0, 1);

	ambient = self->radius == -1.0f;
	nk_checkbox_label(ctx, "ambient", &ambient);

	if(!ambient)
	{
		float volum;
		float rad = self->radius;
		if (rad < 0.0f) rad = 0.0f;
		volum = self->volumetric_intensity;
		if(self->radius < 0.0f) self->radius = 0.01;
		nk_property_float(ctx, "radius:", 0.01, &rad, 1000, 0.1, 0.05);
		if(rad != self->radius)
		{
			self->radius = rad;
			c_light_position_changed(self);
			changed = true;
		}
		nk_property_float(ctx, "volumetric:", -1.0, &volum, 10.0, 0.1, 0.05);
		if(volum != self->volumetric_intensity)
		{
			self->volumetric_intensity = volum;
			changed = true;
		}

	}
	else
	{
		self->radius = -1;
	}

	nk_layout_row_dynamic(ctx, 180, 1);

	old = (struct nk_colorf *)&self->color;
	new.nk = nk_color_picker(ctx, *old, NK_RGBA);

	if(memcmp(&new, &self->color, sizeof(vec4_t)))
	{
		self->color = new.v;
		changed = true;
	}

	last_power = power = (self->color.x + self->color.y + self->color.z) / 3.f;
	nk_layout_row_dynamic(ctx, 20, 1);
	nk_property_float(ctx, "power:", 0.0f, &power, 30.0f, 0.05f, 0.05f);
	if (fabs(power - last_power) > 0.04f)
	{
		if (last_power > 0.f)
		{
			self->color.x *= power / last_power;
			self->color.y *= power / last_power;
			self->color.z *= power / last_power;
		}
		else
		{
			self->color.x = power;
			self->color.y = power;
			self->color.z = power;
		}
		changed = true;
	}

	if (changed)
	{
		world_changed();
	}

	return CONTINUE;
}

static void c_light_destroy(c_light_t *self)
{
	release_tile(self->tile);
	drawable_set_mesh(&self->draw, NULL);
	drawable_set_mesh(&self->widget, NULL);
	if(self->renderer)
	{
		renderer_destroy(self->renderer);
	}
}

void ct_light(ct_t *self)
{
	ct_init(self, "light", sizeof(c_light_t));
	ct_add_dependency(self, ct_node);
	ct_set_init(self, (init_cb)c_light_init);
	ct_set_destroy(self, (destroy_cb)c_light_destroy);

	ct_add_listener(self, WORLD, 0, sig("component_menu"), c_light_menu);
	ct_add_listener(self, WORLD, 0, sig("world_pre_draw"), c_light_pre_draw);
	ct_add_listener(self, ENTITY, 0, sig("node_changed"), c_light_position_changed);
}

c_light_t *c_light_new(float radius, vec4_t color)
{
	c_light_t *self = component_new(ct_light);

	self->color = color;
	self->radius = radius;
	self->volumetric_intensity = 0.0f;

	return self;
}

