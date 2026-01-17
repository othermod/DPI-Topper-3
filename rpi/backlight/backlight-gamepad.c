#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <stdint.h>

#define I2C_CMD_BRIGHT 0x10
#define I2C_BRIGHT_DISABLE 8
#define I2C_BRIGHT_ENABLE 9

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s [set <0-7>|get|on|off]\n", argv[0]);
        return EXIT_FAILURE;
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

    if (!strcmp(argv[1], "set")) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'set' requires a brightness value (0-7)\n");
            close(i2c_fd);
            return EXIT_FAILURE;
        }

        int value = atoi(argv[2]);
        if (value < 0 || value > 7) {
            fprintf(stderr, "Error: Brightness must be between 0-7\n");
            close(i2c_fd);
            return EXIT_FAILURE;
        }

        uint8_t cmd[2] = {I2C_CMD_BRIGHT, (uint8_t)value};
        if (write(i2c_fd, cmd, sizeof(cmd)) != sizeof(cmd)) {
            perror("Brightness set failed");
            close(i2c_fd);
            return EXIT_FAILURE;
        }

        printf("Brightness set to %d\n", value);
    }
    else if (!strcmp(argv[1], "get")) {
        uint8_t buf[9];
        if (read(i2c_fd, buf, sizeof(buf)) != sizeof(buf)) {
            perror("Brightness read failed");
            close(i2c_fd);
            return EXIT_FAILURE;
        }

        int brightness = buf[6] & 0x07;
        printf("%d\n", brightness);
    }
    else if (!strcmp(argv[1], "off")) {
        uint8_t cmd[2] = {I2C_CMD_BRIGHT, I2C_BRIGHT_DISABLE};
        if (write(i2c_fd, cmd, sizeof(cmd)) != sizeof(cmd)) {
            perror("Display off failed");
            close(i2c_fd);
            return EXIT_FAILURE;
        }

        printf("Display off\n");
    }
    else if (!strcmp(argv[1], "on")) {
        uint8_t cmd[2] = {I2C_CMD_BRIGHT, I2C_BRIGHT_ENABLE};
        if (write(i2c_fd, cmd, sizeof(cmd)) != sizeof(cmd)) {
            perror("Display on failed");
            close(i2c_fd);
            return EXIT_FAILURE;
        }

        printf("Display on\n");
    }
    else {
        fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
        fprintf(stderr, "Usage: %s [set <0-7>|get|on|off]\n", argv[0]);
        close(i2c_fd);
        return EXIT_FAILURE;
    }

    close(i2c_fd);
    return EXIT_SUCCESS;
}
