#include "window.h"
#include <utils/shader.h>
#include <utils/drawable.h>
#include <candle.h>
#include <components/model.h>
#include <components/node.h>
#include <systems/render_device.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int window_width = 1360;
int window_height = 765;

vs_t *g_quad_vs;
fs_t *g_quad_fs;
mesh_t *g_quad_mesh;

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

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
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

	/* printf("window resize: %dx%d\n", self->width, self->height); */
	entity_signal(entity_null, sig("window_resize"),
		&(window_resize_data){.width = self->width, .height = self->height}, NULL);

	SDL_SetWindowSize(self->window, self->width, self->height);

	SDL_SetWindowFullscreen(self->window,
		self->fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);

	return 1;
}

void c_window_toggle_fullscreen(c_window_t *self)
{
	loader_push(g_candle->loader, (loader_cb)c_window_toggle_fullscreen_gl,
		NULL, (c_t*)self);
}

void c_window_handle_resize(c_window_t *self, const SDL_Event event)
{
	self->width = event.window.data1;
	self->height = event.window.data2;
	/* printf("window resize: %dx%d\n", self->width, self->height); */
	if(self->renderer)
	{
		renderer_resize(self->renderer, self->width, self->height);
	}

	entity_signal(entity_null, sig("window_resize"),
		&(window_resize_data){.width = self->width, .height = self->height}, NULL);
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

	g_quad_mesh = mesh_new();
	mesh_quad(g_quad_mesh);
	g_quad_mesh->cull = 0;

	drawable_init(&self->draw, ref("quad"), NULL);
	drawable_set_entity(&self->draw, c_entity(self));
	drawable_set_mesh(&self->draw, g_quad_mesh);
	drawable_set_vs(&self->draw, g_quad_vs);

/* 	entity_signal(entity_null, sig("window_resize"), */
/* 			&(window_resize_data){ */
/* 			.width = self->width, */
/* 			.height = self->height}, NULL); */

	return CONTINUE;
}

c_window_t *c_window_new(int width, int height)
{
	c_window_t *self = component_new("window");
	self->width = width ? width : window_width;
	self->height = height ? height : window_height;
	return self;
}

void c_window_draw(c_window_t *self)
{
	if(!self->renderer) return;

	renderer_draw(self->renderer);

	texture_t *tex = self->renderer->output;

	if(!tex)
	{
		printf("No texture to draw\n");
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0); glerr();
	/* glClear(GL_COLOR_BUFFER_BIT); */
	glClear(GL_DEPTH_BUFFER_BIT);

	fs_bind(g_quad_fs);

	glViewport(0, 0, self->width, self->height); glerr();

	shader_t *shader = vs_bind(g_quad_vs);
	uint uni = shader_uniform(shader, "tex", NULL);
	glUniformHandleui64ARB(uni, tex->bufs[tex->draw_id].handle);

	glUniform2f(22, self->width, self->height); glerr();

	drawable_draw(&self->draw);
	glerr();
}

int c_window_mouse_press(c_window_t *self, mouse_button_data *event)
{
	float depth;
	entity_t ent = renderer_entity_at_pixel(self->renderer, event->x, event->y, &depth);
	unsigned int geom = renderer_geom_at_pixel(self->renderer, event->x, event->y, &depth);

	if(ent)
	{
		model_button_data ev = {
			.x = event->x, .y = event->y, .direction = event->direction,
			.depth = depth, .geom = geom, .button = event->button
		};
		return entity_signal_same(ent, ref("model_press"), &ev, NULL);
	}
	return CONTINUE;
}

int c_window_mouse_release(c_window_t *self, mouse_button_data *event)
{

	float depth;
	entity_t ent = renderer_entity_at_pixel(self->renderer, event->x, event->y,
			&depth);
	unsigned int geom = renderer_geom_at_pixel(self->renderer, event->x,
			event->y, &depth);

	if(ent)
	{
		model_button_data ev = {
			.x = event->x, .y = event->y, .direction = event->direction,
			.depth = depth, .geom = geom, .button = event->button
		};
		return entity_signal_same(ent, ref("model_release"), &ev, NULL);
	}
	return CONTINUE;
}

REG()
{
	ct_t *ct = ct_new("window", sizeof(c_window_t), c_window_init, NULL, 0);

	ct_listener(ct, ENTITY, sig("entity_created"), c_window_created);
	ct_listener(ct, WORLD | 100, sig("world_draw"), c_window_draw);
	ct_listener(ct, WORLD | 100, ref("mouse_press"), c_window_mouse_press);
	ct_listener(ct, WORLD | 100, ref("mouse_release"), c_window_mouse_release);


	signal_init(ref("model_press"), 0);
	signal_init(ref("model_release"), 0);
	signal_init(sig("window_resize"), sizeof(window_resize_data));
}
