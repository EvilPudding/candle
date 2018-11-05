#include "glutil.h"
#include <stdio.h>
#include <candle.h>
#include <stdlib.h>
#include <string.h>


void _check_gl_error(const char *file, int line)
{
	static char last_error[512] = "";
	static int count = 0;
	char message[512];
	GLenum err = glGetError();
	int got_error = 0;
	while(err!=GL_NO_ERROR)
	{
		char *error = NULL;
		switch(err)
		{
			case GL_INVALID_OPERATION:      error="INVALID_OPERATION";      break;
			case GL_INVALID_ENUM:           error="INVALID_ENUM";           break;
			case GL_INVALID_VALUE:          error="INVALID_VALUE";          break;
			case GL_OUT_OF_MEMORY:          error="OUT_OF_MEMORY";          break;
			case GL_INVALID_FRAMEBUFFER_OPERATION:  error="INVALID_FRAMEBUFFER_OPERATION";  break;
		}
		snprintf(message, sizeof(message), "GL_%s - %s:%d", error, file, line);
		if(!strncmp(last_error, message, sizeof(last_error)))
		{
			printf("\b\b\r%s%6d\n", message, ++count);
		}
		else
		{
			count = 0;
			printf("%s\n", message);
			strncpy(last_error, message, sizeof(last_error));
		}

		err=glGetError();
		got_error = 1;
	}
	if(got_error)
	{
		exit(1);
	}
	if(SDL_ThreadID() != g_candle->render_id)
	{
		printf("glerr called in non render thread\n");
		exit(1);
	}
}

/* SDL_sem *glSem = NULL; */
SDL_GLContext *context = NULL;
SDL_Window *mainWindow = NULL;

void glInit()
{
	glewExperimental = GL_TRUE; 
	GLenum err = glewInit();

	if (err != GLEW_OK)
	{
		printf("Glew failed to initialize.\n");
		exit(1);
	}
	if (!GLEW_VERSION_2_1)  // check that the machine supports the 2.1 API.
	{
		printf("Glew version.\n");
		exit(1);
	}
	/* glSem = SDL_CreateSemaphore(1); */
}

/* void _glLock(const char *file, int line) */
/* { */
/* 	SDL_SemWait(glSem); */

/* 	if(context) */
/* 		SDL_GL_MakeCurrent(mainWindow, context); */ 
/* 	else */
/* 		printf("No context!\n"); */
/* 	glerr(); */
/* } */

/* void _glUnlock(const char *file, int line) */
/* { */
/* 	glerr(); */
/* 	SDL_SemPost(glSem); */
/* } */

