CC = gcc
LD = gcc

DIR = build

SRCS = $(wildcard *.c) $(wildcard components/*.c) $(wildcard systems/*.c) \
	   $(wildcard formats/*.c)

OBJS_REL = $(patsubst %.c, $(DIR)/%.o, $(SRCS))
OBJS_DEB = $(patsubst %.c, $(DIR)/%.debug.o, $(SRCS))

CFLAGS = $(shell sdl2-config --cflags) -DUSE_VAO -Wall -I.

CFLAGS_REL = $(CFLAGS) -O2

CFLAGS_DEB = $(CFLAGS) -g3

##############################################################################

all: $(DIR)/candle.a

$(DIR)/candle.a: init $(OBJS_REL) 
	$(AR) rs $@ $(OBJS_REL)

$(DIR)/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_REL)

##############################################################################

debug: $(DIR)/candle_debug.a

$(DIR)/candle_debug.a: init $(OBJS_DEB) 
	$(AR) rs $@ $(OBJS_DEB)

$(DIR)/%.debug.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS_DEB)

##############################################################################

init:
	mkdir -p $(DIR)
	mkdir -p $(DIR)/components
	mkdir -p $(DIR)/systems
	mkdir -p $(DIR)/formats

##############################################################################

clean:
	rm -r $(DIR)
