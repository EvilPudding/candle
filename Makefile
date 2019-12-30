CC = cc -std=c89 -pedantic-errors
LD = cc
AR = ar
COMMA = ,

emscripten: CC = emcc
emscripten: LD = emcc
emscripten: AR = emar

DIR = build
# EXTS = ARB_texture_cube_map,EXT_framebuffer_object,TEXTURE_RECTANGLE_ARB
DEPS_REL = $(shell sdl2-config --libs) -lm -lGL
DEPS_DEB = $(DEPS_REL)
SHAD_SRC = $(patsubst %.glsl, $(DIR)/%.glsl.c, $(wildcard shaders/*.glsl))

SRCS = $(wildcard *.c) $(wildcard components/*.c) $(wildcard systems/*.c) \
	   $(wildcard formats/*.c) $(wildcard utils/*.c) $(wildcard vil/*.c) \
	   $(wildcard ecs/*.c) shaders_reg.c

OBJS_REL = $(patsubst %.c, $(DIR)/%.o, $(SRCS))
OBJS_DEB = $(patsubst %.c, $(DIR)/%.debug.o, $(SRCS))
OBJS_EMS = $(patsubst %.c, $(DIR)/%.emscripten.o, $(SRCS))

CFLAGS = $(shell sdl2-config --cflags) -Wall -I. -Wuninitialized \
		 -Wno-unused-function -Wstrict-prototypes \
		 -Wno-format-truncation -Wno-stringop-truncation $(PARENTCFLAGS)

CFLAGS_REL = $(CFLAGS) -O3

CFLAGS_DEB = $(CFLAGS) -g3 -DDEBUG

CFLAGS_EMS = $(CFLAGS) \
			 -s USE_SDL=2 -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 \
			 -s FULL_ES3=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
			 -s EMULATE_FUNCTION_POINTER_CASTS=1

emscripten: CFLAGS := $(CFLAGS_EMS)

##############################################################################

all: $(DIR)/export.a
	echo -n $(DEPS_REL) > $(DIR)/deps

$(DIR)/export.a: init $(OBJS_REL) $(SHAD_REL)
	$(AR) rs $@ $(OBJS_REL) $(SHAD_REL)

$(DIR)/shaders_reg.o: $(DIR)/shaders_reg.c
	$(CC) -o $@ -c $< $(CFLAGS_REL)

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

$(DIR)/shaders_reg.debug.o: $(DIR)/shaders_reg.c
	$(CC) -o $@ -c $< $(CFLAGS_REL)

$(DIR)/%.debug.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_DEB)


##############################################################################

emscripten: $(DIR)/export_emscripten.a
	echo -n $(DEPS_EMS) > $(DIR)/deps

$(DIR)/export_emscripten.a: init $(OBJS_EMS) $(SHAD_EMS)
	$(AR) rs $@ $(OBJS_EMS) $(SHAD_EMS)

$(DIR)/shaders_reg.emscripten.o: $(DIR)/shaders_reg.c
	$(CC) -o $@ -c $< $(CFLAGS_REL)

$(DIR)/%.emscripten.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_EMS)

##############################################################################

$(DIR)/shaders_reg.c: $(SHAD_SRC)
	printf "#include <utils/shader.h>\n" > $@
	printf "$(patsubst %.c,#include<%.c>\n, $(SHAD_SRC))" >> $@
	printf "void shaders_reg_glsl(void) { " >> $@
	printf "$(patsubst $(DIR)/shaders/%.glsl.c, ADD_SRC(%);, $(SHAD_SRC))" >> $@
	printf "}" >> $@

$(DIR)/%.glsl.c: %.glsl
	@xxd -i $< > $(DIR)/$<.c

init:
	# git submodule update
	mkdir -p $(DIR)
	mkdir -p $(DIR)/components
	mkdir -p $(DIR)/systems
	mkdir -p $(DIR)/utils
	mkdir -p $(DIR)/formats
	mkdir -p $(DIR)/vil
	mkdir -p $(DIR)/ecs
	mkdir -p $(DIR)/shaders

install: all debug
	cp $(DIR)/export.a /usr/lib/
	cp $(DIR)/export_debug.a /usr/lib/

##############################################################################

clean:
	rm -fr $(DIR)
