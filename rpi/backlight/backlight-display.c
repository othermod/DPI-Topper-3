#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <time.h>
#include <stdbool.h>
#include <sched.h>
#include <errno.h>

#define T_START 10
#define T_EOS 10
#define T_H_LB 10
#define T_H_HB 25
#define T_L_LB 25
#define T_L_HB 10
#define T_OFF 3000

#define LCD_ADDR 0x72
#define GPIO_PIN 4
#define BUTTON_PIN 20
#define FIFO_PATH "/run/backlight.fifo"

#define RP1_GPIO_STATUS_OFFSET(pin) ((pin) * 2)
#define RP1_GPIO_CTRL_OFFSET(pin) ((pin) * 2 + 1)
#define RP1_GPIO_CTRL_FUNCSEL_SIO 5
#define RP1_GPIO_CTRL_OEOVER_ENABLE 3
#define RP1_GPIO_CTRL_OUTOVER_HIGH 3
#define RP1_GPIO_CTRL_OUTOVER_LOW 2
#define RP1_GPIO_STATUS_INFROM_PAD 17

#define BCM_GPFSEL0 0
#define BCM_GPSET0 7
#define BCM_GPCLR0 10
#define BCM_GPLEV0 13
#define BCM_GPPUD 37
#define BCM_GPPUDCLK0 38

static volatile uint32_t *gpio_map = NULL;
static int mem_fd = -1;
static int is_pi5 = 0;

static uint8_t current_brightness = 4;
static bool display_on = true;
static bool button_pressed = false;
static int fifo_fd = -1;

static void delay_us(uint32_t us) {
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

static int gpio_init_pi5(void) {
    mem_fd = open("/dev/gpiomem0", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
        if (mem_fd < 0) {
            perror("Failed to open /dev/gpiomem0 or /dev/mem");
            return -1;
        }
        void *map = mmap(NULL, 64 * 1024 * 1024, PROT_READ | PROT_WRITE,
                        MAP_SHARED, mem_fd, 0x1f00000000);
        if (map == MAP_FAILED) {
            perror("mmap failed");
            close(mem_fd);
            return -1;
        }
        gpio_map = (volatile uint32_t *)((uint8_t *)map + 0xd0000);
    } else {
        void *map = mmap(NULL, 0x30000, PROT_READ | PROT_WRITE,
                        MAP_SHARED, mem_fd, 0);
        if (map == MAP_FAILED) {
            perror("mmap failed");
            close(mem_fd);
            return -1;
        }
        gpio_map = (volatile uint32_t *)map;
    }
    return 0;
}

static int gpio_init_legacy(void) {
    mem_fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("Failed to open /dev/gpiomem");
        return -1;
    }
    void *map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap failed");
        close(mem_fd);
        return -1;
    }
    gpio_map = (volatile uint32_t *)map;
    return 0;
}

static int gpio_init(void) {
    FILE *f = fopen("/proc/device-tree/model", "r");
    if (f) {
        char model[256];
        if (fgets(model, sizeof(model), f)) {
            if (strstr(model, "Pi 5") || strstr(model, "Raspberry Pi 5")) {
                is_pi5 = 1;
            }
        }
        fclose(f);
    }
    return is_pi5 ? gpio_init_pi5() : gpio_init_legacy();
}

static void gpio_cleanup(void) {
    if (gpio_map) {
        munmap((void *)gpio_map, is_pi5 ? 0x30000 : 4096);
        gpio_map = NULL;
    }
    if (mem_fd >= 0) {
        close(mem_fd);
        mem_fd = -1;
    }
}

static void gpio_set_output_pi5(uint8_t pin) {
    volatile uint32_t *ctrl = &gpio_map[RP1_GPIO_CTRL_OFFSET(pin)];
    uint32_t val = *ctrl;
    val = (val & ~0x1f) | RP1_GPIO_CTRL_FUNCSEL_SIO;
    val = (val & ~0xc000) | (RP1_GPIO_CTRL_OEOVER_ENABLE << 14);
    *ctrl = val;
}

static void gpio_set_output_legacy(uint8_t pin) {
    uint32_t reg = pin / 10;
    uint32_t shift = (pin % 10) * 3;
    gpio_map[BCM_GPFSEL0 + reg] &= ~(0b111 << shift);
    gpio_map[BCM_GPFSEL0 + reg] |= (0b001 << shift);
}

static void gpio_set_output(uint8_t pin) {
    is_pi5 ? gpio_set_output_pi5(pin) : gpio_set_output_legacy(pin);
}

static void gpio_set_input_pi5(uint8_t pin) {
    volatile uint32_t *ctrl = &gpio_map[RP1_GPIO_CTRL_OFFSET(pin)];
    uint32_t val = *ctrl;
    val = (val & ~0xc000) | (0 << 14);
    *ctrl = val;
}

static void gpio_set_input_legacy(uint8_t pin) {
    uint32_t reg = pin / 10;
    uint32_t shift = (pin % 10) * 3;
    gpio_map[BCM_GPFSEL0 + reg] &= ~(0b111 << shift);
}

static void gpio_set_input(uint8_t pin) {
    is_pi5 ? gpio_set_input_pi5(pin) : gpio_set_input_legacy(pin);
}

static void gpio_set_pullup_pi5(uint8_t pin) {
    (void)pin;
}

static void gpio_set_pullup_legacy(uint8_t pin) {
    gpio_map[BCM_GPPUD] = 0x00000002;
    delay_us(10);
    uint32_t reg = pin / 32;
    uint32_t shift = pin % 32;
    gpio_map[BCM_GPPUDCLK0 + reg] = (1 << shift);
    delay_us(10);
    gpio_map[BCM_GPPUD] = 0;
    gpio_map[BCM_GPPUDCLK0 + reg] = 0;
}

