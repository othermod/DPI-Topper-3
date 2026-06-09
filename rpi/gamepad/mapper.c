#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <termios.h>
#include <signal.h>

#define I2C_DEVICE_ADDRESS  0x30
#define DATASIZE               9
#define POLL_US             8000   // 8 ms between I2C reads

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

// ---- I2C ----------------------------------------------------------------------

static int      i2c_fd       = -1;
static uint16_t last_buttons =  0;

static void init_i2c(void) {
    i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) { perror("Failed to open /dev/i2c-1"); exit(1); }
    if (ioctl(i2c_fd, I2C_SLAVE, I2C_DEVICE_ADDRESS) < 0) {
        perror("Failed to set I2C slave");
        close(i2c_fd);
        exit(1);
    }
    uint8_t probe;
    if (read(i2c_fd, &probe, 1) < 1) {
        fprintf(stderr, "No device at I2C address 0x%02X\n", I2C_DEVICE_ADDRESS);
        close(i2c_fd);
        exit(1);
    }
}

// Returns the current 16-bit button state, or the last good value on CRC error.
static uint16_t read_buttons(void) {
    uint8_t buf[DATASIZE];
    if (read(i2c_fd, buf, DATASIZE) != DATASIZE) return last_buttons;
    uint16_t computed = compute_crc16(buf, 7);
    uint16_t received = (uint16_t)buf[7] | ((uint16_t)buf[8] << 8);
    if (computed != received) return last_buttons;
    last_buttons = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    return last_buttons;
}

// ---- Terminal -----------------------------------------------------------------

static struct termios orig_termios;
static bool           raw_mode = false;

static void restore_terminal(void) {
    if (raw_mode) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        raw_mode = false;
    }
}

static void set_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios t = orig_termios;
    t.c_lflag &= ~(ICANON | ECHO);   // no line buffering, no echo; ISIG kept for Ctrl+C
    t.c_cc[VMIN]  = 0;               // non-blocking: read() returns immediately
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    raw_mode = true;
}

static void handle_signal(int sig) {
    (void)sig;
    restore_terminal();
    if (i2c_fd >= 0) close(i2c_fd);
    printf("\n\nInterrupted.\n");
    exit(1);
}

// ---- Mapping state ------------------------------------------------------------

static int8_t   bit_to_ps3[16];   // -1 = unmapped
static uint16_t mapped_bits = 0;  // bitmask of input bits already assigned

// ---- PS3 button table ---------------------------------------------------------
//
// Listed in a natural left-to-right, top-to-bottom controller order.
// The mapper walks through this list, prompting for each one.

typedef struct {
    int        index;   // PS3 button index 0-16
    char       ch;      // character used in the map string
    const char *name;   // human-readable label
} ButtonEntry;

static const ButtonEntry ps3_buttons[] = {
    { 13, 'D', "D-pad Up"         },
    { 14, 'E', "D-pad Down"       },
    { 15, 'F', "D-pad Left"       },
    { 16, 'G', "D-pad Right"      },
    {  0, '0', "Cross  / A"       },
    {  1, '1', "Circle / B"       },
    {  2, '2', "Triangle / Y"     },
    {  3, '3', "Square / X"       },
    {  4, '4', "L1"               },
    {  5, '5', "R1"               },
    {  6, '6', "L2"               },
    {  7, '7', "R2"               },
    {  8, '8', "Back / Select"    },
    {  9, '9', "Start"            },
    { 10, 'A', "Guide / PS"       },
    { 11, 'B', "L3 (left click)"  },
    { 12, 'C', "R3 (right click)" },
};
#define NUM_BUTTONS (int)(sizeof(ps3_buttons) / sizeof(ps3_buttons[0]))

// ---- Input helpers ------------------------------------------------------------

// Block until all currently-held unmapped bits are released, then pause briefly
// to avoid bleed-over from the previous step.
static void drain_presses(void) {
    while (read_buttons() & ~mapped_bits)
        usleep(POLL_US);
    usleep(50000);  // 50 ms debounce gap
}

// Block until either:
//   - A new (unmapped) bit goes high  -> returns bit index 0-15
//   - The user presses Enter          -> returns -1  (skip)
static int wait_for_press(void) {
    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0 && (c == '\n' || c == '\r'))
            return -1;

        uint16_t state = read_buttons();
        uint16_t fresh = state & ~mapped_bits;
        if (fresh) {
            for (int i = 0; i < 16; i++)
                if (fresh & (1 << i)) return i;
        }
        usleep(POLL_US);
    }
}

