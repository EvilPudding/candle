#include "window.h"
#include "../shader.h"
#include "../candle.h"

unsigned long ct_window;
unsigned long window_resize;

int window_width = 1360;
int window_height = 740;

extern SDL_GLContext *context;
extern SDL_Window *mainWindow;

static void init_context_b(c_window_t *self)
{
	mainWindow = self->window = SDL_CreateWindow("Shift", 0, 0,
			self->width, self->height, SDL_WINDOW_OPENGL);

	context = self->context = SDL_GL_CreateContext(self->window);

	if( context == NULL )
	{
		printf("OpenGL context could not be created! SDL Error: %s\n",
				SDL_GetError());
		exit(1);
	}

	glInit();

	glDepthFunc(GL_LESS); glerr();

	SDL_GL_SetSwapInterval(0);

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
	self->super = component_new(ct_window);
	SDL_Init(SDL_INIT_VIDEO);

	self->width = window_width;
	self->height = window_height;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
			SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	init_context_b(self);

	self->key_state = SDL_GetKeyboardState(NULL);

	SDL_SetWindowGrab(self->window, SDL_TRUE);
	SDL_SetRelativeMouseMode(SDL_TRUE);

	self->quad_shader = shader_new("quad");
	self->quad = entity_new(candle->ecm, 2,
			c_name_new("renderer_quad"),
			c_model_new(mesh_quad(), NULL, 0));
	c_model(self->quad)->visible = 0;
}

int c_window_created(c_window_t *self)
{
	entity_signal(c_entity(self), window_resize,
			&(window_resize_data){
			.width = self->width,
			.height = self->height});
	return 1;
}

c_window_t *c_window_new(int width, int height)
{
	c_window_t *self = calloc(1, sizeof *self);
	c_window_init(self);
	self->width = width ? width : window_width;
	self->height = height ? height : window_height;
	return self;
}

int c_window_draw(c_window_t *self)
{
	SDL_GL_SwapWindow(self->window);
	return 1;
}

void c_window_rect(c_window_t *self, int x, int y, int width, int height,
		texture_t *texture)
{
	if(!texture) return;

	/* Draw to opengl context */
	glBindFramebuffer(GL_FRAMEBUFFER, 0); glerr();

	glViewport(x, y, width, height);

	shader_bind(self->quad_shader);

	shader_bind_screen(self->quad_shader, texture, 1, 1);

	c_mesh_gl_draw(c_mesh_gl(self->quad));

}

void c_window_register(ecm_t *ecm)
{
	ct_t *ct = ecm_register(ecm, &ct_window, sizeof(c_window_t),
			(init_cb)c_window_init, 0);

	ct_register_listener(ct, SAME_ENTITY, entity_created,
			(signal_cb)c_window_created);

	window_resize = ecm_register_signal(ecm, sizeof(window_resize_data));
}
