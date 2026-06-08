#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define I2C_DEVICE          "/dev/i2c-1"
#define ATMEGA_ADDR         0x30
#define I2C_CMD_GPIO_ALL    0x30
#define I2C_CMD_GPIO_READ   0x60
#define RESPONSE_LEN        9
#define NUM_PINS            16

// Response byte offsets from I2C_CMD_GPIO_READ
#define IDX_DDRB  0
#define IDX_DDRD  1
#define IDX_PORTB 2
#define IDX_PORTD 3
#define IDX_PINB  4
#define IDX_PIND  5

// ---- CRC ----------------------------------------------------------------

static uint16_t crc_table[256];

static void generate_crc_table(void) {
    const uint16_t poly = 0x1021;
    for (int i = 0; i < 256; i++) {
        uint16_t crc = (uint16_t)i << 8;
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? (crc << 1) ^ poly : crc << 1;
        crc_table[i] = crc;
    }
}

static uint16_t calculate_crc(const uint8_t *data, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++)
        crc = (crc << 8) ^ crc_table[(crc >> 8) ^ data[i]];
    return crc;
}

// ---- I2C ----------------------------------------------------------------

static int open_i2c(void) {
    int fd = open(I2C_DEVICE, O_RDWR);
    if (fd < 0) { perror("open"); return -1; }
    if (ioctl(fd, I2C_SLAVE, ATMEGA_ADDR) < 0) { perror("ioctl"); close(fd); return -1; }
    return fd;
}

static int read_state(int fd, uint8_t buf[RESPONSE_LEN]) {
    uint8_t cmd = I2C_CMD_GPIO_READ;
    if (write(fd, &cmd, 1) != 1) { perror("write"); return -1; }

    // Give the ATmega main loop time to process the command before we read.
    usleep(5000);

    if (read(fd, buf, RESPONSE_LEN) != RESPONSE_LEN) { perror("read"); return -1; }

    uint16_t expected = calculate_crc(buf, 7);
    uint16_t received = ((uint16_t)buf[7] << 8) | buf[8];
    if (expected != received) {
        fprintf(stderr, "CRC mismatch: expected 0x%04X, got 0x%04X\n", expected, received);
        return -1;
    }
    return 0;
}

static int write_state(int fd, uint8_t ddrb, uint8_t ddrd, uint8_t portb, uint8_t portd) {
    uint8_t buf[5] = { I2C_CMD_GPIO_ALL, ddrb, ddrd, portb, portd };
    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) { perror("write"); return -1; }
    return 0;
}

// ---- Pin display --------------------------------------------------------

static void print_pin(int pin, const uint8_t buf[RESPONSE_LEN]) {
    int     port = pin / 8;
    uint8_t bit  = pin % 8;
    uint8_t mask = 1 << bit;

    uint8_t ddr      = (port == 0) ? buf[IDX_DDRB]  : buf[IDX_DDRD];
    uint8_t port_reg = (port == 0) ? buf[IDX_PORTB] : buf[IDX_PORTD];
    uint8_t pin_reg  = (port == 0) ? buf[IDX_PINB]  : buf[IDX_PIND];

    int is_output = (ddr      & mask) != 0;
    int level     = (pin_reg  & mask) != 0;
    int pull_up   = (port_reg & mask) != 0;

    if (is_output) {
        printf("GPIO %d: FSEL=1 FUNC=OUTPUT LEVEL=%d\n", pin, level);
    } else {
        printf("GPIO %d: FSEL=0 FUNC=INPUT LEVEL=%d PULL=%s\n",
               pin, level, pull_up ? "UP" : "NONE");
    }
}

// ---- Pin list parsing ---------------------------------------------------
// Parses a comma-separated list of pins and/or ranges, e.g. "0,3,7-10,15".
// Calls cb(pin, userdata) for each resolved pin in order.

