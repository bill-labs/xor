CC=gcc
PREFIX ?= /opt/gnux
DESTDIR ?=
BINDIR ?= $(PREFIX)/bin

UI_OBJECTS = sdl-ui.o
UI_LIBS = -lSDL

CFLAGS = -I$(PREFIX)/include -I. -Izcore -O3 -fomit-frame-pointer -Wall -Werror
LDFLAGS = -L$(PREFIX)/lib
LIBS = $(UI_LIBS)

TARGET = xor
OBJECTS = game.o speccy.o rom48.o zcore.o $(UI_OBJECTS)

all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LDFLAGS) $(LIBS) -lbz2 -lgnux

sdl-ui.o:	sdl-ui.c
	$(CC) $(CFLAGS) -c $<

speccy.o:	speccy.c
	$(CC) $(CFLAGS) -c $<
rom48.o:	rom48.c
	$(CC) $(CFLAGS) -c $<
game.o:		game.c
	$(CC) $(CFLAGS) -c $<
zcore.o:	x86/zcore.S
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(TARGET) *.core core

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(TARGET) $(DESTDIR)$(BINDIR)/

.PHONY: all clean install
