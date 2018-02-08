#include "../components/mesh_gl.h"
#include "../systems/renderer.h"
#include "blur.h"

shader_t *g_blur_shader;

void filter_blur_init()
{
	g_blur_shader = shader_new("blur");
}

void filter_blur(c_renderer_t *renderer, texture_t *screen, float scale)
{
	/* HORIZONTAL BLUR */

	shader_bind(g_blur_shader);

	texture_target(renderer->temp_buffers[1], 0);

	shader_bind_screen(g_blur_shader, screen, 1, 1);

	glUniform1i(g_blur_shader->u_horizontal_blur, 1);
	c_mesh_gl_draw(c_mesh_gl(renderer->quad), NULL, 0);

	/* VERTICAL BLUR */

	texture_target(screen, 0);

	shader_bind_screen(g_blur_shader, renderer->temp_buffers[1], 1, 1);

	glUniform1i(g_blur_shader->u_horizontal_blur, 0);
	c_mesh_gl_draw(c_mesh_gl(renderer->quad), NULL, 0);
}
