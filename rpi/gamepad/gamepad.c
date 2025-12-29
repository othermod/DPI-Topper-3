#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/uinput.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>

#define GAMEPAD_DATA_SIZE 9
#define MAX_READ_RETRIES 3
#define I2C_CMD_CRC 0x20
#define BUTTON_SKIP 17

static uint16_t crc_table[256];
static volatile bool running = true;

static void signal_handler(int sig) {
    (void)sig;
    running = false;
}

static void generate_crc_table(void) {
    const uint16_t poly = 0x1021;

    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ poly;
            } else {
                crc = crc << 1;
            }
        }
        crc_table[i] = crc;
    }
}

static uint16_t calculate_crc(const uint8_t *data) {
    uint16_t crc = 0xFFFF;

    for (int i = 0; i < 7; i++) {
        uint8_t index = (crc >> 8) ^ data[i];
        crc = (crc << 8) ^ crc_table[index];
    }

    return crc;
}

static bool verify_crc(const uint8_t *buf) {
    uint16_t received_crc = buf[7] | (buf[8] << 8);
    uint16_t calculated_crc = calculate_crc(buf);

    return received_crc == calculated_crc;
}

int main(int argc, char *argv[]) {
    int poll_interval_ms = 10;
    int axis_min = 0;
    int axis_max = 255;
    int axis_fuzz = 0;
    int axis_flat = 0;
    bool crc_enabled = true;
    uint8_t button_map[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--poll-interval")) poll_interval_ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--axis-min")) axis_min = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--axis-max")) axis_max = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--axis-fuzz")) axis_fuzz = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--axis-flat")) axis_flat = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--disable-crc")) crc_enabled = false;
        else if (!strcmp(argv[i], "--button-map")) {
            char *map_str = argv[++i];
            if (strlen(map_str) != 16) {
                fprintf(stderr, "Error: --button-map requires exactly 16 hex digits\n");
                return EXIT_FAILURE;
            }
            for (int j = 0; j < 16; j++) {
                char c = map_str[j];
                uint8_t val;
                if (c >= '0' && c <= '9') val = c - '0';
                else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
                else if (c == 'g' || c == 'G') val = 16;
                else if (c == 'x' || c == 'X') val = BUTTON_SKIP;
                else {
                    fprintf(stderr, "Error: --button-map contains invalid character '%c'\n", c);
                    return EXIT_FAILURE;
                }
                if (val > 16 && val != BUTTON_SKIP) {
                    fprintf(stderr, "Error: --button-map values must be 0-F, G, or X\n");
                    return EXIT_FAILURE;
                }
                button_map[j] = val;
            }
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (crc_enabled) {
        generate_crc_table();
    }

    int i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) {
        perror("I2C open failed");
        return EXIT_FAILURE;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, 0x30) < 0) {
        perror("I2C address set failed");
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    uint8_t crc_cmd[2] = {I2C_CMD_CRC, crc_enabled ? 1 : 0};
    if (write(i2c_fd, crc_cmd, sizeof(crc_cmd)) != sizeof(crc_cmd)) {
        perror("CRC config failed");
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    int uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd < 0) {
        perror("Uinput open failed");
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(uinput_fd, UI_SET_EVBIT, EV_ABS);

    for (int i = BTN_TRIGGER_HAPPY1; i <= BTN_TRIGGER_HAPPY17; i++) {
        ioctl(uinput_fd, UI_SET_KEYBIT, i);
    }

    struct uinput_abs_setup abs_setup;

    abs_setup.code = ABS_X;
    abs_setup.absinfo.minimum = axis_min;
    abs_setup.absinfo.maximum = axis_max;
    abs_setup.absinfo.fuzz = axis_fuzz;
    abs_setup.absinfo.flat = axis_flat;
    ioctl(uinput_fd, UI_ABS_SETUP, &abs_setup);

    abs_setup.code = ABS_Y;
    ioctl(uinput_fd, UI_ABS_SETUP, &abs_setup);

    abs_setup.code = ABS_RX;
    ioctl(uinput_fd, UI_ABS_SETUP, &abs_setup);

    abs_setup.code = ABS_RY;
    ioctl(uinput_fd, UI_ABS_SETUP, &abs_setup);

    struct uinput_setup usetup = {0};
    snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "PS3 Controller");
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x054c;
    usetup.id.product = 0x0268;
    usetup.id.version = 0x0110;

    if (ioctl(uinput_fd, UI_DEV_SETUP, &usetup) < 0 ||
        ioctl(uinput_fd, UI_DEV_CREATE) < 0) {
        perror("Uinput setup failed");
    close(i2c_fd);
    close(uinput_fd);
    return EXIT_FAILURE;
        }

        uint8_t buf[GAMEPAD_DATA_SIZE];
        uint16_t prev_buttons = 0;

        while (running) {
            usleep(poll_interval_ms * 1000);

            int retries = 0;
            bool read_success = false;

            while (retries < MAX_READ_RETRIES) {
                if (read(i2c_fd, buf, sizeof(buf)) != sizeof(buf)) {
                    retries++;
                    usleep(100);
                    continue;
                }

                if (crc_enabled && !verify_crc(buf)) {
                    retries++;
                    usleep(100);
                    continue;
                }

                read_success = true;
                break;
            }

            if (!read_success) {
                continue;
            }

            uint16_t buttons = buf[0] | (buf[1] << 8);
            uint8_t joy1_x = buf[2];
            uint8_t joy1_y = buf[3];
            uint8_t joy2_x = buf[4];
            uint8_t joy2_y = buf[5];

            struct input_event events[21];
            int event_count = 0;

            uint16_t changed = buttons ^ prev_buttons;
            for (int bit = 0; bit < 16; bit++) {
                if (changed & (1 << bit)) {
                    if (button_map[bit] == BUTTON_SKIP) continue;
                    events[event_count].type = EV_KEY;
                    events[event_count].code = BTN_TRIGGER_HAPPY1 + button_map[bit];
                    events[event_count].value = (buttons >> bit) & 1;
                    event_count++;
                }
            }

            events[event_count].type = EV_ABS;
            events[event_count].code = ABS_X;
            events[event_count].value = joy1_x;
            event_count++;

            events[event_count].type = EV_ABS;
            events[event_count].code = ABS_Y;
            events[event_count].value = joy1_y;
            event_count++;

            events[event_count].type = EV_ABS;
            events[event_count].code = ABS_RX;
            events[event_count].value = joy2_x;
            event_count++;

            events[event_count].type = EV_ABS;
            events[event_count].code = ABS_RY;
            events[event_count].value = joy2_y;
            event_count++;

            events[event_count].type = EV_SYN;
            events[event_count].code = SYN_REPORT;
            events[event_count].value = 0;
            event_count++;

            if (write(uinput_fd, events, sizeof(struct input_event) * event_count) < 0) {
                perror("Event write failed");
            }

            prev_buttons = buttons;
        }

        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
        close(i2c_fd);

        return EXIT_SUCCESS;
}
