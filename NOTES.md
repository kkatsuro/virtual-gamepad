          ____________________________              __
         / [__ZL__]          [__ZR__] \               |
        / [__ TL __]        [__ TR __] \              | Front Triggers
     __/________________________________\__         __|
    /                                  _   \          |
   /      /\           __             (N)   \         |
  /       ||      __  |MO|  __     _       _ \        | Main Pad
 |    <===DP===> |SE|      |ST|   (W) -|- (E) |       |
  \       ||    ___          ___       _     /        |
  /\      \/   /   \        /   \     (S)   /\      __|
 /  \________ | LS  | ____ |  RS | ________/  \       |
|         /  \ \___/ /    \ \___/ /  \         |      | Control Sticks
|        /    \_____/      \_____/    \        |    __|
|       /                              \       |
 \_____/                                \_____/

     |________|______|    |______|___________|
       D-Pad    Left       Right   Action Pad
               Stick       Stick

                 |_____________|
                    Menu Pad


Most gamepads have the following features:

        Action-Pad 4 buttons in diamonds-shape (on the right side). The buttons are differently labeled on most devices so we define them as NORTH, SOUTH, WEST and EAST.
        D-Pad (Direction-pad) 4 buttons (on the left side) that point up, down, left and right.
        Menu-Pad Different constellations, but most-times 2 buttons: SELECT - START Furthermore, many gamepads have a fancy branded button that is used as special system-button. It often looks different to the other buttons and is used to pop up system-menus or system-settings.
        Analog-Sticks Analog-sticks provide freely moveable sticks to control directions. Not all devices have both or any, but they are present at most times. Analog-sticks may also provide a digital button if you press them.
        Triggers Triggers are located on the upper-side of the pad in vertical direction. Not all devices provide them, but the upper buttons are normally named Left- and Right-Triggers, the lower buttons Z-Left and Z-Right.
        Rumble Many devices provide force-feedback features. But are mostly just simple rumble motors.


D-Pad:

Every gamepad provides a D-Pad with four directions: Up, Down, Left, Right Some of these are available as digital buttons, some as analog buttons. Some may even report both. The kernel does not convert between these so applications should support both and choose what is more appropriate if both are reported.

        Digital buttons are reported as:

        BTN_DPAD_*

        Analog buttons are reported as:

        ABS_HAT0X and ABS_HAT0Y
TODO:
add fake rumble support
#ifdef RUMBLE
            struct input_event ev_read;
            while(read(gamepad_fd, &ev_read, sizeof(ev_read)) == sizeof(ev_read)) {
                switch(ev_read.type) {
                case EV_FF: {
                    printf("readed EV_FF\n");
                } break;
                case EV_UINPUT: {
                    printf("readed EV_UINPUT\n");
                } break;
                default: {
                    printf("readed some different event type\n");
                } break;
                }
            }
#endif

# design
Now as I think of it, im pretty sure I always want to start 'config' window, so that should actually be main window.
running 'canal' itself as a different process is really 

# todo
## mapper
* analog gui:
  * resize window
  * change + to something else
  * leave half transparent trail after movement
  * allow for mapping button to return to zero position or modifier under which you can move at all
  * allow running for 2 different analogs

* config gui:
  * 
