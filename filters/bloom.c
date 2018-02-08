#include "../systems/renderer.h"
#include "../components/mesh_gl.h"
#include "bloom.h"
#include "blur.h"

shader_t *g_bright_shader;

void filter_bloom_init()
{
	g_bright_shader = shader_new("bright");
}

void filter_bloom(c_renderer_t *renderer, float scale, texture_t *screen)
{
	texture_t *tmp = renderer->temp_buffers[0];
	/* RESIZE AND FILTER */
	shader_bind(g_bright_shader);

	texture_target(tmp, 0);

	shader_bind_screen(g_bright_shader, screen, 1, 1);

	c_mesh_gl_draw(c_mesh_gl(renderer->quad), NULL, 0);

	/* ----------------- */

	/* filter_blur(tmp, 1.0); */

	filter_blur(renderer, tmp, 1.0f);

	filter_blur(renderer, tmp, 1.0f);
	filter_blur(renderer, tmp, 1.0f);


	texture_target(screen, 0);
	
	shader_bind(renderer->quad_shader);

	shader_bind_screen(renderer->quad_shader, tmp, 1, 1);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	c_mesh_gl_draw(c_mesh_gl(renderer->quad), NULL, 0);

	glDisable(GL_BLEND);
}
