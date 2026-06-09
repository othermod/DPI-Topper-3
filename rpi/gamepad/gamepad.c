#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <linux/uinput.h>
#include <linux/input.h>
#include <ctype.h>

// ---- Constants ----------------------------------------------------------------

#define POLLING_DELAY_US  16000
#define I2C_DEVICE_ADDRESS  0x30
#define DATASIZE               9   // Topper i2cStructure is 9 bytes

// Append one input_event to an array and advance the count.
#define EMIT(ev, cnt, t, c, v) \
    do { (ev)[(cnt)].type = (t); (ev)[(cnt)].code = (c); \
         (ev)[(cnt)].value = (v); (cnt)++; } while (0)

// Set all four calibration fields for one ABS axis in a uinput_user_dev.
#define SET_ABS(dev, ax, mn, mx, fl, fz) \
    do { (dev).absmin[(ax)] = (mn); (dev).absmax[(ax)] = (mx); \
         (dev).absflat[(ax)] = (fl); (dev).absfuzz[(ax)] = (fz); } while (0)

// ---- Globals ------------------------------------------------------------------

static bool    autocenter     = false;
static uint8_t joystick_count = 2;

static int axis_min       = 40;
static int axis_max       = 215;
static int axis_flat      = 20;
#define    AXIS_FUZZ        4
static int axis_center_lx = 127;
static int axis_center_ly = 127;
static int axis_center_rx = 127;
static int axis_center_ry = 127;

static int i2c_fd     = -1;
static int gamepad_fd = -1;

typedef struct {
    uint16_t buttons;
    uint8_t  joyLX, joyLY, joyRX, joyRY;
} ControllerState;

static ControllerState current  = {0};
static ControllerState previous = {0};

// bit_to_ps3[i]: PS3 button index (0-16) for input bit i, or -1 if unmapped.
static int8_t bit_to_ps3[16];

// ---- PS3 layout ---------------------------------------------------------------
//
// Virtual device impersonates a PS3 DualShock (VID 054c, PID 0268) so that
// SDL's gamecontrollerdb GUID 030000004c0500006802000011810000 applies and
// games see a correctly mapped controller out of the box.
//
// Button indices produced by the hid-sony layout (joydev sequential numbering):
//   b0=cross  b1=circle  b2=triangle  b3=square
//   b4=L1  b5=R1  b6=L2  b7=R2
//   b8=back  b9=start  b10=guide
//   b11=L3  b12=R3
//   b13=dpup  b14=dpdown  b15=dpleft  b16=dpright
//
// Axis indices:
//   a0=leftx  a1=lefty  a2=L2-trigger  a3=rightx  a4=righty  a5=R2-trigger
//
// BTN_TL2 and BTN_TR2 are registered even when nothing is mapped to b6/b7 so
// that BTN_SELECT lands at index b8. The trigger axes (ABS_Z, ABS_RZ) are
// always registered; they are driven to 255/0 whenever b6/b7 fire.

static const uint16_t ps3_keycodes[17] = {
    BTN_SOUTH,      // b0  a/cross
    BTN_EAST,       // b1  b/circle
    BTN_NORTH,      // b2  y/triangle
    BTN_WEST,       // b3  x/square
    BTN_TL,         // b4  L1
    BTN_TR,         // b5  R1
    BTN_TL2,        // b6  L2  (also drives ABS_Z)
    BTN_TR2,        // b7  R2  (also drives ABS_RZ)
    BTN_SELECT,     // b8  back/select
    BTN_START,      // b9  start
    BTN_MODE,       // b10 guide/PS button
    BTN_THUMBL,     // b11 L3
    BTN_THUMBR,     // b12 R3
    BTN_DPAD_UP,    // b13 dpup
    BTN_DPAD_DOWN,  // b14 dpdown
    BTN_DPAD_LEFT,  // b15 dpleft
    BTN_DPAD_RIGHT, // b16 dpright
};

static const char *ps3_button_names[17] = {
    "a/cross", "b/circle", "y/triangle", "x/square",
    "L1", "R1", "L2", "R2",
    "back", "start", "guide",
    "L3", "R3",
    "dpup", "dpdown", "dpleft", "dpright",
};

// ---- CRC-16-CCITT -------------------------------------------------------------

static uint16_t crc16_table[256];

static void init_crc16_table(void) {
    const uint16_t poly = 0x1021;
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i << 8;
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? (crc << 1) ^ poly : crc << 1;
        crc16_table[i] = crc;
    }
}

static uint16_t compute_crc16(const uint8_t *data, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++)
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    return crc;
}

