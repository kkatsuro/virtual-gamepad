#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include <linux/input.h>
#include <sys/ioctl.h>

#include <SDL2/SDL.h>

#include "./gamepad_uinput.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION
#include "../libs/nuklear.h"
#include "../libs/nuklear_sdl_renderer.h"
#include "sdl_image_as.h"

#define IMAGE_WIDTH  512
#define IMAGE_HEIGHT 316

#define SIDEBAR 250
#define CONFIG_WINDOW_WIDTH  1280
#define CONFIG_WINDOW_HEIGHT 720
#define IMAGE_X (CONFIG_WINDOW_WIDTH  - IMAGE_WIDTH + SIDEBAR)  / 2
#define IMAGE_Y (CONFIG_WINDOW_HEIGHT - IMAGE_HEIGHT) / 2

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"

#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 500

#define DEVICES_CAPACITY 256
#define MAP(x, current_range_max, desired_range_max) (((double) x) / ((double) current_range_max / desired_range_max))

/* 
 * this software is currently in after process of inlining, which is 'opposite process to factoring'
 * be warned that there's a lot of temporary mess here and please don't be terrified by that main() func,
 * John Carmack told me to do that! http://number-none.com/blow/blog/programming/2014/09/26/carmack-on-inlined-code.html
 */

// evdev.c
typedef struct {
    struct dirent entry;
    char name[256];
} Device;

enum {
    EVDEV_MOUSE = 1,
    EVDEV_KEYBOARD = 2
};

typedef struct {
    int fd;
    int type;
} event_device_fd;

typedef struct input_event input_event;

// TODO: fill weird key names with NULL
// https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
const char *keyboard_key_names[] = {
    NULL, NULL, "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    "-", "=", NULL, "Tab", "Q", "W", "E", "R", "T",
    "Y", "U", "I", "O", "P", "[", "]", "Enter", "Left ctrl",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'",
    "`", "Left Shift", "\\", "Z", "X", "C", "V", "B", "N", "M",
    ",", ".", "/", "Right Shift", "KPASTERISK", "Left Alt", "Space", NULL,
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", NULL,
    NULL, "KP7", "KP8", "KP9", "KPMINUS", "KP4", "KP5", "KP6", "KPPLUS",
    "KP1", "KP2", "KP3", "KP0", "KPDOT", "ZENKAKUHANKAKU", "102ND", "F11", "F12",
    "RO", "KATAKANA", "HIRAGANA", "HENKAN", "KATAKANAHIRAGANA", "MUHENKAN", "KPJPCOMMA",
    "KPENTER", "Right Ctrl", "KPSLASH", "SYSRQ", "RIGHTALT", "LINEFEED", "HOME", "UP",
    "PAGEUP", "Left Arrow", "Right Arrow", "END", "DOWN", "PAGEDOWN", "INSERT", "DELETE", "MACRO",
    "MUTE", "VOLUMEDOWN", "VOLUMEUP", "POWER", "KPEQUAL", "KPPLUSMINUS", "PAUSE", "SCALE",
    "KPCOMMA", "HANGEUL", "HANGUEL", "HANJA", "YEN", NULL, NULL, "COMPOSE",
    "STOP", "AGAIN", "PROPS", "UNDO", "FRONT", "COPY", "OPEN", "PASTE", "FIND",
    "CUT", "HELP", "MENU", "CALC", "SETUP", "SLEEP", "WAKEUP", "FILE", "SENDFILE",
    "DELETEFILE", "XFER", "PROG1", "PROG2", "WWW", "MSDOS", "COFFEE", "SCREENLOCK",
    "ROTATE_DISPLAY", "DIRECTION", "CYCLEWINDOWS", "MAIL", "BOOKMARKS", "COMPUTER",
    "BACK", "FORWARD", "CLOSECD", "EJECTCD", "EJECTCLOSECD", "NEXTSONG", "PLAYPAUSE",
    "PREVIOUSSONG", "STOPCD", "RECORD", "REWIND", "PHONE", "ISO", "CONFIG", "HOMEPAGE",
    "REFRESH", "EXIT", "MOVE", "EDIT", "SCROLLUP", "SCROLLDOWN", "KPLEFTPAREN",
    "KPRIGHTPAREN", "NEW", "REDO",

    "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",

    [0x110]="Left Mouse Click", "Right Mouse Click", "Middle Mouse Click", 
    "'Side' Mouse Click", "BTN_EXTRA", "BTN_FORWARD", "BTN_BACK", "BTN_TASK"  // TODO: these are mouse buttons probably too
};


