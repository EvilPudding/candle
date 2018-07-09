CC = cc
LD = cc

DIR = build

SHAD = $(patsubst %.frag, $(DIR)/%.frag.o, $(wildcard shaders/*.frag)) \
	   $(patsubst %.vert, $(DIR)/%.vert.o, $(wildcard shaders/*.vert)) \

SRCS = $(wildcard *.c) $(wildcard components/*.c) $(wildcard systems/*.c) \
	   $(wildcard formats/*.c) $(wildcard utils/*.c)

OBJS_REL = $(patsubst %.c, $(DIR)/%.o, $(SRCS))
OBJS_DEB = $(patsubst %.c, $(DIR)/%.debug.o, $(SRCS))

CFLAGS = $(shell sdl2-config --cflags) -DUSE_VAO -Wall -I. -Wuninitialized -Wstrict-prototypes $(PARENTCFLAGS)

CFLAGS_REL = $(CFLAGS) -O3

CFLAGS_DEB = $(CFLAGS) -g3

##############################################################################

all: $(DIR)/export.a
	echo $(myarr)

$(DIR)/export.a: init $(OBJS_REL) $(SHAD)
	$(AR) rs $@ $(OBJS_REL) $(SHAD)

$(DIR)/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_REL)

$(DIR)/%.frag.o: %.frag
	@xxd -i $< > $(DIR)/$<.c
	@printf "\n#include <utils/shader.h>\n\
	void shaders_%s_frag_reg(void) { \n\
	shader_add_source(\"%s.frag\", shaders_%s_frag, shaders_%s_frag_len);}" \
	$(*F) $(*F) $(*F) $(*F) >> $(DIR)/$<.c
	$(CC) -o $@ -c $(DIR)/$<.c $(CFLAGS_REL)

$(DIR)/%.vert.o: %.vert
	@xxd -i $< $(DIR)/$<.c 
	@printf "\n#include <utils/shader.h>\n\
	void shaders_%s_vert_reg(void) { \n\
	shader_add_source(\"%s.vert\", shaders_%s_vert, shaders_%s_vert_len);}" \
	$(*F) $(*F) $(*F) $(*F) >> $(DIR)/$<.c 
	$(CC) -o $@ -c $(DIR)/$<.c $(CFLAGS_REL)


##############################################################################

debug: $(DIR)/export_debug.a

$(DIR)/export_debug.a: init $(OBJS_DEB) $(SHAD)
	$(AR) rs $@ $(OBJS_DEB) $(SHAD)

$(DIR)/%.debug.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_DEB)

##############################################################################

init:
	mkdir -p $(DIR)
	mkdir -p $(DIR)/components
	mkdir -p $(DIR)/systems
	mkdir -p $(DIR)/utils
	mkdir -p $(DIR)/formats
	mkdir -p $(DIR)/shaders

install: all debug
	cp $(DIR)/export.a /usr/lib/
	cp $(DIR)/export_debug.a /usr/lib/

##############################################################################

clean:
	rm -r $(DIR)