// ---- Map string ---------------------------------------------------------------
//
// The --map argument is a 16-character string, one character per input bit
// (bit 0 first). Each character encodes which PS3 button that bit drives:
//
//   '0'-'9'       PS3 b0-b9
//   'A'-'F'/'a'-'f'  PS3 b10-b15
//   'G'/'g'       PS3 b16  (dpright; extends hex naturally past F)
//   '-'           unmapped / ignored
//
// Example: wiring up/down/left/right to bits 0-3
//   --map DEFGffffffffffff--
//   bit0->b13(dpup) bit1->b14(dpdown) bit2->b15(dpleft) bit3->b16(dpright)

static bool parse_map_string(const char *s) {
    size_t len = strlen(s);
    if (len != 16) {
        fprintf(stderr, "Error: --map requires exactly 16 characters (got %zu)\n", len);
        fprintf(stderr, "  Use 0-9, A-G for PS3 button indices b0-b16, '-' to leave unmapped\n");
        return false;
    }
    for (int i = 0; i < 16; i++) {
        char c = (char)toupper((unsigned char)s[i]);
        if (c == '-') {
            bit_to_ps3[i] = -1;
        } else if (c >= '0' && c <= '9') {
            bit_to_ps3[i] = c - '0';
        } else if (c >= 'A' && c <= 'G') {
            bit_to_ps3[i] = c - 'A' + 10;
        } else {
            fprintf(stderr, "Error: invalid character '%c' at position %d in map string\n",
                    s[i], i);
            fprintf(stderr, "  Valid: 0-9, A-G (or a-g) for b0-b16, '-' for unmapped\n");
            return false;
        }
    }
    return true;
}

static void print_mapping(void) {
    printf("Button mapping (bit -> PS3 button):\n");
    for (int i = 0; i < 16; i++) {
        if (bit_to_ps3[i] < 0)
            printf("  bit %2d -> (unmapped)\n", i);
        else
            printf("  bit %2d -> b%-2d (%s)\n",
                   i, bit_to_ps3[i], ps3_button_names[bit_to_ps3[i]]);
    }
}

// ---- Cleanup ------------------------------------------------------------------

static void cleanup(void) {
    if (gamepad_fd >= 0) {
        ioctl(gamepad_fd, UI_DEV_DESTROY);
        close(gamepad_fd);
        gamepad_fd = -1;
    }
    if (i2c_fd >= 0) {
        close(i2c_fd);
        i2c_fd = -1;
    }
}

// ---- I2C ----------------------------------------------------------------------

static void init_i2c(void) {
    i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) {
        perror("Failed to open /dev/i2c-1");
        exit(1);
    }
    if (ioctl(i2c_fd, I2C_SLAVE, I2C_DEVICE_ADDRESS) < 0) {
        perror("Failed to set I2C slave address");
        cleanup();
        exit(1);
    }
    // Probe: confirm something is there before starting the loop.
    uint8_t probe;
    if (read(i2c_fd, &probe, 1) < 1) {
        fprintf(stderr, "No I2C device found at address 0x%02X\n", I2C_DEVICE_ADDRESS);
        cleanup();
        exit(1);
    }
    printf("I2C device found at 0x%02X\n", I2C_DEVICE_ADDRESS);
}

// Topper i2cStructure wire layout (9 bytes, AVR little-endian):
//   [0-1]  buttons  uint16_t  (bit0-7 = PORTB, bit8-15 = PORTD, active-high after ATmega inversion)
//   [2]    joyLX    uint8_t
//   [3]    joyLY    uint8_t
//   [4]    joyRX    uint8_t
//   [5]    joyRY    uint8_t
//   [6]    status   uint8_t   (brightness:3, display_on:1, crc_active:1, ...)
//   [7-8]  crc16    uint16_t  little-endian, over bytes 0-6
//
// We parse manually from a raw byte buffer to avoid any struct-packing
// differences between AVR and the host architecture.

static bool read_i2c_data(void) {
    uint8_t buf[DATASIZE];
    if (read(i2c_fd, buf, DATASIZE) != DATASIZE) return false;

    uint16_t computed = compute_crc16(buf, 7);
    uint16_t received = (uint16_t)buf[7] | ((uint16_t)buf[8] << 8);
    if (computed != received) return false;

    current.buttons = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    current.joyLX   = buf[2];
    current.joyLY   = buf[3];
    current.joyRX   = buf[4];
    current.joyRY   = buf[5];
    return true;
}

// ---- uinput -------------------------------------------------------------------