// gamepad_button types
enum {
    BUTTON_UNMAPPED = 0,
    BUTTON_BUTTON   = 1,
    BUTTON_AXIS     = 2,
    BUTTON_ANALOG_MOVEMENT = 3
};

// gamepad_button.which for gamepad_button.type=BUTTON_ANALOG_MOVEMENT
enum {
    LEFT_ANALOG_MOVEMENT  = 1,
    RIGHT_ANALOG_MOVEMENT = 2
};

typedef struct {
  int which; // transmitted button constant
  int type;  // button or analog
  int value; // transmitted value
} gamepad_button;

#define KEYMAP_SIZE (int) (sizeof(keyboard_key_names)/sizeof(keyboard_key_names[0]))
gamepad_button keymap[KEYMAP_SIZE];

#define BUFSIZE 128
typedef struct {
    char button_name[BUFSIZE];          // these 4 members are constant after init
    char setting_button_name[BUFSIZE];  // 
    gamepad_button button;              //
    SDL_Texture *texture;               //
                            
    char display_name[BUFSIZE];         // and these actually do depend from current mapping
    int key;                            //
} button_mapping;

// == evdev.c ==
static int is_event_device(const struct dirent *dir)
{
    return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}

size_t scan_devices(Device *devices, size_t capacity)
{
    struct dirent **namelist;

    int ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, versionsort);

    size_t size = 0;
    for (int i = 0; i < ndev && size < capacity; ++i) {
        char fname[512];
        snprintf(fname, sizeof(fname),
                 "%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);

        int fd = open(fname, O_RDONLY);
        if (fd >= 0) {
            ioctl(fd, EVIOCGNAME(sizeof(devices[size].name)), devices[size].name);
            memcpy(&devices[size].entry, namelist[i], sizeof(devices[size].entry));
            size += 1;
            close(fd);
        }
        free(namelist[i]);
    }
    free(namelist);

    return size;
}

// TODO: maybe modify this
void open_device_if_named(Device device, event_device_fd *evdevices, int *evdev_cursor, char *name, int type) {
    if (strcmp(device.name, name)) {
        return;
    }
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/%s", DEV_INPUT_EVENT, device.entry.d_name);
    int fd = open(filename, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "ERROR: Could not open file %s: %s\n", filename, strerror(errno));
        return;
    }
    fprintf(stderr, "INFO: opened device: %s\n", name);
    evdevices[*evdev_cursor].fd = fd;
    evdevices[*evdev_cursor].type = type;
    (*evdev_cursor)++;
    // TODO: add device filename (??)
}


/* @inpure: modifies state of keymap and mapping_array */
int map_button(int new_key_code, button_mapping* mapping_array, int size, int mapped_button_index, gamepad_button* keymap) {
    assert (keyboard_key_names[new_key_code] != NULL);  // TODO: how this should work?

    button_mapping *mapping = &mapping_array[mapped_button_index];

    int previous_mapping_index = -1;
    { // clear both gamepad button and key mappings
        if (mapping->key != -1)
            keymap[mapping->key].type = BUTTON_UNMAPPED;

        for (int i = 0; i < size; i++) {
            if (new_key_code == mapping_array[i].key) {  // TODO: say that key was already mapped to something
                mapping_array[i].key = -1;
                strncpy(mapping_array[i].display_name, mapping_array[i].button_name, BUFSIZE);
                previous_mapping_index = i;
                break;
            }
        }
    }

    keymap[new_key_code] = mapping->button;
    mapping->key = new_key_code;
    strncpy(mapping->display_name, mapping->button_name, BUFSIZE);
    strncat(mapping->display_name, ": ", BUFSIZE);
    strncat(mapping->display_name, keyboard_key_names[new_key_code], BUFSIZE);

    return previous_mapping_index;
}


