CC = cc
LD = cc

DIR = build

DEPS =  $(shell sdl2-config --libs) -lglut -lGLU -lm -lGL -lGLEW

SHAD = $(patsubst %.glsl, $(DIR)/%.glsl.o, $(wildcard shaders/*.glsl))

SRCS = $(wildcard *.c) $(wildcard components/*.c) $(wildcard systems/*.c) \
	   $(wildcard formats/*.c) $(wildcard utils/*.c) $(wildcard vil/*.c) \
	   $(wildcard ecs/*.c)

OBJS_REL = $(patsubst %.c, $(DIR)/%.o, $(SRCS))
OBJS_DEB = $(patsubst %.c, $(DIR)/%.debug.o, $(SRCS))

CFLAGS = $(shell sdl2-config --cflags) -DUSE_VAO -Wall -I. -Wuninitialized \
	-Wstrict-prototypes $(PARENTCFLAGS)

CFLAGS_REL = $(CFLAGS) -O3

CFLAGS_DEB = $(CFLAGS) -g3

##############################################################################

all: $(DIR)/export.a
	echo -n $(DEPS) > $(DIR)/deps

$(DIR)/export.a: init $(OBJS_REL) $(SHAD)
	$(AR) rs $@ $(OBJS_REL) $(SHAD)

$(DIR)/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_REL)

$(DIR)/%.glsl.o: %.glsl
	@xxd -i $< > $(DIR)/$<.c
	@printf "\n#include <utils/shader.h>\n\
	void shaders_%s_glsl_reg(void) { \n\
	shader_add_source(\"%s.glsl\", shaders_%s_glsl, shaders_%s_glsl_len);}" \
	$(*F) $(*F) $(*F) $(*F) >> $(DIR)/$<.c
	$(CC) -o $@ -c $(DIR)/$<.c $(CFLAGS_REL)


##############################################################################

debug: $(DIR)/export_debug.a
	echo -n $(DEPS) > $(DIR)/deps

$(DIR)/export_debug.a: init $(OBJS_DEB) $(SHAD)
	$(AR) rs $@ $(OBJS_DEB) $(SHAD)

$(DIR)/%.debug.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_DEB)

##############################################################################

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
	rm -r $(DIR)
