CC = cc
LD = cc
AR = ar

emscripten: CC = emcc
emscripten: LD = emcc
emscripten: AR = emar

DIR = build

EMS_OPTS = -s USE_GLFW=3 -s USE_WEBGL2=1 \
-s FULL_ES3=1 -s EMULATE_FUNCTION_POINTER_CASTS=1 \
-s WASM_MEM_MAX=2GB -O3 -s WASM=1 -s ASSERTIONS=1 \
-s ALLOW_MEMORY_GROWTH=1 -s SAFE_HEAP=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0

LIBS = -lm -lGL -pthread -ldl -lX11 -lrt
LDFLAGS_REL = $(LIBS) candle/$(DIR)/candle.a \
-Wl,--format=binary -Wl,candle/$(DIR)/data.zip -Wl,--format=default
LDFLAGS_DEB = $(LIBS) candle/$(DIR)/candle_debug.a \
-Wl,--format=binary -Wl,candle/$(DIR)/data.zip -Wl,--format=default
LDFLAGS_EMS = -lm -lGL -ldl -lrt candle/$(DIR)/candle_emscripten.a \
			  $(EMS_OPTS) --preload-file $(SAUCES) --shell-file index.html

GLFW = third_party/glfw/src
TINYCTHREAD = third_party/tinycthread/source

PLUGINS = $(wildcard ../*.candle)
PLUGINS_REL = $(patsubst %, ../%/$(DIR)/export.a, $(PLUGINS))
PLUGINS_DEB = $(patsubst %, ../%/$(DIR)/export_debug.a, $(PLUGINS))
PLUGINS_EMS = $(patsubst %, ../%/$(DIR)/export_emscripten.a, $(PLUGINS))

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
	   $(wildcard ecs/*.c) third_party/miniz.c

OBJS_REL = $(patsubst %.c, $(DIR)/%.o, $(SRCS) $(THIRD_PARTY_SRC))
OBJS_DEB = $(patsubst %.c, $(DIR)/%.debug.o, $(SRCS) $(THIRD_PARTY_SRC))
OBJS_EMS = $(patsubst %.c, $(DIR)/%.emscripten.o, $(SRCS))

CFLAGS = -std=c89 -pedantic -Wall -I. -Wno-unused-function \
         -Ithird_party/glfw/include -I$(TINYCTHREAD) -D_GLFW_X11 \
         -Wno-format-truncation -Wno-stringop-truncation \
         -DTHREADED -DUSE_VAO

CFLAGS_REL = $(CFLAGS) -O3

CFLAGS_DEB = $(CFLAGS) -g3 -DDEBUG

CFLAGS_EMS = -Wall -I. -Wno-unused-function \
			 -Ithird_party/glfw/include -D_GLFW_X11 $(EMS_OPTS)

##############################################################################

all: $(DIR)/candle.a

$(DIR)/candle.a: init $(OBJS_REL) $(DIR)/data.zip
	echo -n $(LDFLAGS_REL) >> $(DIR)/libs
	$(AR) rs $@ $(OBJS_REL)

$(DIR)/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_REL)

../%/$(DIR)/export.a:
	$(MAKE) -C $(patsubst %/$(DIR)/export.a, %, $@)
	echo -n " " >> $(DIR)/libs
	echo -n $(patsubst ../%, %, $@) >> $(DIR)/libs
	echo -n " " >> $(DIR)/libs
	-cat $(patsubst %/export.a, %/libs, $@) >> $(DIR)/libs

# utils/gl.c:
# 	galogen gl.xml --api gl --ver 3.0 --profile core --exts $(EXTS)
# 	mv gl.c gl.h utils/

##############################################################################

debug: $(DIR)/candle_debug.a

$(DIR)/candle_debug.a: init $(OBJS_DEB) $(DIR)/data.zip
	echo -n $(LDFLAGS_DEB) >> $(DIR)/libs_debug
	$(AR) rs $@ $(OBJS_DEB)

$(DIR)/%.debug.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_DEB)

../%/$(DIR)/export_debug.a:
	$(MAKE) -C $(patsubst %/$(DIR)/export_debug.a, %, $@)
	echo -n " " >> $(DIR)/libs_debug
	echo -n $(patsubst ../%, %, $@) >> $(DIR)/libs_debug
	echo -n " " >> $(DIR)/libs_debug
	-cat $(patsubst %/export_debug.a, %/libs_debug, $@) >> $(DIR)/libs_debug

##############################################################################

emscripten: $(DIR)/candle_emscripten.a

$(DIR)/candle_emscripten.a: init $(OBJS_EMS)
	echo -n $(LDFLAGS_EMS) >> $(DIR)/libs_emscripten
	$(AR) rs $@ $(OBJS_EMS)

$(DIR)/%.emscripten.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_EMS)

../%/$(DIR)/export_emscripten.a:
	$(MAKE) -C $(patsubst %/$(DIR)/export_emscripten.a, %, $@)
	echo -n " " >> $(DIR)/libs_emscripten
	echo -n $(patsubst ../%, %, $@) >> $(DIR)/libs_emscripten
	echo -n " " >> $(DIR)/libs_emscripten
	-cat $(patsubst %/export_emscripten.a, %/libs_emscripten, $@) >> $(DIR)/libs_emscripten

##############################################################################

$(DIR)/data.zip: $(DIR)/packager
	$(DIR)/packager ../$(SAUCES)

$(DIR)/packager: 
	$(CC) packager/packager.c third_party/miniz.c -O3 -o $(DIR)/packager

##############################################################################

init:
	rm -f $(DIR)/libs
	rm -f $(DIR)/libs_debug
	rm -f $(DIR)/libs_emscripten
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