static int parse_pins(const char *arg, void (*cb)(int, void *), void *userdata) {
    char buf[64];
    strncpy(buf, arg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *token = strtok(buf, ",");
    while (token) {
        char *dash = strchr(token, '-');
        int start, end;
        if (dash) {
            start = atoi(token);
            end   = atoi(dash + 1);
        } else {
            start = end = atoi(token);
        }
        if (start < 0 || end >= NUM_PINS || start > end) {
            fprintf(stderr, "Invalid pin '%s' (valid range: 0-%d)\n", token, NUM_PINS - 1);
            return -1;
        }
        for (int p = start; p <= end; p++)
            cb(p, userdata);
        token = strtok(NULL, ",");
    }
    return 0;
}

// ---- get ----------------------------------------------------------------

static void print_pin_cb(int pin, void *userdata) {
    print_pin(pin, (const uint8_t *)userdata);
}

static int cmd_get(int fd, int argc, char *argv[], int optind) {
    uint8_t buf[RESPONSE_LEN];
    if (read_state(fd, buf) < 0) return -1;

    if (optind >= argc) {
        // No pin argument: print all
        for (int p = 0; p < NUM_PINS; p++)
            print_pin(p, buf);
    } else {
        if (parse_pins(argv[optind], print_pin_cb, buf) < 0) return -1;
    }
    return 0;
}

// ---- set ----------------------------------------------------------------

struct set_context {
    uint8_t *buf;
    int      argc;
    char   **argv;
    int      optind;
    int      error;
};

static void apply_options_to_pin(int pin, void *userdata) {
    struct set_context *ctx = (struct set_context *)userdata;
    if (ctx->error) return;

    int     port = pin / 8;
    uint8_t bit  = pin % 8;
    uint8_t mask = 1 << bit;

    uint8_t *ddr      = (port == 0) ? &ctx->buf[IDX_DDRB]  : &ctx->buf[IDX_DDRD];
    uint8_t *port_reg = (port == 0) ? &ctx->buf[IDX_PORTB] : &ctx->buf[IDX_PORTD];

    for (int i = ctx->optind; i < ctx->argc; i++) {
        const char *opt = ctx->argv[i];
        if      (strcmp(opt, "ip") == 0) { *ddr      &= ~mask; }
        else if (strcmp(opt, "op") == 0) { *ddr      |=  mask; }
        else if (strcmp(opt, "pu") == 0) { *port_reg |=  mask; }
        else if (strcmp(opt, "pn") == 0) { *port_reg &= ~mask; }
        else if (strcmp(opt, "dh") == 0) { *port_reg |=  mask; }
        else if (strcmp(opt, "dl") == 0) { *port_reg &= ~mask; }
        else if (strcmp(opt, "pd") == 0) {
            fprintf(stderr, "Warning: pull-down not supported on ATmega8; ignoring 'pd'\n");
        } else {
            fprintf(stderr, "Unknown option '%s'\n", opt);
            ctx->error = 1;
            return;
        }
    }
}

static int cmd_set(int fd, const char *pin_spec, int argc, char *argv[], int optind) {
    if (optind >= argc) {
        fprintf(stderr, "set requires at least one option\n");
        return -1;
    }

    uint8_t buf[RESPONSE_LEN];
    if (read_state(fd, buf) < 0) return -1;

    struct set_context ctx = { buf, argc, argv, optind, 0 };
    if (parse_pins(pin_spec, apply_options_to_pin, &ctx) < 0) return -1;
    if (ctx.error) return -1;

    return write_state(fd, buf[IDX_DDRB], buf[IDX_DDRD], buf[IDX_PORTB], buf[IDX_PORTD]);
}

// ---- Usage --------------------------------------------------------------

static void usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s get [<pin>|<pin,pin,...>|<start>-<end>]\n", prog);
    printf("  %s set <pin[,pin,...]|start-end> <options>...\n\n", prog);
    printf("  pin: 0-%d (0-7 = PORTB bit 0-7, 8-15 = PORTD bit 0-7)\n\n", NUM_PINS - 1);
    printf("Valid [options] for %s set are:\n", prog);
    printf("  ip      set GPIO as input\n");
    printf("  op      set GPIO as output\n");
    printf("  pu      set GPIO in-pad pull up\n");
    printf("  pd      set GPIO in-pad pull down (not supported on ATmega8)\n");
    printf("  pn      set GPIO pull none (no pull)\n");
    printf("  dh      set GPIO to drive high (1) level (only valid if set to be an output)\n");
    printf("  dl      set GPIO to drive low (0) level (only valid if set to be an output)\n");
    printf("Examples:\n");
    printf("  %s get              Prints state of all GPIOs one per line\n", prog);
    printf("  %s get 5            Prints state of GPIO 5\n", prog);
    printf("  %s get 0,3,7        Prints state of GPIOs 0, 3, and 7\n", prog);
    printf("  %s get 0-7          Prints state of GPIOs 0 through 7\n", prog);
    printf("  %s set 5 op         Set GPIO 5 to be an output\n", prog);
    printf("  %s set 5 dh         Set GPIO 5 to drive high\n", prog);
    printf("  %s set 5 op dh      Set GPIO 5 as output driving high\n", prog);
    printf("  %s set 5 ip pu      Set GPIO 5 as input with pull-up\n", prog);
    printf("  %s set 5 ip pn      Set GPIO 5 as floating input\n", prog);
    printf("  %s set 0-15 ip pn   Set all GPIOs as floating inputs\n", prog);
}

// ---- Main ---------------------------------------------------------------

int main(int argc, char *argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    if (strcmp(argv[1], "help") == 0) { usage(argv[0]); return 0; }

    generate_crc_table();

    int fd = open_i2c();
    if (fd < 0) return 1;

    int result = 0;
    const char *cmd = argv[1];

    if (strcmp(cmd, "get") == 0) {
        result = cmd_get(fd, argc, argv, 2);

    } else if (strcmp(cmd, "set") == 0) {
        if (argc < 4) {
            fprintf(stderr, "set requires a pin number and at least one option\n");
            close(fd); return 1;
        }
        result = cmd_set(fd, argv[2], argc, argv, 3);

    } else {
        fprintf(stderr, "Unknown command '%s'\n\n", cmd);
        usage(argv[0]);
        result = -1;
    }

    close(fd);
    return result < 0 ? 1 : 0;
}
