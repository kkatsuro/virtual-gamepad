PKGS=sdl2
CFLAGS_COMMON=-Wall -Wextra -std=c11 -pedantic
CFLAGS=$(CFLAGS_COMMON) `pkg-config --cflags $(PKGS)`
LIBS=`pkg-config --libs $(PKGS)` -lm

all: bin/canal

bin/canal: src/gamepad-image.h src/main.c
	$(CC) $(CFLAGS) -o bin/canal src/main.c $(LIBS)

src/gamepad-image.h: bin/image2c
	bin/image2c pngs/* > src/gamepad-image.h

bin/image2c: src/image2c.c src/stb_image.h
	$(CC) $(CFLAGS_COMMON) -o bin/image2c src/image2c.c -lm