static int setup_uinput_gamepad(int fd) {
    ioctl(fd, UI_SET_EVBIT, EV_KEY);

    // Register all 17 PS3 buttons. BTN_TL2/BTN_TR2 are always registered
    // (even if nothing maps to b6/b7) so that BTN_SELECT lands at index b8.
    for (int i = 0; i < 17; i++)
        ioctl(fd, UI_SET_KEYBIT, ps3_keycodes[i]);

    // Register all 6 axes. Z and RZ start at 0 and are driven by b6/b7.
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);
    ioctl(fd, UI_SET_ABSBIT, ABS_Z);
    ioctl(fd, UI_SET_ABSBIT, ABS_RX);
    ioctl(fd, UI_SET_ABSBIT, ABS_RY);
    ioctl(fd, UI_SET_ABSBIT, ABS_RZ);

    struct uinput_user_dev uidev = {0};
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "PS3 Controller");
    uidev.id = (struct input_id){ BUS_USB, 0x054c, 0x0268, 0x8111 };

    SET_ABS(uidev, ABS_X,  axis_min, axis_max, axis_flat, AXIS_FUZZ);
    SET_ABS(uidev, ABS_Y,  axis_min, axis_max, axis_flat, AXIS_FUZZ);
    SET_ABS(uidev, ABS_Z,  0, 255, 0, 0);   // trigger: digital 0 or 255
    SET_ABS(uidev, ABS_RX, axis_min, axis_max, axis_flat, AXIS_FUZZ);
    SET_ABS(uidev, ABS_RY, axis_min, axis_max, axis_flat, AXIS_FUZZ);
    SET_ABS(uidev, ABS_RZ, 0, 255, 0, 0);   // trigger: digital 0 or 255

    if (write(fd, &uidev, sizeof(uidev)) < 0) {
        perror("Failed to write uinput_user_dev");
        return -1;
    }
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("Failed to create uinput device");
        return -1;
    }

    // Push initial axis positions so the kernel has sane values from the start.
    struct input_event init_events[7];
    int n = 0;
    EMIT(init_events, n, EV_ABS, ABS_X,  axis_center_lx);
    EMIT(init_events, n, EV_ABS, ABS_Y,  axis_center_ly);
    EMIT(init_events, n, EV_ABS, ABS_Z,  0);
    EMIT(init_events, n, EV_ABS, ABS_RX, axis_center_rx);
    EMIT(init_events, n, EV_ABS, ABS_RY, axis_center_ry);
    EMIT(init_events, n, EV_ABS, ABS_RZ, 0);
    EMIT(init_events, n, EV_SYN, SYN_REPORT, 0);
    write(fd, init_events, sizeof(struct input_event) * n);

    return 0;
}

static void init_gamepad(void) {
    gamepad_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (gamepad_fd < 0) {
        perror("Failed to open /dev/uinput");
        cleanup();
        exit(1);
    }
    if (setup_uinput_gamepad(gamepad_fd) != 0) {
        cleanup();
        exit(1);
    }
    printf("Virtual PS3 controller created "
           "(GUID 030000004c0500006802000011810000)\n");
}

// ---- Event emission -----------------------------------------------------------

static void update_gamepad_events(void) {
    struct input_event events[40];
    int n = 0;

    // Buttons: iterate over changed bits, look up PS3 target, emit.
    // If the bit maps to b6 (L2) or b7 (R2), also drive the trigger axis.
    uint16_t changed = previous.buttons ^ current.buttons;
    for (int i = 0; i < 16; i++) {
        if (!(changed & (1 << i))) continue;
        int8_t btn = bit_to_ps3[i];
        if (btn < 0) continue;

        int pressed = (current.buttons >> i) & 1;
        EMIT(events, n, EV_KEY, ps3_keycodes[btn], pressed);

        if (btn == 6) EMIT(events, n, EV_ABS, ABS_Z,  pressed ? 255 : 0);
        if (btn == 7) EMIT(events, n, EV_ABS, ABS_RZ, pressed ? 255 : 0);
    }

    // Analog sticks: emit only on change.
    if (joystick_count >= 1) {
        if (previous.joyLX != current.joyLX)
            EMIT(events, n, EV_ABS, ABS_X, current.joyLX);
        if (previous.joyLY != current.joyLY)
            EMIT(events, n, EV_ABS, ABS_Y, current.joyLY);
    }
    if (joystick_count >= 2) {
        if (previous.joyRX != current.joyRX)
            EMIT(events, n, EV_ABS, ABS_RX, current.joyRX);
        if (previous.joyRY != current.joyRY)
            EMIT(events, n, EV_ABS, ABS_RY, current.joyRY);
    }

    if (n > 0) {
        EMIT(events, n, EV_SYN, SYN_REPORT, 0);
        write(gamepad_fd, events, sizeof(struct input_event) * n);
    }

    previous = current;
}

// ---- Autocenter ---------------------------------------------------------------

