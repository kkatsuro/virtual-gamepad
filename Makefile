PKGS=sdl2
CFLAGS_COMMON=-Wall -Wextra -std=c11 -pedantic
CFLAGS=$(CFLAGS_COMMON) `pkg-config --cflags $(PKGS)`
LIBS=`pkg-config --libs $(PKGS)` -lm
SRCS=src/main.c src/gamepad_uinput.c

all: canal
canal: $(SRCS) libs/nuklear.h
	$(CC) $(CFLAGS) -o canal $(SRCS) $(LIBS)
