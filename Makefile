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

PLUGINS = $(wildcard ../*.candle)
PLUGINS_REL = $(patsubst %, %/$(DIR)/candle.a, $(PLUGINS))
PLUGINS_DEB = $(patsubst %, %/$(DIR)/candle_debug.a, $(PLUGINS))
PLUGINS_EMS = $(patsubst %, %/$(DIR)/candle_emscripten.a, $(PLUGINS))

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
		   $(TINYCTHREAD)/tinycthread.c \
		   third_party/miniz.c

SRCS = $(wildcard *.c) $(wildcard components/*.c) $(wildcard systems/*.c) \
	   $(wildcard formats/*.c) $(wildcard utils/*.c) $(wildcard vil/*.c) \
	   $(wildcard ecs/*.c)

OBJS_REL = $(patsubst %.c, $(DIR)/%.o, $(THIRD_PARTY_SRC) $(SRCS))
OBJS_DEB = $(patsubst %.c, $(DIR)/%.debug.o, $(THIRD_PARTY_SRC) $(SRCS))
OBJS_EMS = $(patsubst %.c, $(DIR)/%.emscripten.o, $(TINYCTHREAD)/tinycthread.c $(SRCS))

CFLAGS = -std=c89 -pedantic -Wall -I. -Wno-unused-function \
         -Ithird_party/glfw/include -I$(TINYCTHREAD) -D_GLFW_X11 \
         -Wno-format-truncation -Wno-stringop-truncation $(PARENTCFLAGS) \
         -DTHREADED -DUSE_VAO

CFLAGS_REL = $(CFLAGS) -g3

CFLAGS_DEB = $(CFLAGS) -g3 -DDEBUG

CFLAGS_EMS = -Wall -I. -Wno-unused-function \
			 -Ithird_party/glfw/include -I$(TINYCTHREAD) -D_GLFW_X11 \
			 $(PARENTCFLAGS) \
			 -s USE_GLFW=3 -s USE_WEBGL2=1 \
			 -s FULL_ES3=1 -s ALLOW_MEMORY_GROWTH=1 \
			 -s EMULATE_FUNCTION_POINTER_CASTS=1 \
			 -s WASM_MEM_MAX=2GB -O3 -s WASM=1 -s ASSERTIONS=1 \
			 -s SAFE_HEAP=1

emscripten: CFLAGS := $(CFLAGS_EMS)

##############################################################################

all: $(DIR)/candle.a

$(DIR)/data.zip: $(DIR)/packager
	$(DIR)/packager ../$(SAUCES)

$(DIR)/packager: 
	$(CC) packager/packager.c third_party/miniz.c -O3 -o $(DIR)/packager

../%/$(DIR)/export.a:
	$(MAKE) -C $(patsubst %/$(DIR)/export.a, %, $@)
	echo -n " " >> $(DIR)/libs
	echo -n %/$(DIR)/export.a >> $(DIR)/libs
	echo -n " " >> $(DIR)/libs
	-cat $(patsubst ../%/$(DIR)/export.a, ../%/$(DIR)/libs, $@) >> $(DIR)/libs


$(DIR)/candle.a: init $(OBJS_REL) $(DIR)/data.zip
	$(AR) rs $@ $(OBJS_REL)
	echo -n candle/$(DIR)/candle.a >> $(DIR)/libs
	echo -n " " >> $(DIR)/libs
	echo -n -Wl,--format=binary -Wl,candle/$(DIR)/data.zip -Wl,--format=default >> $(DIR)/libs
	echo -n " " >> $(DIR)/libs
	echo -n $(DEPS_REL) >> $(DIR)/libs

$(DIR)/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_REL)

# utils/gl.c:
# 	galogen gl.xml --api gl --ver 3.0 --profile core --exts $(EXTS)
# 	mv gl.c gl.h utils/

release: all
	tar -cf $(DIR)/archive.tar -C $(DIR) $(RES) twinpeaks
	gzip -9f $(DIR)/archive.tar
	cp candle/selfextract.sh $(DIR)/twinpeaks.sh
	cat $(DIR)/archive.tar.gz >> $(DIR)/twinpeaks.sh
	rm $(DIR)/archive.tar.gz
	mkdir -p release
	mv $(DIR)/twinpeaks.sh release/twinpeaks
	chmod +x release/twinpeaks

##############################################################################

debug: $(DIR)/candle_debug.a
	echo -n $(DEPS_DEB) > $(DIR)/libs

$(DIR)/candle_debug.a: init $(OBJS_DEB)
	$(AR) rs $@ $(OBJS_DEB)

$(DIR)/%.debug.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_DEB)


##############################################################################

emscripten: $(DIR)/candle_emscripten.a
	echo -n $(DEPS_EMS) > $(DIR)/libs

$(DIR)/candle_emscripten.a: init $(OBJS_EMS)
	$(AR) rs $@ $(OBJS_EMS)

$(DIR)/%.emscripten.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_EMS)

##############################################################################

init:
	rm -f $(DIR)/libs
	rm -f $(DIR)/cflags
	mkdir -p $(DIR)
	mkdir -p $(DIR)/components
	mkdir -p $(DIR)/systems
	mkdir -p $(DIR)/utils
	mkdir -p $(DIR)/formats
	mkdir -p $(DIR)/vil
	mkdir -p $(DIR)/ecs
	mkdir -p $(DIR)/$(GLFW)
	mkdir -p $(DIR)/$(TINYCTHREAD)

##############################################################################

clean:
	rm -fr $(DIR)
