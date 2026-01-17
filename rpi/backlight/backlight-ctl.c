#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define FIFO_PATH "/run/backlight.fifo"

static void print_usage(const char *prog) {
    printf("Usage: %s [COMMAND] [ARGS]\n", prog);
    printf("\nCommands:\n");
    printf("  set <0-7>     Set brightness level\n");
    printf("  get           Get current brightness level\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    int fd = open(FIFO_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open FIFO (is daemon running?)");
        return EXIT_FAILURE;
    }

    char command[64];
    int len;

    if (strcmp(argv[1], "set") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: 'set' requires brightness value (0-7)\n");
            close(fd);
            return EXIT_FAILURE;
        }
        
        int level = atoi(argv[2]);
        if (level < 0 || level > 7) {
            fprintf(stderr, "Error: Brightness must be 0-7\n");
            close(fd);
            return EXIT_FAILURE;
        }
        
        len = snprintf(command, sizeof(command), "set %d", level);
    }
    else if (strcmp(argv[1], "get") == 0) {
        len = snprintf(command, sizeof(command), "get");
    }
    else {
        fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
        print_usage(argv[0]);
        close(fd);
        return EXIT_FAILURE;
    }

    if (write(fd, command, len) != len) {
        perror("Failed to write command");
        close(fd);
        return EXIT_FAILURE;
    }

    close(fd);
    return EXIT_SUCCESS;
}
