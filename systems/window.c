#include <utils/glutil.h>
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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

int window_width = 1360;
int window_height = 766;

vs_t *g_quad_vs;
fs_t *g_quad_fs;
mesh_t *g_quad_mesh;

extern SDL_GLContext *context;
extern SDL_Window *mainWindow;

static void init_context_b(c_window_t *self)
{
/* #ifdef __EMSCRIPTEN__ */

/* 	emscripten_set_canvas_element_size("#canvas", self->width, self->height); */

/* 	EmscriptenWebGLContextAttributes attrs; */
/* 	emscripten_webgl_init_context_attributes(&attrs); */
/* 	attrs.antialias = false; */
/* 	attrs.majorVersion = 2; */
/* 	attrs.minorVersion = 0; */
/* 	attrs.alpha = false; */
/* 	attrs.depth = true; */
/* 	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(0, &attrs); */

/* 	if (emscripten_webgl_enable_extension(ctx, "EXT_color_buffer_float") == EM_FALSE) printf("%d\n", __LINE__); */
/* 	if (emscripten_webgl_enable_extension(ctx, "GL_EXT_color_buffer_float") == EM_FALSE) printf("%d\n", __LINE__); */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "EXT_texture_filter_anisotropic") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "GL_EXT_texture_filter_anisotropic") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "OES_texture_float_linear") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "GL_OES_texture_float_linear") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "WEBGL_lose_context") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "WEBGL_depth_texture") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "GL_WEBGL_lose_context") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	emscripten_webgl_make_context_current(ctx); */
/* #else */
	mainWindow = self->window = SDL_CreateWindow("Candle",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			self->width, self->height,
			SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
			| SDL_WINDOW_ALLOW_HIGHDPI);
	if (!mainWindow)
	{
		printf("Window could not be created! SDL Error: %s\n",
				SDL_GetError());
	}

	context = self->context = SDL_GL_CreateContext(self->window);

	if( context == NULL )
	{
		printf("OpenGL context could not be created! SDL Error: %s\n",
				SDL_GetError());
		exit(1);
	}
/* #endif */

#ifdef __EMSCRIPTEN__
	if (!SDL_GL_ExtensionSupported("EXT_color_buffer_float")) {
		exit(1);
	}
	/* emscripten_run_script( */
	/* 		"function throwOnGLError(err, funcName, args) {\n" */
	/* 		"throw WebGLDebugUtils.glEnumToString(err) + ' was caused by call to: ' + funcName;\n" */
	/* 		"};\n" */
	/* 		"canvas = document.getElementById('canvas');\n" */
	/* 		"gl = canvas.getContext('webgl2');\n" */
	/* 		"gl = WebGLDebugUtils.makeDebugContext(gl, throwOnGLError);\n"); */
#endif

	glInit();

	glDepthFunc(GL_LESS); glerr();
	const GLubyte *renderer = glGetString( GL_RENDERER );
	const GLubyte *vendor = glGetString( GL_VENDOR );
	const GLubyte *version = glGetString( GL_VERSION );
	const GLubyte *glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);

	/* GLint major, minor; */
	/* glGetIntegerv(GL_MAJOR_VERSION, &major); */
	/* glGetIntegerv(GL_MINOR_VERSION, &minor); */
	/* glerr(); */
	printf("GL Vendor : %s\n", vendor);
	printf("GL Renderer : %s\n", renderer);
	printf("GL Version (string) : %s\n", version);
	/* printf("GL Version (integer) : %d.%d\n", major, minor); */
	printf("GLSL Version : %s\n", glslVersion); 

}

void c_window_init(c_window_t *self)
{
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS);
	/* SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, */
			/* SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); */

	SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
	self->width = window_width;
	self->height = window_height;

	/* SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); */

#ifdef __EMSCRIPTEN__
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
			SDL_GL_CONTEXT_PROFILE_CORE);
#endif

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

void c_window_handle_resize(c_window_t *self, const void *event)
{
	
	self->width = ((const SDL_Event*)event)->window.data1;
	self->height = ((const SDL_Event*)event)->window.data2;
	/* printf("window resize: %dx%d\n", self->width, self->height); */
	if(self->renderer)
	{
		renderer_resize(self->renderer, self->width, self->height);
	}

	entity_signal(entity_null, sig("window_resize"),
		&(window_resize_data){.width = self->width, .height = self->height}, NULL);
}

#ifdef DEBUG
void c_window_debug(GLenum source,
            GLenum type,
            GLuint id,
            GLenum severity,
            GLsizei length,
            const GLchar *message,
            c_window_t *self)
{
	if( severity == GL_DEBUG_SEVERITY_LOW ||
		severity == GL_DEBUG_SEVERITY_MEDIUM ||
		severity == GL_DEBUG_SEVERITY_HIGH)
	{
		printf("OpenGL debug: %.*s\n", length, message);
	}
}
#endif

int c_window_created(c_window_t *self)
{
	init_context_b(self);

#ifdef DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_TRUE);
	glDebugMessageCallback((void*)c_window_debug, self);

#endif
	if(!g_quad_vs)
	{
		g_quad_vs = vs_new("quad", false, 1, vertex_modifier_new(
			/* "texcoord *= screen_size;\n" */
			""
		));
		g_quad_fs = fs_new("quad");
	}

	g_quad_mesh = mesh_new();
	mesh_quad(g_quad_mesh);
	g_quad_mesh->cull = 0;

	drawable_init(&self->draw, ref("quad"));
	drawable_set_entity(&self->draw, c_entity(self));
	drawable_set_mesh(&self->draw, g_quad_mesh);
	drawable_set_vs(&self->draw, g_quad_vs);
	SDL_GL_SetSwapInterval(0);

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

void c_window_lock_fps(c_window_t *self, int32_t lock_fps)
{
	SDL_GL_SetSwapInterval(lock_fps);
}

int c_window_draw(c_window_t *self)
{
	if(!self->renderer) return CONTINUE;

	/* renderer_draw(self->renderer); */

	texture_t *tex = self->renderer->output;

	if(!tex)
	{
		printf("No texture to draw\n");
		return CONTINUE;
	}

	/* glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE); glerr(); */
	glBindFramebuffer(GL_FRAMEBUFFER, 0); glerr();
	/* glClear(GL_COLOR_BUFFER_BIT); */
	glClear(GL_DEPTH_BUFFER_BIT); glerr();

	fs_bind(g_quad_fs);

	glViewport(0, 0, self->width, self->height); glerr();

	shader_t *shader = vs_bind(g_quad_vs, 0);
	uint32_t uni = shader_cached_uniform(shader, ref("tex"));
	glUniform1i(uni, 0);
	glActiveTexture(GL_TEXTURE0);
	texture_bind(tex, tex->draw_id);

	uint32_t ss = shader_cached_uniform(shader, ref("screen_size"));
	glUniform2f(ss, self->width, self->height); glerr();

	drawable_draw(&self->draw);
	glerr();

	glActiveTexture(GL_TEXTURE0); glerr();
	glBindTexture(GL_TEXTURE_2D, 0); glerr();
	return CONTINUE;
}

REG()
{
	ct_t *ct = ct_new("window", sizeof(c_window_t), c_window_init, NULL, 0);

	ct_listener(ct, ENTITY, sig("entity_created"), c_window_created);
	ct_listener(ct, WORLD | 10, sig("world_draw"), c_window_draw);

	signal_init(sig("window_resize"), sizeof(window_resize_data));
}