void file_skip_character(FILE* file, char skipped) {
    char c;
    while ((c = getc(file)) == skipped)
        if (c == EOF) return;

    ungetc(c, file);
}


int load_config(char *filename, gamepad_button *keymap, button_mapping *mapping_array, int mapping_array_size) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "ERROR: could not open config file %s: %s\n", filename, strerror(errno));
        return -1;
    } 

    memset(keymap, 0, sizeof(gamepad_button) * KEYMAP_SIZE);

    char c;
    char buffer[BUFSIZE];
    char setting[BUFSIZE];
    char value[BUFSIZE];
    int cursor = 0;
    while ((c = getc(file)) != EOF) {
        if (c == ' ') {  // parse setting
            buffer[cursor] = '\0';
            strncpy(setting, buffer, BUFSIZE);
            file_skip_character(file, ' ');
            cursor = 0;
            continue;
        }
        if (c == '\n') {  // parse value
            buffer[cursor] = '\0';
            strncpy(value, buffer, BUFSIZE);
            cursor = 0;  // TODO: is it really fine to do this here?

            // TODO: test error checking
            {  // keymap_add_setting()  
                
                int button_mapping_setting_index = -1;
                {  // find_button_mapping_for_setting()
                    for (int i = 0; i < mapping_array_size; i++) {
                        int compare = strcmp(setting, mapping_array[i].setting_button_name) == 0;
                        if (compare) {
                            button_mapping_setting_index = i;
                            break;
                        }
                    }
                }

                if (button_mapping_setting_index == -1) {
                    fprintf(stderr, "ERROR: invalid button name in config file setting: %s %s\n", setting, value);
                    continue;
                }

                int key = atoi(value);
                if (key == 0) {
                    fprintf(stderr, "ERROR: keycode in config file doesn't seem to be number: %s %s\n", setting, value);
                    continue;
                }

                if (key >= KEYMAP_SIZE) {
                    fprintf(stderr, "ERROR: keycode in config file exceeds max value %d: %s %s\n", KEYMAP_SIZE, setting, value);
                    continue;
                }

                if (keyboard_key_names[key] == NULL) {
                    fprintf(stderr, "ERROR: this keycode isn't allowed because of non-existant reasons: %s %s\n"
                                    "       if you really want to use it, change keyboard_key_names[%s] to name "
                                    "of your button in the source code", setting, value, value);
                    continue;
                }

                map_button(key, mapping_array, mapping_array_size, button_mapping_setting_index, keymap);
            }

            continue;
        }

        buffer[cursor++] = c;
        if (cursor >= BUFSIZE) {
            printf("not long enough, buffer: %s\n", buffer);
            cursor = 0;
            assert(cursor >= BUFSIZE);
        }
    }

    if (fclose(file) == -1) {
        fprintf(stderr, "ERROR: Could not close file %s: %s\n", filename, strerror(errno));
    }

    return 0;
}

int save_config(char *filename, button_mapping *mapping_array, int size) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        fprintf(stderr, "ERROR: Could not open file %s: %s\n", filename, strerror(errno));
        return 0;
    }

    for (int i = 0; i < size; i++) {
        button_mapping mapping = mapping_array[i];
        if (mapping.key != -1)
            fprintf(file, "%s %d\n", mapping.setting_button_name, mapping.key);
    }

    if (fclose(file) == -1) {
        fprintf(stderr, "ERROR: Could not close file %s: %s\n", filename, strerror(errno));
    }

    return 1;
}

int find_last_index_with_char(char *string, int length, char c) {
   int last_index = -1;
   for (int i = 0; string[i] && i < length; i++) {
      if (string[i] == c) {
         last_index = i;
      }
   }
   return last_index;
}

// TODO: how reliable is that?
void enter_executable_path() {
   char path[4096];
   ssize_t length = readlink("/proc/self/exe", path, 4096);
   path[length] = '\0';
   int last_slash = find_last_index_with_char(path, 4096, '/');
   path[last_slash+1] = '\0';
   chdir(path);
}

