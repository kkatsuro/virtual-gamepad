#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>

#include "./gamepad_uinput.h"

// TODO: setup this:
  /* Event type 3 (EV_ABS) */
  /*   Event code 0 (ABS_X) */
  /*   Event code 1 (ABS_Y) */
  /*   Event code 3 (ABS_RX) */
  /*   Event code 4 (ABS_RY) */
  /*     Min   -32768 */
  /*     Max    32767 */
  /*     Fuzz      16 */
  /*     Flat     128 */

  /*   Event code 2 (ABS_Z) */
  /*   Event code 5 (ABS_RZ) */
  /*     Min        0 */
  /*     Max     1023 */

  /*   Event code 16 (ABS_HAT0X) */
  /*   Event code 17 (ABS_HAT0Y) */
  /*     Min       -1 */
  /*     Max        1 */
int uinput_setup() {
    int gamepad_fd = open("/dev/uinput", O_RDWR | O_NONBLOCK);

    if (!gamepad_fd) {
        fprintf(stderr, "ERROR: Could not open uinput device /dev/uinput: %s\n", strerror(errno));
        fprintf(stderr, "       You need to install uinput module if you have not already\n");
        exit(1);
    }

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    strncpy(uidev.name, "Microsoft Xbox Series S|X Controller", UINPUT_MAX_NAME_SIZE-1); // '-1 to guarantee 0 termination' ?
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x045e;
    uidev.id.product = 0x0b12;
    uidev.id.version = 0x0507;

    ioctl(gamepad_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(gamepad_fd, UI_SET_KEYBIT, KEY_RECORD);
    ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_SOUTH );
    ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_EAST  );
    ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_NORTH );
    ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_WEST  );
    ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_TL    );
    ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_TR    );
    ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_SELECT);
    ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_START );
    ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_MODE  );
    ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_THUMBL);
    ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_THUMBR);

    ioctl(gamepad_fd, UI_SET_EVBIT, EV_ABS);

    static int abs[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY };
    for (int i = 0; i < 4; i++) {
        ioctl(gamepad_fd, UI_SET_ABSBIT, abs[i]);
        uidev.absmin[abs[i]] = -32768;
        uidev.absmax[abs[i]] = 32767;
        uidev.absfuzz[abs[i]] = 16;
        uidev.absflat[abs[i]] = 128;
    }

    ioctl(gamepad_fd, UI_SET_ABSBIT, ABS_Z);
    ioctl(gamepad_fd, UI_SET_ABSBIT, ABS_RZ);
    ioctl(gamepad_fd, UI_SET_ABSBIT, ABS_HAT0X);
    ioctl(gamepad_fd, UI_SET_ABSBIT, ABS_HAT0Y);

    #ifdef RUMBLE
    ioctl(gamepad_fd, UI_SET_EVBIT, EV_FF);
    ioctl(gamepad_fd, UI_SET_FFBIT, FF_PERIODIC);
    ioctl(gamepad_fd, UI_SET_FFBIT, FF_RUMBLE);
    ioctl(gamepad_fd, UI_SET_FFBIT, FF_GAIN);
    ioctl(gamepad_fd, UI_SET_FFBIT, FF_SQUARE);
    ioctl(gamepad_fd, UI_SET_FFBIT, FF_TRIANGLE);
    ioctl(gamepad_fd, UI_SET_FFBIT, FF_SINE);
    uidev.ff_effects_max = 1;
    #endif

    ssize_t res = write(gamepad_fd, &uidev, sizeof(uidev));
    if (res < 0) {
        fprintf(stderr, "ERROR: Could not do write setup to uinput device: %s\n", strerror(errno));
        exit(1);
    }

    if (ioctl(gamepad_fd, UI_DEV_CREATE) < 0) {
        fprintf(stderr, "ERROR: Could not create uinput device: %s\n", strerror(errno));
        exit(1);
    }

    return gamepad_fd;
}

void gamepad_emit(int fd, int type, int code, int val) {
   struct input_event ie;

   ie.type = type;
   ie.code = code;
   ie.value = val;
   /* timestamp values below are ignored */
   ie.time.tv_sec = 0;
   ie.time.tv_usec = 0;

   write(fd, &ie, sizeof(ie));
}

void uinput_close(int fd) {
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
}