static void sample_axis_centers(void) {
    // Take one I2C read and treat the current stick positions as center.
    // Run before the virtual device is created so stale data doesn't matter.
    read_i2c_data();
    axis_center_lx = current.joyLX;
    axis_center_ly = current.joyLY;
    axis_center_rx = current.joyRX;
    axis_center_ry = current.joyRY;
    printf("Axis centers sampled: lx=%d ly=%d rx=%d ry=%d\n",
           axis_center_lx, axis_center_ly, axis_center_rx, axis_center_ry);
}

// ---- Argument parsing ---------------------------------------------------------

static void parse_args(int argc, char *argv[]) {
    bool map_provided = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            puts(
"Usage: gamepad --map <string> [options]\n"
"\n"
"  --map <string>         16-character button mapping string (required)\n"
"                         Each position corresponds to one input bit (bit 0 first).\n"
"                         The character at that position names the PS3 button it drives:\n"
"\n"
"                           0-9   -> PS3 b0-b9\n"
"                           A-F   -> PS3 b10-b15  (case-insensitive)\n"
"                           G     -> PS3 b16  (dpright)\n"
"                           -     -> unmapped / ignored\n"
"\n"
"                         PS3 button reference:\n"
"                           0=cross  1=circle  2=triangle  3=square\n"
"                           4=L1     5=R1      6=L2        7=R2\n"
"                           8=back   9=start   A=guide\n"
"                           B=L3     C=R3\n"
"                           D=dpup   E=dpdown  F=dpleft    G=dpright\n"
"\n"
"                         Bits mapped to L2 (6) or R2 (7) also drive the\n"
"                         corresponding trigger axis (ABS_Z / ABS_RZ).\n"
"\n"
"                         Example -- up/down/left/right on bits 0-3:\n"
"                           --map DEFG------------\n"
"\n"
"  --joysticks <0-2>      Number of analog sticks to read (default: 2)\n"
"  --min <0-255>          Stick axis minimum value (default: 40)\n"
"  --max <0-255>          Stick axis maximum value (default: 215)\n"
"  --deadzone <0-100>     Stick axis deadzone flat value (default: 20)\n"
"  --autocenter           Sample stick positions at startup as center point\n"
"  --help, -h             Show this help and exit"
            );
            exit(0);

        } else if (strcmp(argv[i], "--map") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --map requires a value\n");
                exit(1);
            }
            if (!parse_map_string(argv[++i])) exit(1);
            map_provided = true;

        } else if (strcmp(argv[i], "--autocenter") == 0) {
            autocenter = true;
            printf("Autocenter enabled\n");

        } else if (strcmp(argv[i], "--joysticks") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --joysticks requires a value\n");
                exit(1);
            }
            int val = atoi(argv[++i]);
            if (val < 0 || val > 2) {
                fprintf(stderr, "Error: --joysticks must be 0, 1, or 2\n");
                exit(1);
            }
            joystick_count = (uint8_t)val;
            printf("Joysticks: %d\n", joystick_count);

        } else if (strcmp(argv[i], "--min") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --min requires a value\n");
                exit(1);
            }
            int val = atoi(argv[++i]);
            if (val < 0 || val > 255) {
                fprintf(stderr, "Error: --min must be 0-255\n");
                exit(1);
            }
            axis_min = val;
            printf("Axis min: %d\n", axis_min);

        } else if (strcmp(argv[i], "--max") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --max requires a value\n");
                exit(1);
            }
            int val = atoi(argv[++i]);
            if (val < 0 || val > 255) {
                fprintf(stderr, "Error: --max must be 0-255\n");
                exit(1);
            }
            axis_max = val;
            printf("Axis max: %d\n", axis_max);

        } else if (strcmp(argv[i], "--deadzone") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --deadzone requires a value\n");
                exit(1);
            }
            int val = atoi(argv[++i]);
            if (val < 0 || val > 100) {
                fprintf(stderr, "Error: --deadzone must be 0-100\n");
                exit(1);
            }
            axis_flat = val;
            printf("Deadzone: %d\n", axis_flat);

        } else {
            fprintf(stderr, "Error: unknown argument '%s'\n", argv[i]);
            fprintf(stderr, "Run with --help for usage\n");
            exit(1);
        }
    }

    if (!map_provided) {
        fprintf(stderr,
            "Error: --map is required.\n"
            "  Provide a 16-character string mapping each input bit to a PS3 button.\n"
            "  Run with --help for full usage and the button index reference.\n"
            "  Use the mapper utility to generate a string interactively.\n");
        exit(1);
    }
}

// ---- Main ---------------------------------------------------------------------

int main(int argc, char *argv[]) {
    parse_args(argc, argv);
    print_mapping();

    init_crc16_table();
    init_i2c();

    if (autocenter)
        sample_axis_centers();

    init_gamepad();

    while (1) {
        if (!read_i2c_data()) continue;
        update_gamepad_events();
        usleep(POLLING_DELAY_US);
    }

    cleanup();
    return 0;
}
