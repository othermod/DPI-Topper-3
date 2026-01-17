#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GAMEPAD_DATA_SIZE 9
#define I2C_CMD_CRC 0x20
#define TIMEOUT_MS 5000
#define POLL_INTERVAL_MS 50
#define BUTTON_SKIP 17

static const char* button_names[17] = {
    "SELECT (back)",
    "L3 (left stick button)",
    "R3 (right stick button)",
    "START",
    "D-PAD UP",
    "D-PAD RIGHT",
    "D-PAD DOWN",
    "D-PAD LEFT",
    "L2 (left trigger)",
    "R2 (right trigger)",
    "L1 (left shoulder)",
    "R1 (right shoulder)",
    "TRIANGLE (North)",
    "CIRCLE (East)",
    "CROSS (South)",
    "SQUARE (West)",
    "GUIDE/PS button"
};

static uint16_t read_buttons(int i2c_fd) {
    uint8_t buf[GAMEPAD_DATA_SIZE];

    if (read(i2c_fd, buf, sizeof(buf)) != sizeof(buf)) {
        return 0xFFFF; // Error value
    }

    return buf[0] | (buf[1] << 8);
}

static int detect_pressed_bit(uint16_t prev, uint16_t current) {
    uint16_t pressed = current & ~prev;

    if (pressed == 0) {
        return -1;
    }

    // Find first set bit
    for (int bit = 0; bit < 16; bit++) {
        if (pressed & (1 << bit)) {
            return bit;
        }
    }

    return -1;
}

int main(void) {
    uint8_t button_map[16];
    bool mapped[16] = {false};

    printf("===========================================\n");
    printf("PS3 Controller Button Mapping Configuration\n");
    printf("===========================================\n\n");

    // Open I2C
    int i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) {
        perror("Failed to open I2C device");
        printf("Make sure you have permissions: sudo chmod 666 /dev/i2c-1\n");
        return EXIT_FAILURE;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, 0x30) < 0) {
        perror("Failed to set I2C address");
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    // Disable CRC for simplicity
    uint8_t crc_cmd[2] = {I2C_CMD_CRC, 0};
    if (write(i2c_fd, crc_cmd, sizeof(crc_cmd)) != sizeof(crc_cmd)) {
        perror("Failed to configure CRC");
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    printf("Configuration will now begin.\n");
    printf("You will be asked to press each button in sequence.\n");
    printf("Make sure NO buttons are currently pressed.\n\n");
    printf("Press ENTER to continue...");
    getchar();
    printf("\n");

    // Get baseline (no buttons pressed)
    uint16_t baseline = read_buttons(i2c_fd);
    if (baseline == 0xFFFF) {
        fprintf(stderr, "Failed to read initial button state\n");
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    // Configure each button
    for (int sdl_button = 0; sdl_button < 17; sdl_button++) {
        printf("Press and hold: %s (or press ENTER to skip)\n", button_names[sdl_button]);
        fflush(stdout);

        // Set stdin to non-blocking
        int stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);

        int detected_bit = -1;
        bool skipped = false;

        // Wait for button press or skip command - no timeout
        while (true) {
            usleep(10000);  // 10ms poll for immediate response

            // Check for skip command (just ENTER)
            char skip_input[10];
            if (fgets(skip_input, sizeof(skip_input), stdin) != NULL) {
                if (skip_input[0] == '\n') {
                    printf("Skipped %s\n\n", button_names[sdl_button]);
                    skipped = true;
                    break;
                }
            }

            uint16_t current = read_buttons(i2c_fd);

            if (current == 0xFFFF) {
                continue;
            }

            detected_bit = detect_pressed_bit(baseline, current);

            if (detected_bit >= 0) {
                // Check if this bit was already mapped
                if (mapped[detected_bit]) {
                    printf("Error: This button was already used for %s! Press a different button.\n",
                           button_names[button_map[detected_bit]]);
                    // Wait for button release before continuing
                    while (read_buttons(i2c_fd) != baseline) {
                        usleep(10000);
                    }
                    detected_bit = -1;
                    continue;
                }

                printf("%s button detected at position %d - keep holding...", button_names[sdl_button], detected_bit);
                fflush(stdout);

                // Verify button is held for 3 seconds
                bool held = true;
                for (int i = 0; i < 300; i++) {  // 300 * 10ms = 3000ms (3 seconds)
                    usleep(10000);
                    current = read_buttons(i2c_fd);

                    if (current == 0xFFFF || !(current & (1 << detected_bit))) {
                        printf(" released too soon!\n");
                        held = false;
                        detected_bit = -1;
                        break;
                    }
                }

                if (held) {
                    printf(" confirmed!\n");
                    button_map[detected_bit] = sdl_button;
                    mapped[detected_bit] = true;
                    break;
                }
            }
        }

        // Restore stdin to blocking
        fcntl(STDIN_FILENO, F_SETFL, stdin_flags);

        if (skipped) {
            continue;  // Move to next button
        }

        // Wait for button release
        printf("Now release the button...");
        fflush(stdout);

        int release_timeout = TIMEOUT_MS / 10;
        while (release_timeout > 0) {
            usleep(10000);
            uint16_t current = read_buttons(i2c_fd);

            if (current == 0xFFFF) {
                continue;
            }

            if (current == baseline) {
                printf(" released!\n\n");
                break;
            }

            release_timeout--;
        }

        if (release_timeout == 0) {
            printf(" (timeout - continuing anyway)\n\n");
        }
    }

    close(i2c_fd);

    // Check if all buttons were mapped
    for (int i = 0; i < 16; i++) {
        if (!mapped[i]) {
            button_map[i] = BUTTON_SKIP;
        }
    }

    // Generate output
    printf("\n===========================================\n");
    printf("Configuration Complete!\n");
    printf("===========================================\n\n");

    printf("Button mapping hex string:\n");
    for (int i = 0; i < 16; i++) {
        if (button_map[i] == BUTTON_SKIP) {
            printf("x");
        } else if (button_map[i] == 16) {
            printf("g");
        } else {
            printf("%x", button_map[i]);
        }
    }
    printf("\n\n");

    printf("Use this command to run the gamepad driver:\n");
    printf("./gamepad --button-map ");
    for (int i = 0; i < 16; i++) {
        if (button_map[i] == BUTTON_SKIP) {
            printf("x");
        } else if (button_map[i] == 16) {
            printf("g");
        } else {
            printf("%x", button_map[i]);
        }
    }
    printf("\n\n");

    // Show detailed mapping
    printf("Detailed mapping:\n");
    printf("Position -> SDL Button (Function)\n");
    printf("-----------------------------------\n");
    for (int i = 0; i < 16; i++) {
        if (mapped[i]) {
            printf("   %2d   ->   %2d      (%s)\n",
                   i, button_map[i], button_names[button_map[i]]);
        } else {
            printf("   %2d   ->  SKIP     (no button)\n", i);
        }
    }
    printf("\n");

    return EXIT_SUCCESS;
}
