#ifndef UINPUT_H_
#define UINPUT_H_

int uinput_setup();
void uinput_close(int fd);
void gamepad_emit(int fd, int type, int code, int val);

#endif