int main() {
    enter_executable_path();

    struct input_event events[64];

    SDL_Window *config_window;
    SDL_Renderer *config_renderer;

    struct nk_context *ctx;
    int input_is_our_event;

    int gui_config_window_opened = 1;
    Device devices[DEVICES_CAPACITY];
    gamepad_button keymap[KEYMAP_SIZE];

    event_device_fd evdevices[DEVICES_CAPACITY];
    int evdev_num = 0;
    {  // load_evdev_devices(),  modified tsoding voidf code for opening evdev device, https://github.com/tsoding/voidf/
        const size_t devices_size = scan_devices(devices, DEVICES_CAPACITY);

        printf("Found %lu devices\n", devices_size);
        if (devices_size == 0) {
            printf("Most likely not enough permissions to read files from %s\n", DEV_INPUT_EVENT);
            exit(1);
        }

        // NOTE: names can repeat here and one of my keyboards doesnt work, thats why we need to save them all -
        // - why would you care about identifiers??
        // TODO:
        // * do this for every new connected device, and manage disconnects
        // * inform user and probably open configuration window if no keyboard and/or mouse
        for (size_t i = 0; i < devices_size; ++i) {
            open_device_if_named(devices[i], evdevices, &evdev_num, "Logitech G305", EVDEV_MOUSE);
            open_device_if_named(devices[i], evdevices, &evdev_num, "Turing Gaming Keyboard Turing Gaming Keyboard", EVDEV_KEYBOARD);
        }
    }

    {  // set_guids_and_stuff()
        uid_t ruid = getuid();
        gid_t rgid = getgid();

        if (seteuid(ruid) < 0) {
            fprintf(stderr, "WARNING: Could not set Effective UID to the real one: %s\n", strerror(errno));
        }

        if (setegid(rgid) < 0) {
            fprintf(stderr, "WARNING: Could not set Effective GID to the real one: %s\n", strerror(errno));
        }
    }

    int gamepad_fd = uinput_setup();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "ERROR: Could not initialize SDL: %s\n", SDL_GetError());
        return 0;
    }
    
    int config_window_id = -1;
    if (gui_config_window_opened) {  // create_config_renderer();

        config_window = SDL_CreateWindow("nukl-test.c",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          CONFIG_WINDOW_WIDTH, CONFIG_WINDOW_HEIGHT,
                                          0);
        config_renderer = SDL_CreateRenderer(config_window, -1, SDL_RENDERER_PRESENTVSYNC);

        if (!config_renderer) {
            fprintf(stderr, "ERROR: Could not initialize renderer: %s\n", SDL_GetError());
            return 1;
        }

        float font_scale = 1;
        ctx = nk_sdl_init(config_window, config_renderer);
        {
            struct nk_font_atlas *atlas;
            struct nk_font_config config = nk_font_config(0);
            struct nk_font *font;

            nk_sdl_font_stash_begin(&atlas);
            font = nk_font_atlas_add_default(atlas, 13 * font_scale, &config);
            nk_sdl_font_stash_end();

            font->handle.height /= font_scale;
            nk_style_set_font(ctx, &font->handle);
        }
        config_window_id = SDL_GetWindowID(config_window);

        gui_config_window_opened = 1;
    }

    SDL_Rect gamepad_image_rect = {IMAGE_X, IMAGE_Y, IMAGE_WIDTH, IMAGE_HEIGHT};
    SDL_Texture *gamepad_image = file_as_texture(config_renderer, "pngs/gamepad.png");

    // very important TODO: don't load textures like that anymore, that's way too much waiting for a compiler..
    button_mapping mapping_array[] = {
        { "LStick Active",  "left_stick_active",  { LEFT_ANALOG_MOVEMENT,  BUTTON_ANALOG_MOVEMENT, 1 }, NULL, "", -1 },
        { "RStick Active", "right_stick_active", { RIGHT_ANALOG_MOVEMENT, BUTTON_ANALOG_MOVEMENT, 1 }, NULL, "", -1 },

        { "A button", "a_button", { BTN_SOUTH, BUTTON_BUTTON, 1 }, file_as_texture(config_renderer, "pngs/s.png"),  "", -1 },
        { "B button", "b_button", { BTN_EAST,  BUTTON_BUTTON, 1 }, file_as_texture(config_renderer, "pngs/e.png"),  "", -1 },
        { "Y button", "y_button", { BTN_NORTH, BUTTON_BUTTON, 1 }, file_as_texture(config_renderer, "pngs/w.png"),  "", -1 },
        { "X button", "x_button", { BTN_WEST,  BUTTON_BUTTON, 1 }, file_as_texture(config_renderer, "pngs/n.png"),  "", -1 },
     
        { "Dpad Down",  "dpad_down",  { ABS_HAT0X, BUTTON_AXIS, -1 }, file_as_texture(config_renderer, "pngs/dpad-down.png"),   "", -1 },
        { "Dpad Left",  "dpad_left",  { ABS_HAT0Y, BUTTON_AXIS, -1 }, file_as_texture(config_renderer, "pngs/dpad-left.png"),   "", -1 },
        { "Dpad Right", "dpad_right", { ABS_HAT0X, BUTTON_AXIS,  1 }, file_as_texture(config_renderer, "pngs/dpad-right.png"),  "", -1 },
        { "Dpad Up",    "dpad_up",    { ABS_HAT0Y, BUTTON_AXIS,  1 }, file_as_texture(config_renderer, "pngs/dpad-up.png"),     "", -1 },
     
        { "Left Shoulder",  "left_shoulder",  { BTN_TL, BUTTON_BUTTON, 1 }, file_as_texture(config_renderer, "pngs/lshoulder.png"),  "", -1 },
        { "Right Shoulder", "right_Shoulder", { BTN_TR, BUTTON_BUTTON, 1 }, file_as_texture(config_renderer, "pngs/rshoulder.png"),  "", -1 },
     
        { "Left Stick Press",  "left_stick_press",  { BTN_THUMBL, BUTTON_BUTTON, 1 }, file_as_texture(config_renderer, "pngs/lstick.png"),  "", -1 },
        { "Right Stick Press", "right_stick_press", { BTN_THUMBR, BUTTON_BUTTON, 1 }, file_as_texture(config_renderer, "pngs/rstick.png"),  "", -1 },

        { "Left Trigger",  "left_trigger",  { ABS_Z,  BUTTON_AXIS, 1024 }, file_as_texture(config_renderer, "pngs/ltrigger.png"),  "", -1 },
        { "Right Trigger", "right_trigger", { ABS_RZ, BUTTON_AXIS, 1024 }, file_as_texture(config_renderer, "pngs/rtrigger.png"),  "", -1 },
     
        { "Left Stick Down",  "left_stick_down",   { ABS_Y,  BUTTON_AXIS, -32767 }, file_as_texture(config_renderer, "pngs/lstick-down.png"),   "", -1 },
        { "Left Stick Left",  "left_stick_left",   { ABS_X,  BUTTON_AXIS, -32767 }, file_as_texture(config_renderer, "pngs/lstick-left.png"),   "", -1 },
        { "Left Stick Right", "left_stick_right",  { ABS_X,  BUTTON_AXIS,  32768 }, file_as_texture(config_renderer, "pngs/lstick-right.png"),  "", -1 },
        { "Left Stick Up",    "left_stick_up",     { ABS_Y,  BUTTON_AXIS,  32768 }, file_as_texture(config_renderer, "pngs/lstick-up.png"),     "", -1 },
     
        { "Right Stick Down",  "right_stick_down",  { ABS_RY, BUTTON_AXIS, -32767 }, file_as_texture(config_renderer, "pngs/rstick-down.png"),  "", -1 },
        { "Right Stick Left",  "right_stick_left",  { ABS_RX, BUTTON_AXIS, -32767 }, file_as_texture(config_renderer, "pngs/rstick-left.png"),  "", -1 },
        { "Right Stick Right", "right_stick_right", { ABS_RX, BUTTON_AXIS,  32768 }, file_as_texture(config_renderer, "pngs/rstick-right.png"), "", -1 },
        { "Right Stick Up",    "right_stick_up",    { ABS_RY, BUTTON_AXIS,  32768 }, file_as_texture(config_renderer, "pngs/rstick-up.png"),    "", -1 },
     
        { "Select", "select", { BTN_SELECT, BUTTON_BUTTON, 1 }, file_as_texture(config_renderer, "pngs/select_btn.png"),  "", -1 },
        { "Start",  "start",  { BTN_START,  BUTTON_BUTTON, 1 }, file_as_texture(config_renderer, "pngs/start.png"),       "", -1 },
    };

    int mapping_array_size = sizeof(mapping_array) / sizeof(mapping_array[0]);

    for (int i = 0; i < mapping_array_size; i++) {
        strncpy(mapping_array[i].display_name, mapping_array[i].button_name, BUFSIZE);
    }

    load_config("mapping.conf", keymap, mapping_array, mapping_array_size);
    

    // NOTE: button mapping happens all over the place with these 2 variables
    int gui_config_mapping_opened = 0;
    int mapped_index;

    # define CHECKBOX_FALSE 1
    # define CHECKBOX_TRUE 0
    int draw_left_analog = CHECKBOX_FALSE;
    int draw_right_analog = CHECKBOX_TRUE;

    int left_analog_active = 0;
    int right_analog_active = 0;

    SDL_Renderer *left_analog_renderer = NULL;
    SDL_Renderer *right_analog_renderer = NULL;

    int running = 1;
    int x = 250;
    int y = 250;
    int prev_x = 250;
    int prev_y = 250;

    {  // NOTE: continue here
        typedef struct {
            int x;
            int y;
        } point;

        typedef struct {
            SDL_Renderer *renderer;
            int is_active;
            int is_window_opened;
            point pos;
            point prev_pos;
        } analog_draw_info;

        analog_draw_info lanalog_draw_info = {
            NULL,
            0,
            CHECKBOX_FALSE,
            { 250, 250 },
            { 250, 250 },
        };
    }

    if (draw_left_analog == CHECKBOX_TRUE) {  // create_analog_window()
        char* title = "analog left stick";
        // TODO: start it in better location
        SDL_Window *Window = SDL_CreateWindow(title, 300, 500, 500, 500, SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS);
        left_analog_renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_PRESENTVSYNC);
        if (!left_analog_renderer) {
            printf("ERROR: couldn't initalize renderer\n");  // TODO: fprintf to stderr
            exit(1);
        }
    }

    if (draw_right_analog == CHECKBOX_TRUE) {  // create_analog_window()
        char* title = "analog right stick";
        // TODO: start it in better location
        SDL_Window *Window = SDL_CreateWindow(title, 300, 500, 500, 500, SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS);
        right_analog_renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_PRESENTVSYNC);
        if (!right_analog_renderer) {
            printf("ERROR: couldn't initalize renderer\n");
            exit(1);
        }
    }

    while (running) {

        {  // config window start input
            input_is_our_event = SDL_GetMouseFocus() == config_window;
            if (gui_config_window_opened && input_is_our_event)
                nk_input_begin(ctx);
        }

        SDL_Event Event;
        while (SDL_PollEvent(&Event)) {
            switch (Event.type) {

            case SDL_QUIT: {
                running = 0; printf("SDL_QUIT\n");
            } break;

            case SDL_WINDOWEVENT:
            {
                if (Event.window.event == SDL_WINDOWEVENT_CLOSE && 1) { // TODO: if Event.window.windowID == main window id
                    running = 0; printf("SDL_QUIT\n");
                }
            } break; 

            }

            {  // config_window_handle_sdl_event()
                if (gui_config_window_opened && input_is_our_event) { 

                    if (Event.type == SDL_WINDOWEVENT && Event.window.event == SDL_WINDOWEVENT_CLOSE 
                                                      && Event.window.windowID == config_window_id) {
                        gui_config_window_opened = 0;
                        SDL_DestroyWindow(config_window);
                        nk_sdl_shutdown();
                        running = 0;
                    }

                    nk_sdl_handle_event(&Event);

                }
            }
        }

        {  // config window end input
            if (gui_config_window_opened && input_is_our_event)
                nk_input_end(ctx);
        }

        int gamepad_input_happened = 0;
        for (int dev = 0; dev < evdev_num; dev++) {
            int rd = read(evdevices[dev].fd, events, sizeof(events));

            if (rd > 0) {  // handle_events()
                size_t n = rd / sizeof(struct input_event);

                for (size_t i = 0; i < n; ++i) {
                    input_event ev = events[i];

                    // NOTE: this works with keyboard only for now,
                    // when you do mouse mapping doesnt work anymore -
                    // - left click you use to choose button for remapping goes here and window closes immediately
                    if (ev.type == EV_KEY && evdevices[dev].type == EVDEV_KEYBOARD) {

                        int key = ev.code;
                        // printf("%d %d\n", key, KEYMAP_SIZE);  // XXX: this once printed either 0x110274 or 0x110274274. WHY?

                        if (key < KEYMAP_SIZE && keymap[key].type != BUTTON_UNMAPPED) {   // handle_event_uinput()
                            gamepad_button button = keymap[key];

                            if (button.type == BUTTON_BUTTON) {  // TODO: should we leave this emitting 2 for repeat?
                                gamepad_emit(gamepad_fd, EV_KEY, button.which, button.value * ev.value);
                                gamepad_input_happened = 1;
                            }

                            else if (button.type == BUTTON_AXIS) {
                                if (ev.value != 2) {  // TODO: save it somewhere so we can zero-out axes if both ends were pressed
                                    gamepad_emit(gamepad_fd, EV_ABS, button.which, button.value * ev.value); 
                                    gamepad_input_happened = 1;
                                }
                            }

                            else if (button.type == BUTTON_ANALOG_MOVEMENT) {
                                if (button.which == RIGHT_ANALOG_MOVEMENT) {
                                    right_analog_active = ev.value;
                                }
                                else if (button.which == LEFT_ANALOG_MOVEMENT) {
                                    left_analog_active = ev.value;
                                }
                                else {  // TODO: can this even happen?
                                    assert ( button.type == BUTTON_ANALOG_MOVEMENT &&
                                           (button.which == RIGHT_ANALOG_MOVEMENT  || button.which == LEFT_ANALOG_MOVEMENT) );
                                }
                                
                            }

                        }

                        if (gui_config_mapping_opened) {  // handle_event_mapping
                            int key = ev.code;
                            gui_config_mapping_opened = 0;
                            if (key < KEYMAP_SIZE && keyboard_key_names[key] != NULL) {
                            
                                // TODO: inform about previously_mapped
                                int previously_mapped = map_button(key, mapping_array, mapping_array_size, mapped_index, keymap);
                                save_config("mapping.conf", mapping_array, mapping_array_size);

                            } else {
                                if (key != KEY_ESC) {  // TODO: make backspace remove mapping
                                    gui_config_mapping_opened = 1;  // dont close if mapped button unsupported
                                }
                            }

                        }
                    }

                    else if (evdevices[dev].type == EVDEV_MOUSE && right_analog_active && ev.type == EV_REL) {

                        if (ev.code == REL_X) {
                            x += ev.value;
                            if (x < 0) x = 0; else if (x > WINDOW_WIDTH-1) x = WINDOW_WIDTH-1;
                        }
                        if (ev.code == REL_Y) {
                            y += ev.value;
                            if (y < 0) y = 0; else if (y > WINDOW_HEIGHT-1) y = WINDOW_HEIGHT-1;
                        }

                    }

                }   
            }   
        }

        if (!right_analog_active) {
            x = 250;
            y = 250;
        }

        if (prev_x != x) {
            int value = (int) MAP(x-250, 250, 32767);
            gamepad_emit(gamepad_fd, EV_ABS, ABS_RX, value);
            gamepad_input_happened = 1;
        }

        if (prev_y != y) {
            int value = (int) MAP(y-250, 250, 32767);
            gamepad_emit(gamepad_fd, EV_ABS, ABS_RY, value);
            gamepad_input_happened = 1;
        }

        prev_x = x;
        prev_y = y;

        if (gamepad_input_happened) {
            gamepad_emit(gamepad_fd, EV_SYN, SYN_REPORT, 0);
        }

        if (draw_right_analog == CHECKBOX_TRUE) {
            SDL_SetRenderDrawColor(right_analog_renderer, 0x18, 0x18, 0x18, 0xFF);
            SDL_RenderClear(right_analog_renderer);
            SDL_SetRenderDrawColor(right_analog_renderer, 0xAA, 0xAA, 0xAA, 0xFF);

            {  // sdl_renderer_draw_cross();
                int size = 5;
                for (int draw_x = x - size; draw_x <= x + size; draw_x++)
                    SDL_RenderDrawPoint(right_analog_renderer, draw_x, y);
                for (int draw_y = y - size; draw_y <= y + size; draw_y++)
                    SDL_RenderDrawPoint(right_analog_renderer, x, draw_y);
            }

            SDL_RenderPresent(right_analog_renderer);
        } else if (right_analog_renderer != NULL) {
            // TODO: close it
        }

        {  // render_config_window()
            if (!gui_config_window_opened)
                continue;

            SDL_Texture *hovered_button_texture = NULL;

            {  // display_buttons()
                int main_window_flags =  NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCROLL_AUTO_HIDE;  //  | NK_WINDOW_MOVABLE
                if (gui_config_mapping_opened)
                    main_window_flags |= NK_WINDOW_NO_INPUT;

                if (nk_begin(ctx, "buttons", nk_rect(10, 10, SIDEBAR, CONFIG_WINDOW_HEIGHT-20), main_window_flags)) {
                    /* nk_layout_row_begin	Starts a new row with given height and number of columns */
                    /* nk_layout_row_push 	Pushes another column with given size or window ratio */
                    /* nk_layout_row_end   Finished previously started row */ 

                    nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
                    {
                        nk_layout_row_push(ctx, 20);
                        nk_checkbox_label(ctx, "", &draw_left_analog);
                        nk_layout_row_push(ctx, SIDEBAR-20);
                        if (nk_button_label(ctx, mapping_array[0].display_name)) {
                            gui_config_mapping_opened = 1;
                            mapped_index = 0;
                        }
                    }
                    nk_layout_row_end(ctx);

                    nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
                    {
                        nk_layout_row_push(ctx, 20);
                        nk_checkbox_label(ctx, "", &draw_right_analog);
                        nk_layout_row_push(ctx, SIDEBAR-20);
                        if (nk_button_label(ctx, mapping_array[1].display_name)) {
                            gui_config_mapping_opened = 1;
                            mapped_index = 1;
                        }
                    }
                    nk_layout_row_end(ctx);

                    nk_layout_row_dynamic(ctx, 30, 1);
                    for (int i = 2; i < mapping_array_size; i++) {

                        if (nk_button_label(ctx, mapping_array[i].display_name)) {
                            gui_config_mapping_opened = 1;
                            mapped_index = i;
                        }

                        if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER)
                            hovered_button_texture = mapping_array[i].texture;

                    }
                }
                nk_end(ctx);
            }

            {  // display_mapping_window()
                if (gui_config_mapping_opened && nk_begin(ctx, "map button", nk_rect(150, 200, 250, 100), NK_WINDOW_BORDER)) {
                    nk_layout_row_dynamic(ctx, 70, 1);

                    char name[BUFSIZE+32];
                    snprintf(name, BUFSIZE+32, "press key for: %s", mapping_array[mapped_index].button_name);
                    nk_label(ctx, name, NK_TEXT_CENTERED);
                    nk_end(ctx);
                }
            }

            SDL_SetRenderDrawColor(config_renderer, 0x18, 0x18, 0x18, 0xFF);
            SDL_RenderClear(config_renderer);

            SDL_RenderCopy(config_renderer, gamepad_image, NULL, &gamepad_image_rect); 
            if (hovered_button_texture != NULL)
                SDL_RenderCopy(config_renderer, hovered_button_texture, NULL, &gamepad_image_rect); 

            nk_sdl_render(NK_ANTI_ALIASING_ON);

            SDL_RenderPresent(config_renderer);
        }
    }

    uinput_close(gamepad_fd);

    if (gui_config_window_opened)
        nk_sdl_shutdown();

    SDL_Quit();
}
