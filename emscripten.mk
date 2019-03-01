CC = emcc
LD = emcc
AR = emar

DIR = build

EMOPTS = -s USE_SDL=2 -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s EMULATE_FUNCTION_POINTER_CASTS=1 \
		 -s SAFE_HEAP=1 -s WASM=1

DEPS =  $(EMOPTS)

SHAD = $(patsubst %.glsl, $(DIR)/%.glsl.o, $(wildcard shaders/*.glsl))

SRCS = $(wildcard *.c) $(wildcard components/*.c) $(wildcard systems/*.c) \
	   $(wildcard formats/*.c) $(wildcard utils/*.c) $(wildcard vil/*.c) \
	   $(wildcard ecs/*.c)

OBJS_REL = $(patsubst %.c, $(DIR)/%.o, $(SRCS))
OBJS_DEB = $(patsubst %.c, $(DIR)/%.debug.o, $(SRCS))

CFLAGS = $(EMOPTS) -DUSE_VAO -Wall -I. -Wuninitialized \
	-Wstrict-prototypes $(PARENTCFLAGS)

CFLAGS_REL = $(CFLAGS) -O2

CFLAGS_DEB = $(CFLAGS) -g3 -DDEBUG

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