// Block until the given bit goes low.
static void wait_for_release(int bit) {
    while (read_buttons() & (1u << bit))
        usleep(POLL_US);
}

// Block until the user presses a key in `allowed` (case-insensitive lowercase string).
// Returns the raw character received.
static char wait_for_key(const char *allowed) {
    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            char lo = (c >= 'A' && c <= 'Z') ? c + 32 : c;
            for (const char *p = allowed; *p; p++)
                if (lo == *p) return c;
        }
        usleep(POLL_US);
    }
}

// ---- Per-button mapping loop --------------------------------------------------

static void map_button(int idx) {
    int        ps3_idx = ps3_buttons[idx].index;
    const char *name   = ps3_buttons[idx].name;

    while (1) {   // loops on redo
        printf("[%2d/%d] %-22s  (ENTER to skip)\n",
               idx + 1, NUM_BUTTONS, name);
        printf("        Press the button now...\n");
        fflush(stdout);

        // Wait for any leftover presses from the previous step to clear.
        drain_presses();

        int bit = wait_for_press();
        if (bit < 0) {
            printf("        Skipped.\n\n");
            return;
        }

        printf("        Detected bit %d. Release the button...", bit);
        fflush(stdout);
        wait_for_release(bit);
        printf(" ok.\n");

        printf("        Bit %d -> %s\n", bit, name);
        printf("        Keep? [y=yes  n=skip  r=redo]: ");
        fflush(stdout);

        char resp = wait_for_key("ynr\n\r");
        char lo   = (resp >= 'A' && resp <= 'Z') ? resp + 32 : resp;
        printf("\n");

        if (lo == 'y' || lo == '\n' || lo == '\r') {
            bit_to_ps3[bit] = (int8_t)ps3_idx;
            mapped_bits |= (1u << bit);
            printf("        Mapped.\n\n");
            return;
        }
        if (lo == 'n') {
            printf("        Skipped.\n\n");
            return;
        }
        // lo == 'r': fall through to top of loop
        printf("        Trying again...\n\n");
    }
}

// ---- Results ------------------------------------------------------------------

static char ps3_index_to_char(int idx) {
    if (idx >= 0  && idx <= 9)  return '0' + idx;
    if (idx >= 10 && idx <= 15) return 'A' + (idx - 10);
    if (idx == 16)              return 'G';
    return '-';
}

static void print_results(void) {
    char map[17];
    memset(map, '-', 16);
    map[16] = '\0';

    for (int bit = 0; bit < 16; bit++) {
        if (bit_to_ps3[bit] >= 0)
            map[bit] = ps3_index_to_char(bit_to_ps3[bit]);
    }

    printf("=== Mapping complete ===\n\n");

    // Show only the bits that were mapped.
    bool any = false;
    for (int bit = 0; bit < 16; bit++) {
        if (bit_to_ps3[bit] < 0) continue;
        if (!any) {
            printf("  %-6s  %-22s  %s\n", "Bit",  "Button",   "Char");
            printf("  %-6s  %-22s  %s\n", "---",  "------",   "----");
            any = true;
        }
        const char *bname = "?";
        for (int j = 0; j < NUM_BUTTONS; j++) {
            if (ps3_buttons[j].index == bit_to_ps3[bit]) {
                bname = ps3_buttons[j].name;
                break;
            }
        }
        printf("  bit %-2d  %-22s  %c\n", bit, bname, map[bit]);
    }

    if (!any) {
        printf("  (no buttons were mapped)\n");
    }

    printf("\nMap string: %s\n\n", map);
    printf("Run the driver with:\n");
    printf("  gamepad --map %s\n\n", map);
}

// ---- Main ---------------------------------------------------------------------

int main(void) {
    init_crc16_table();
    init_i2c();
    memset(bit_to_ps3, -1, sizeof(bit_to_ps3));

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    set_raw_mode();

    printf("\n");
    printf("Topper Gamepad Mapper\n");
    printf("=====================\n");
    printf("You will be asked to press each button on your controller.\n");
    printf("Press ENTER to skip any button that does not exist.\n");
    printf("Press Ctrl+C to quit at any time.\n\n");

    for (int i = 0; i < NUM_BUTTONS; i++)
        map_button(i);

    restore_terminal();
    print_results();

    close(i2c_fd);
    return 0;
}
