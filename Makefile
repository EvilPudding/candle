CC = cc
LD = cc
AR = ar

emscripten: CC = emcc
emscripten: LD = emcc
emscripten: AR = emar

DIR = build
DEPS_REL = -lm -lGL -pthread -ldl -lX11 -lrt
DEPS_DEB = $(DEPS_REL)

GLFW = third_party/glfw/src
TINYCTHREAD = third_party/tinycthread/source

THIRD_PARTY_SRC = \
		   $(GLFW)/context.c \
		   $(GLFW)/init.c \
		   $(GLFW)/input.c \
		   $(GLFW)/monitor.c \
		   $(GLFW)/window.c \
		   $(GLFW)/vulkan.c \
		   $(GLFW)/egl_context.c \
		   $(GLFW)/glx_context.c \
		   $(GLFW)/linux_joystick.c \
		   $(GLFW)/osmesa_context.c \
		   $(GLFW)/posix_thread.c \
		   $(GLFW)/posix_time.c \
		   $(GLFW)/x11_init.c \
		   $(GLFW)/x11_monitor.c \
		   $(GLFW)/x11_window.c \
		   $(GLFW)/xkb_unicode.c \
		   $(TINYCTHREAD)/tinycthread.c

SRCS = $(wildcard *.c) $(wildcard components/*.c) $(wildcard systems/*.c) \
	   $(wildcard formats/*.c) $(wildcard utils/*.c) $(wildcard vil/*.c) \
	   $(wildcard ecs/*.c)

OBJS_REL = $(patsubst %.c, $(DIR)/%.o, $(THIRD_PARTY_SRC) $(SRCS))
OBJS_DEB = $(patsubst %.c, $(DIR)/%.debug.o, $(THIRD_PARTY_SRC) $(SRCS))
OBJS_EMS = $(patsubst %.c, $(DIR)/%.emscripten.o, $(TINYCTHREAD)/tinycthread.c $(SRCS))

CFLAGS = -std=c89 -pedantic -Wall -I. -Wno-unused-function \
         -Ithird_party/glfw/include -I$(TINYCTHREAD) -D_GLFW_X11 \
         -Wno-format-truncation -Wno-stringop-truncation $(PARENTCFLAGS) \
         -DTHREADED

CFLAGS_REL = $(CFLAGS) -O3

CFLAGS_DEB = $(CFLAGS) -g3 -DDEBUG

CFLAGS_EMS = -Wall -I. -Wno-unused-function \
			 -Ithird_party/glfw/include -I$(TINYCTHREAD) -D_GLFW_X11 \
			 $(PARENTCFLAGS) \
			 -s USE_GLFW=3 -s USE_WEBGL2=1 \
			 -s FULL_ES3=1 -s ALLOW_MEMORY_GROWTH=1 \
			 -s EMULATE_FUNCTION_POINTER_CASTS=1 \
			 -s WASM_MEM_MAX=2GB -O3

emscripten: CFLAGS := $(CFLAGS_EMS)

##############################################################################

all: $(DIR)/export.a
	echo -n $(DEPS_REL) > $(DIR)/deps

$(DIR)/export.a: init $(OBJS_REL) $(SHAD_REL)
	$(AR) rs $@ $(OBJS_REL) $(SHAD_REL)

$(DIR)/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_REL)

# utils/gl.c:
# 	galogen gl.xml --api gl --ver 3.0 --profile core --exts $(EXTS)
# 	mv gl.c gl.h utils/

##############################################################################

debug: $(DIR)/export_debug.a
	echo -n $(DEPS_DEB) > $(DIR)/deps

$(DIR)/export_debug.a: init $(OBJS_DEB) $(SHAD_REL)
	$(AR) rs $@ $(OBJS_DEB) $(SHAD_REL)

$(DIR)/%.debug.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_DEB)


##############################################################################

emscripten: $(DIR)/export_emscripten.a
	echo -n $(DEPS_EMS) > $(DIR)/deps

$(DIR)/export_emscripten.a: init $(OBJS_EMS) $(SHAD_EMS)
	$(AR) rs $@ $(OBJS_EMS) $(SHAD_EMS)

$(DIR)/%.emscripten.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_EMS)

##############################################################################

init:
	mkdir -p $(DIR)
	mkdir -p $(DIR)/components
	mkdir -p $(DIR)/systems
	mkdir -p $(DIR)/utils
	mkdir -p $(DIR)/formats
	mkdir -p $(DIR)/vil
	mkdir -p $(DIR)/ecs
	mkdir -p $(DIR)/$(GLFW)
	mkdir -p $(DIR)/$(TINYCTHREAD)

install: all debug
	cp $(DIR)/export.a /usr/lib/
	cp $(DIR)/export_debug.a /usr/lib/

##############################################################################

clean:
	rm -fr $(DIR)