static void gpio_set_pullup(uint8_t pin) {
    is_pi5 ? gpio_set_pullup_pi5(pin) : gpio_set_pullup_legacy(pin);
}

static uint8_t gpio_read_pi5(uint8_t pin) {
    volatile uint32_t status = gpio_map[RP1_GPIO_STATUS_OFFSET(pin)];
    return (status >> RP1_GPIO_STATUS_INFROM_PAD) & 1;
}

static uint8_t gpio_read_legacy(uint8_t pin) {
    uint32_t reg = pin / 32;
    uint32_t shift = pin % 32;
    uint32_t level = gpio_map[BCM_GPLEV0 + reg];
    return (level >> shift) & 1;
}

static uint8_t gpio_read(uint8_t pin) {
    return is_pi5 ? gpio_read_pi5(pin) : gpio_read_legacy(pin);
}

static inline void gpio_set_high(uint8_t pin) {
    if (is_pi5) {
        volatile uint32_t *ctrl = &gpio_map[RP1_GPIO_CTRL_OFFSET(pin)];
        uint32_t val = *ctrl;
        val = (val & ~0x3000) | (RP1_GPIO_CTRL_OUTOVER_HIGH << 12);
        *ctrl = val;
    } else {
        gpio_map[BCM_GPSET0] = (1 << pin);
    }
}

static inline void gpio_set_low(uint8_t pin) {
    if (is_pi5) {
        volatile uint32_t *ctrl = &gpio_map[RP1_GPIO_CTRL_OFFSET(pin)];
        uint32_t val = *ctrl;
        val = (val & ~0x3000) | (RP1_GPIO_CTRL_OUTOVER_LOW << 12);
        *ctrl = val;
    } else {
        gpio_map[BCM_GPCLR0] = (1 << pin);
    }
}

static void send_byte(uint8_t byte) {
    delay_us(T_START);
    for (int i = 7; i >= 0; i--) {
        int bit = (byte >> i) & 1;
        gpio_set_low(GPIO_PIN);
        delay_us(bit ? T_L_HB : T_L_LB);
        gpio_set_high(GPIO_PIN);
        delay_us(bit ? T_H_HB : T_H_LB);
    }
    gpio_set_low(GPIO_PIN);
    delay_us(T_EOS);
    gpio_set_high(GPIO_PIN);
}

static void init_chip(void) {
    gpio_set_low(GPIO_PIN);
    delay_us(T_OFF);
    gpio_set_high(GPIO_PIN);
    delay_us(150);
    gpio_set_low(GPIO_PIN);
    delay_us(300);
    gpio_set_high(GPIO_PIN);
}

static void set_brightness(uint8_t level) {
    if (level > 7) {
        return;
    }
    current_brightness = level;
    display_on = true;
    
    uint8_t bytes_to_send[] = {LCD_ADDR, level * 4 + 1};
    
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    sched_setscheduler(0, SCHED_FIFO, &param);
    
    for (int byte = 0; byte < 2; byte++) {
        send_byte(bytes_to_send[byte]);
    }
    
    param.sched_priority = 0;
    sched_setscheduler(0, SCHED_OTHER, &param);
}

static void handle_button_press(void) {
    if (!display_on) {
        set_brightness(current_brightness);
    } else {
        current_brightness = (current_brightness + 1) % 8;
        set_brightness(current_brightness);
    }
}

static void check_button(void) {
    uint8_t button_level = gpio_read(BUTTON_PIN);
    
    if (!button_level && !button_pressed) {
        button_pressed = true;
        handle_button_press();
    } else if (button_level && button_pressed) {
        button_pressed = false;
    }
}

static void process_command(const char *cmd) {
    char command[64];
    int value;
    
    if (sscanf(cmd, "%63s %d", command, &value) >= 1) {
        if (strcmp(command, "set") == 0) {
            if (value >= 0 && value <= 7) {
                set_brightness(value);
            }
        } else if (strcmp(command, "get") == 0) {
            printf("%d\n", current_brightness);
            fflush(stdout);
        }
    }
}

static int fifo_init(void) {
    unlink(FIFO_PATH);
    
    if (mkfifo(FIFO_PATH, 0666) < 0) {
        perror("Failed to create FIFO");
        return -1;
    }
    
    fifo_fd = open(FIFO_PATH, O_RDWR | O_NONBLOCK);
    if (fifo_fd < 0) {
        perror("Failed to open FIFO");
        unlink(FIFO_PATH);
        return -1;
    }
    
    return 0;
}

static void fifo_cleanup(void) {
    if (fifo_fd >= 0) {
        close(fifo_fd);
        fifo_fd = -1;
    }
    unlink(FIFO_PATH);
}

int main(int argc, char *argv[]) {
    if (gpio_init() < 0) {
        return EXIT_FAILURE;
    }

    gpio_set_output(GPIO_PIN);
    gpio_set_low(GPIO_PIN);
    gpio_set_input(BUTTON_PIN);
    gpio_set_pullup(BUTTON_PIN);

    if (fifo_init() < 0) {
        gpio_cleanup();
        return EXIT_FAILURE;
    }

    init_chip();
    delay_us(1000);
    set_brightness(current_brightness);

    fd_set read_fds;
    struct timeval timeout;
    char buffer[64];

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(fifo_fd, &read_fds);
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;
        
        int ret = select(fifo_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ret > 0 && FD_ISSET(fifo_fd, &read_fds)) {
            ssize_t n = read(fifo_fd, buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                process_command(buffer);
            }
        }
        
        check_button();
    }

    fifo_cleanup();
    gpio_cleanup();
    return EXIT_SUCCESS;
}
