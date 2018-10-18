CC = gcc -std=gnu99
LD = gcc

DIR = build

SHAD = $(patsubst %.glsl, $(DIR)/%.glsl.o, $(wildcard shaders/*.glsl))

SRCS = $(wildcard *.c) $(wildcard components/*.c) $(wildcard systems/*.c) \
	   $(wildcard formats/*.c)

OBJS_REL = $(patsubst %.c, $(DIR)/%.o, $(SRCS))
OBJS_DEB = $(patsubst %.c, $(DIR)/%.debug.o, $(SRCS))

CFLAGS = -DGLEW_STATIC $(shell sdl2-config --cflags) -DUSE_VAO -Wall -I. \
		 -Wuninitialized -Wstrict-prototypes $(PARENTCFLAGS)

CFLAGS_REL = $(CFLAGS) -O3

CFLAGS_DEB = $(CFLAGS) -g3

##############################################################################

all: $(DIR)/candle.a

$(DIR)/candle.a: init $(OBJS_REL) $(SHAD)
	$(AR) rs $@ $(OBJS_REL) $(SHAD)

$(DIR)/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_REL)

$(DIR)/%.glsl.o: %.glsl
	@xxd -i $< > $(DIR)/$<.c
	@printf "\n#include <shader.h>\n\
	void shaders_%s_frag_reg(void) { \n\
	shader_add_source(\"%s.glsl\", shaders_%s_frag, shaders_%s_frag_len);}" \
	$(*F) $(*F) $(*F) $(*F) >> $(DIR)/$<.c
	$(CC) -o $@ -c $(DIR)/$<.c $(CFLAGS_REL)


##############################################################################

debug: $(DIR)/candle_debug.a

$(DIR)/candle_debug.a: init $(OBJS_DEB) $(SHAD)
	$(AR) rs $@ $(OBJS_DEB) $(SHAD)

$(DIR)/%.debug.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_DEB)

##############################################################################

init:
	mkdir -p $(DIR)
	mkdir -p $(DIR)/components
	mkdir -p $(DIR)/systems
	mkdir -p $(DIR)/formats
	mkdir -p $(DIR)/shaders

install: all debug
	cp $(DIR)/candle.a /usr/lib/
	cp $(DIR)/candle_debug.a /usr/lib/

##############################################################################

clean:
	rm -r $(DIR)
