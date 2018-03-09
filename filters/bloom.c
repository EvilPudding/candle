#include "../systems/renderer.h"
#include "../components/mesh_gl.h"
#include "bloom.h"
#include "blur.h"

fs_t *g_bright_shader;

void filter_bloom_init()
{
	g_bright_shader = fs_new("bright");
}

void filter_bloom(c_renderer_t *renderer, float scale, texture_t *screen)
{
	texture_t *tmp = renderer->temp_buffers[0];
	/* RESIZE AND FILTER */
	fs_bind(g_bright_shader);

	texture_target(tmp, 0);

	entity_signal(c_entity(renderer), render_quad, screen);

	/* ----------------- */

	/* filter_blur(tmp, 1.0); */

	filter_blur(renderer, tmp, 1.0f);

	filter_blur(renderer, tmp, 1.0f);
	filter_blur(renderer, tmp, 1.0f);


	texture_target(screen, 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	entity_signal(c_entity(renderer), render_quad, tmp);

	glDisable(GL_BLEND);

}
