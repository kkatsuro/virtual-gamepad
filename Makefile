PKGS=sdl2
CFLAGS_COMMON=-Wall -Wextra -std=c11 -pedantic
CFLAGS=$(CFLAGS_COMMON) `pkg-config --cflags $(PKGS)`
LIBS=`pkg-config --libs $(PKGS)` -lm

all: canal

canal: src/main.c
	$(CC) $(CFLAGS) -o canal src/main.c $(LIBS)
