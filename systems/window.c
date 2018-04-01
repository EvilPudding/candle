#include "window.h"
#include <shader.h>
#include <candle.h>
#include <components/model.h>
#include <components/mesh_gl.h>
#include <components/node.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int window_width = 1360;
int window_height = 765;

static vs_t *g_quad_vs = NULL;
static fs_t *g_quad_fs = NULL;

extern SDL_GLContext *context;
extern SDL_Window *mainWindow;

static void init_context_b(c_window_t *self)
{
	mainWindow = self->window = SDL_CreateWindow("Candle",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			self->width, self->height,
			SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
			| SDL_WINDOW_ALLOW_HIGHDPI);

	context = self->context = SDL_GL_CreateContext(self->window);

	if( context == NULL )
	{
		printf("OpenGL context could not be created! SDL Error: %s\n",
				SDL_GetError());
		exit(1);
	}

	glInit();

	glDepthFunc(GL_LESS); glerr();

	const GLubyte *renderer = glGetString( GL_RENDERER );
	const GLubyte *vendor = glGetString( GL_VENDOR );
	const GLubyte *version = glGetString( GL_VERSION );
	const GLubyte *glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);

	GLint major, minor;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	printf("GL Vendor : %s\n", vendor);
	printf("GL Renderer : %s\n", renderer);
	printf("GL Version (string) : %s\n", version);
	printf("GL Version (integer) : %d.%d\n", major, minor);
	printf("GLSL Version : %s\n", glslVersion); 

}

void c_window_init(c_window_t *self)
{
	SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS);
	/* SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, */
			/* SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); */

	self->width = window_width;
	self->height = window_height;

	/* SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); */

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
			SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	self->key_state = SDL_GetKeyboardState(NULL);

}

int c_window_toggle_fullscreen_gl(c_window_t *self)
{

	self->fullscreen = !self->fullscreen;

	SDL_DisplayMode dm;
	if(SDL_GetDesktopDisplayMode(0, &dm) != 0)
	{
		SDL_Log("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
		return 1;
	}
	self->width = dm.w;
	self->height = dm.h;

	printf("window resize: %dx%d\n", self->width, self->height);
	entity_signal(entity_null, sig("window_resize"),
		&(window_resize_data){.width = self->width, .height = self->height});

	SDL_SetWindowSize(self->window, self->width, self->height);

	SDL_SetWindowFullscreen(self->window,
		self->fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);

	return 1;
}
void c_window_toggle_fullscreen(c_window_t *self)
{
	loader_push(candle->loader, (loader_cb)c_window_toggle_fullscreen_gl,
		NULL, (c_t*)self);
}

void c_window_handle_resize(c_window_t *self, const SDL_Event event)
{
	self->width = event.window.data1;
	self->height = event.window.data2;
	printf("window resize: %dx%d\n", self->width, self->height);

	entity_signal(entity_null, sig("window_resize"),
		&(window_resize_data){.width = self->width, .height = self->height});
}

int c_window_created(c_window_t *self)
{
	init_context_b(self);


	if(!g_quad_vs)
	{
		g_quad_vs = vs_new("quad", 1, vertex_modifier_new(
			/* "texcoord *= screen_size;\n" */
			""
		));
		g_quad_fs = fs_new("quad");
	}

	entity_add_component(c_entity(self), c_model_new(mesh_quad(), NULL, 0));
	c_model(self)->visible = 0;
	c_model_cull_face(c_model(self), 0, 2);


	entity_signal(entity_null, sig("window_resize"),
			&(window_resize_data){
			.width = self->width,
			.height = self->height});

	return 1;
}

c_window_t *c_window_new(int width, int height)
{
	c_window_t *self = component_new("c_window");
	self->width = width ? width : window_width;
	self->height = height ? height : window_height;
	return self;
}

int c_window_draw(c_window_t *self)
{
	SDL_GL_SwapWindow(self->window);
	return 1;
}

int c_window_render_quad(c_window_t *self, texture_t *texture)
{

	/* fs_bind(g_quad_fs); */
	shader_t *shader = vs_bind(g_quad_vs);
	if(!shader || !shader->ready) return 0;

	if(texture)
	{
		shader_bind_screen(shader, texture, 1, 1);
	}
	c_node_t *node = c_node(self);
	if(node)
	{
		c_node_update_model(node);

		shader_update(shader, &node->model);
	}

	c_mesh_gl_draw(c_mesh_gl(self), 0);
	return 1;
}

void c_window_rect(c_window_t *self, int x, int y, int width, int height,
		texture_t *texture)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0); glerr();
	if(!texture) return;
	fs_bind(g_quad_fs);
	shader_t *shader = vs_bind(g_quad_vs);
	if(!shader || !shader->ready)
	{
		exit(1);
	}

	/* Draw to opengl context */

	glViewport(x, y, width, height); glerr();

	shader_bind_screen(shader, texture, 1, 1);
	glUniform2f(shader->u_screen_size,
			self->width,
			self->height); glerr();

	c_mesh_gl_draw(c_mesh_gl(self), 0);

}


REG()
{
	ct_t *ct = ct_new("c_window", sizeof(c_window_t), (init_cb)c_window_init,
			0);

	ct_listener(ct, ENTITY, sig("entity_created"), c_window_created);

	signal_init(sig("window_resize"), sizeof(window_resize_data));

	ct_listener(ct, WORLD, sig("render_quad"), c_window_render_quad);
}
